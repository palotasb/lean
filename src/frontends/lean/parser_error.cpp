/*
Copyright (c) 2013 Microsoft Corporation. All rights reserved.
Released under Apache 2.0 license as described in the file LICENSE.

Author: Leonardo de Moura
*/
#include <utility>
#include "kernel/kernel_exception.h"
#include "frontends/lean/parser_imp.h"

namespace lean {
void parser_imp::display_error_pos(unsigned line, unsigned pos) {
    regular(m_io_state) << "Error (line: " << line;
    if (pos != static_cast<unsigned>(-1))
        regular(m_io_state) << ", pos: " << pos;
    regular(m_io_state) << ")";
}

void parser_imp::display_error_pos(pos_info const & p) { display_error_pos(p.first, p.second); }

void parser_imp::display_error_pos(optional<expr> const & e) {
    if (e) {
        auto it = m_expr_pos_info.find(*e);
        if (it == m_expr_pos_info.end())
            return display_error_pos(m_last_cmd_pos);
        else
            return display_error_pos(it->second);
    } else {
        return display_error_pos(m_last_cmd_pos);
    }
}

void parser_imp::display_error(char const * msg, unsigned line, unsigned pos) {
    display_error_pos(line, pos);
    regular(m_io_state) << " " << msg << endl;
}

void parser_imp::display_error(char const * msg) {
    display_error(msg, m_scanner.get_line(), m_scanner.get_pos());
}

void parser_imp::display_error(kernel_exception const & ex) {
    optional<expr> main_expr = ex.get_main_expr();
    if (main_expr)
        display_error_pos(some_expr(m_elaborator.get_original(*main_expr)));
    else
        display_error_pos(main_expr);
    regular(m_io_state) << " " << ex << endl;
}

void parser_imp::display_error(metavar_not_synthesized_exception const & ex) {
    display_error_pos(some_expr(m_elaborator.get_original(ex.m_mvar)));
    regular(m_io_state) << " " << ex.what() << ", metavariable: " << ex.m_mvar << ", type:\n";
    formatter fmt = m_io_state.get_formatter();
    options opts  = m_io_state.get_options();
    regular(m_io_state) << mk_pair(fmt(ex.m_mvar_ctx, ex.m_mvar_type, true, opts), opts) << "\n";
}

std::pair<unsigned, unsigned> parser_imp::lean_pos_info_provider::get_pos_info(expr const & e) const {
    expr const & o = m_ref.m_elaborator.get_original(e);
    auto it = m_ref.m_expr_pos_info.find(o);
    if (it == m_ref.m_expr_pos_info.end())
        throw exception("position is not available"); // information is not available
    return it->second;
}

void parser_imp::display_error(elaborator_exception const & ex) {
    formatter fmt = m_io_state.get_formatter();
    options opts  = m_io_state.get_options();
    lean_pos_info_provider pos_provider(*this);
    regular(m_io_state) << mk_pair(ex.get_justification().pp(fmt, opts, &pos_provider, true), opts) << endl;
}

void parser_imp::display_error(script_exception const & ex) {
    switch (ex.get_source()) {
    case script_exception::source::String:
        display_error_pos(ex.get_line() + m_last_script_pos.first - 1, static_cast<unsigned>(-1));
        regular(m_io_state) << " executing script," << ex.get_msg() << endl;
        break;
    case script_exception::source::File:
        display_error_pos(m_last_script_pos);
        regular(m_io_state) << " executing external script (" << ex.get_filename() << ":" << ex.get_line() << ")," << ex.get_msg() << endl;
        break;
    case script_exception::source::Unknown:
        display_error_pos(m_last_script_pos);
        regular(m_io_state) << " executing script, exact error position is not available, " << ex.what() << endl;
        break;
    }
}

void parser_imp::display_error(tactic_cmd_error const & ex) {
    display_error(ex.what(), ex.m_pos.first, ex.m_pos.second);
    display_proof_state(ex.m_state);
}

/**
   \brief Execute the given function \c f, and handle exceptions occurring
   when executing \c f.
   The parameter \c s is an error synchronization procedure.
*/
void parser_imp::protected_call(std::function<void()> && f, std::function<void()> && sync) {
    try {
        f();
    } catch (tactic_cmd_error & ex) {
        m_found_errors = true;
        if (m_show_errors)
            display_error(ex);
        sync();
        if (m_use_exceptions)
            throw;
    } catch (parser_error & ex) {
        m_found_errors = true;
        if (m_show_errors)
            display_error(ex.what(), ex.m_pos.first, ex.m_pos.second);
        sync();
        if (m_use_exceptions) {
            throw parser_exception(ex.what(), ex.m_pos.first, ex.m_pos.second);
        }
    } catch (kernel_exception & ex) {
        m_found_errors = true;
        if (m_show_errors)
            display_error(ex);
        sync();
        if (m_use_exceptions)
            throw;
    } catch (elaborator_exception & ex) {
        m_found_errors = true;
        if (m_show_errors)
            display_error(ex);
        sync();
        if (m_use_exceptions)
            throw;
    } catch (metavar_not_synthesized_exception & ex) {
        m_found_errors = true;
        if (m_show_errors)
            display_error(ex);
        sync();
        if (m_use_exceptions)
            throw;
    } catch (script_exception & ex) {
        m_found_errors = true;
        reset_interrupt();
        if (m_show_errors)
            display_error(ex);
        sync();
        if (m_use_exceptions)
            throw;
    } catch (interrupted & ex) {
        if (m_verbose)
            regular(m_io_state) << "!!!Interrupted!!!" << endl;
        reset_interrupt();
        sync();
        if (m_use_exceptions)
            throw;
    } catch (exception & ex) {
        m_found_errors = true;
        if (m_show_errors)
            display_error(ex.what());
        sync();
        if (m_use_exceptions)
            throw;
    }
}
}
