// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sessions/base_session_service_commands.h"

#include "base/pickle.h"
#include "components/sessions/session_backend.h"
#include "components/sessions/session_types.h"

namespace sessions {
namespace {

// Helper used by CreateUpdateTabNavigationCommand(). It writes |str| to
// |pickle|, if and only if |str| fits within (|max_bytes| - |*bytes_written|).
// |bytes_written| is incremented to reflect the data written.
void WriteStringToPickle(Pickle& pickle, int* bytes_written, int max_bytes,
                         const std::string& str) {
  int num_bytes = str.size() * sizeof(char);
  if (*bytes_written + num_bytes < max_bytes) {
    *bytes_written += num_bytes;
    pickle.WriteString(str);
  } else {
    pickle.WriteString(std::string());
  }
}

}  // namespace

scoped_ptr<SessionCommand> CreateUpdateTabNavigationCommand(
    SessionID::id_type command_id,
    SessionID::id_type tab_id,
    const sessions::SerializedNavigationEntry& navigation) {
  // Use pickle to handle marshalling.
  Pickle pickle;
  pickle.WriteInt(tab_id);
  // We only allow navigations up to 63k (which should be completely
  // reasonable).
  static const size_t max_state_size =
      std::numeric_limits<SessionCommand::size_type>::max() - 1024;
  navigation.WriteToPickle(max_state_size, &pickle);
  return scoped_ptr<SessionCommand>(new SessionCommand(command_id, pickle));
}

scoped_ptr<SessionCommand> CreateSetTabExtensionAppIDCommand(
    SessionID::id_type command_id,
    SessionID::id_type tab_id,
    const std::string& extension_id) {
  // Use pickle to handle marshalling.
  Pickle pickle;
  pickle.WriteInt(tab_id);

  // Enforce a max for ids. They should never be anywhere near this size.
  static const SessionCommand::size_type max_id_size =
      std::numeric_limits<SessionCommand::size_type>::max() - 1024;

  int bytes_written = 0;

  WriteStringToPickle(pickle, &bytes_written, max_id_size, extension_id);

  return scoped_ptr<SessionCommand>(new SessionCommand(command_id, pickle));
}

scoped_ptr<SessionCommand> CreateSetTabUserAgentOverrideCommand(
    SessionID::id_type command_id,
    SessionID::id_type tab_id,
    const std::string& user_agent_override) {
  // Use pickle to handle marshalling.
  Pickle pickle;
  pickle.WriteInt(tab_id);

  // Enforce a max for the user agent length.  They should never be anywhere
  // near this size.
  static const SessionCommand::size_type max_user_agent_size =
      std::numeric_limits<SessionCommand::size_type>::max() - 1024;

  int bytes_written = 0;

  WriteStringToPickle(pickle, &bytes_written, max_user_agent_size,
      user_agent_override);

  return scoped_ptr<SessionCommand>(new SessionCommand(command_id, pickle));
}

scoped_ptr<SessionCommand> CreateSetWindowAppNameCommand(
    SessionID::id_type command_id,
    SessionID::id_type window_id,
    const std::string& app_name) {
  // Use pickle to handle marshalling.
  Pickle pickle;
  pickle.WriteInt(window_id);

  // Enforce a max for ids. They should never be anywhere near this size.
  static const SessionCommand::size_type max_id_size =
      std::numeric_limits<SessionCommand::size_type>::max() - 1024;

  int bytes_written = 0;

  WriteStringToPickle(pickle, &bytes_written, max_id_size, app_name);

  return scoped_ptr<SessionCommand>(new SessionCommand(command_id, pickle));
}

bool RestoreUpdateTabNavigationCommand(
    const SessionCommand& command,
    sessions::SerializedNavigationEntry* navigation,
    SessionID::id_type* tab_id) {
  scoped_ptr<Pickle> pickle(command.PayloadAsPickle());
  if (!pickle.get())
    return false;
  PickleIterator iterator(*pickle);
  return iterator.ReadInt(tab_id) && navigation->ReadFromPickle(&iterator);
}

bool RestoreSetTabExtensionAppIDCommand(const SessionCommand& command,
                                        SessionID::id_type* tab_id,
                                        std::string* extension_app_id) {
  scoped_ptr<Pickle> pickle(command.PayloadAsPickle());
  if (!pickle.get())
    return false;

  PickleIterator iterator(*pickle);
  return iterator.ReadInt(tab_id) && iterator.ReadString(extension_app_id);
}

bool RestoreSetTabUserAgentOverrideCommand(const SessionCommand& command,
                                           SessionID::id_type* tab_id,
                                           std::string* user_agent_override) {
  scoped_ptr<Pickle> pickle(command.PayloadAsPickle());
  if (!pickle.get())
    return false;

  PickleIterator iterator(*pickle);
  return iterator.ReadInt(tab_id) && iterator.ReadString(user_agent_override);
}

bool RestoreSetWindowAppNameCommand(const SessionCommand& command,
                                    SessionID::id_type* window_id,
                                    std::string* app_name) {
  scoped_ptr<Pickle> pickle(command.PayloadAsPickle());
  if (!pickle.get())
    return false;

  PickleIterator iterator(*pickle);
  return iterator.ReadInt(window_id) && iterator.ReadString(app_name);
}

}  // namespace sessions
