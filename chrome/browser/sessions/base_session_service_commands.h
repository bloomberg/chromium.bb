// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SESSIONS_BASE_SESSION_SERVICE_COMMANDS_H_
#define CHROME_BROWSER_SESSIONS_BASE_SESSION_SERVICE_COMMANDS_H_

#include <string>

#include "base/memory/scoped_ptr.h"
#include "components/sessions/session_id.h"

class SessionCommand;

namespace sessions {
class SerializedNavigationEntry;
}

// These commands create and read common base commands for SessionService and
// PersistentTabRestoreService.

// Creates a SessionCommand that represents a navigation.
scoped_ptr<SessionCommand> CreateUpdateTabNavigationCommand(
    SessionID::id_type command_id,
    SessionID::id_type tab_id,
    const sessions::SerializedNavigationEntry& navigation);

// Creates a SessionCommand that represents marking a tab as an application.
scoped_ptr<SessionCommand> CreateSetTabExtensionAppIDCommand(
    SessionID::id_type command_id,
    SessionID::id_type tab_id,
    const std::string& extension_id);

// Creates a SessionCommand that containing user agent override used by a
// tab's navigations.
scoped_ptr<SessionCommand> CreateSetTabUserAgentOverrideCommand(
    SessionID::id_type command_id,
    SessionID::id_type tab_id,
    const std::string& user_agent_override);

// Creates a SessionCommand stores a browser window's app name.
scoped_ptr<SessionCommand> CreateSetWindowAppNameCommand(
    SessionID::id_type command_id,
    SessionID::id_type window_id,
    const std::string& app_name);

// Converts a SessionCommand previously created by
// CreateUpdateTabNavigationCommand into a
// sessions::SerializedNavigationEntry. Returns true on success. If
// successful |tab_id| is set to the id of the restored tab.
bool RestoreUpdateTabNavigationCommand(
    const SessionCommand& command,
    sessions::SerializedNavigationEntry* navigation,
    SessionID::id_type* tab_id);

// Extracts a SessionCommand as previously created by
// CreateSetTabExtensionAppIDCommand into the tab id and application
// extension id.
bool RestoreSetTabExtensionAppIDCommand(const SessionCommand& command,
                                        SessionID::id_type* tab_id,
                                        std::string* extension_app_id);

// Extracts a SessionCommand as previously created by
// CreateSetTabUserAgentOverrideCommand into the tab id and user agent.
bool RestoreSetTabUserAgentOverrideCommand(const SessionCommand& command,
                                           SessionID::id_type* tab_id,
                                           std::string* user_agent_override);

// Extracts a SessionCommand as previously created by
// CreateSetWindowAppNameCommand into the window id and application name.
bool RestoreSetWindowAppNameCommand(const SessionCommand& command,
                                    SessionID::id_type* window_id,
                                    std::string* app_name);

#endif  // CHROME_BROWSER_SESSIONS_BASE_SESSION_SERVICE_COMMANDS_H_
