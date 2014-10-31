// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SESSIONS_SESSION_SERVICE_COMMANDS_H_
#define CHROME_BROWSER_SESSIONS_SESSION_SERVICE_COMMANDS_H_

#include <map>
#include <string>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/memory/scoped_vector.h"
#include "base/memory/weak_ptr.h"
#include "base/task/cancelable_task_tracker.h"
#include "chrome/browser/sessions/base_session_service.h"
#include "chrome/browser/sessions/session_types.h"
#include "ui/base/ui_base_types.h"

class SessionCommand;

// The following functions create sequentialized change commands which are
// used to reconstruct the current/previous session state.
// It is up to the caller to delete the returned SessionCommand* object.
scoped_ptr<SessionCommand> CreateSetSelectedTabInWindowCommand(
    const SessionID& window_id,
    int index);
scoped_ptr<SessionCommand> CreateSetTabWindowCommand(const SessionID& window_id,
                                                     const SessionID& tab_id);
scoped_ptr<SessionCommand> CreateSetWindowBoundsCommand(
    const SessionID& window_id,
    const gfx::Rect& bounds,
    ui::WindowShowState show_state);
scoped_ptr<SessionCommand> CreateSetTabIndexInWindowCommand(
    const SessionID& tab_id,
    int new_index);
scoped_ptr<SessionCommand> CreateTabClosedCommand(SessionID::id_type tab_id);
scoped_ptr<SessionCommand> CreateWindowClosedCommand(SessionID::id_type tab_id);
scoped_ptr<SessionCommand> CreateSetSelectedNavigationIndexCommand(
    const SessionID& tab_id,
    int index);
scoped_ptr<SessionCommand> CreateSetWindowTypeCommand(
    const SessionID& window_id,
    SessionWindow::WindowType type);
scoped_ptr<SessionCommand> CreatePinnedStateCommand(const SessionID& tab_id,
                                         bool is_pinned);
scoped_ptr<SessionCommand> CreateSessionStorageAssociatedCommand(
    const SessionID& tab_id,
    const std::string& session_storage_persistent_id);
scoped_ptr<SessionCommand> CreateSetActiveWindowCommand(
    const SessionID& window_id);
scoped_ptr<SessionCommand> CreateTabNavigationPathPrunedFromBackCommand(
    const SessionID& tab_id,
    int count);
scoped_ptr<SessionCommand> CreateTabNavigationPathPrunedFromFrontCommand(
    const SessionID& tab_id,
    int count);
scoped_ptr<SessionCommand> CreateUpdateTabNavigationCommand(
    const SessionID& tab_id,
    const sessions::SerializedNavigationEntry& navigation);
scoped_ptr<SessionCommand> CreateSetTabExtensionAppIDCommand(
    const SessionID& tab_id,
    const std::string& extension_id);
scoped_ptr<SessionCommand> CreateSetTabUserAgentOverrideCommand(
    const SessionID& tab_id,
    const std::string& user_agent_override);
scoped_ptr<SessionCommand> CreateSetWindowAppNameCommand(
    const SessionID& window_id,
    const std::string& app_name);

// Searches for a pending command in |pending_commands| that can be replaced
// with |command|. If one is found, pending command is removed, the command
// is added to the pending commands (taken ownership) and true is returned.
bool ReplacePendingCommand(
  ScopedVector<SessionCommand>& pending_commands,
  scoped_ptr<SessionCommand>* command);

// Returns true if provided |command| either closes a window or a tab.
bool IsClosingCommand(SessionCommand* command);

// Converts a list of commands into SessionWindows. On return any valid
// windows are added to valid_windows. It is up to the caller to delete
// the windows added to valid_windows. |active_window_id| will be set with the
// id of the last active window, but it's only valid when this id corresponds
// to the id of one of the windows in valid_windows.
void RestoreSessionFromCommands(const ScopedVector<SessionCommand>& commands,
                                std::vector<SessionWindow*>* valid_windows,
                                SessionID::id_type* active_window_id);

#endif  // CHROME_BROWSER_SESSIONS_SESSION_SERVICE_COMMANDS_H_
