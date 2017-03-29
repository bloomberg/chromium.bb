// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_sessions/synced_session.h"

namespace sync_sessions {

SyncedSessionWindow::SyncedSessionWindow() {}

SyncedSessionWindow::~SyncedSessionWindow() {}

sync_pb::SessionWindow SyncedSessionWindow::ToSessionWindowProto() const {
  sync_pb::SessionWindow sync_data;
  sync_data.set_browser_type(window_type);
  sync_data.set_window_id(wrapped_window.window_id.id());
  sync_data.set_selected_tab_index(wrapped_window.selected_tab_index);

  for (const auto& tab : wrapped_window.tabs)
    sync_data.add_tab(tab->tab_id.id());

  return sync_data;
}

SyncedSession::SyncedSession()
    : session_tag("invalid"), device_type(TYPE_UNSET) {}

SyncedSession::~SyncedSession() {}

sync_pb::SessionHeader SyncedSession::ToSessionHeaderProto() const {
  sync_pb::SessionHeader header;
  for (const auto& window_pair : windows) {
    sync_pb::SessionWindow* w = header.add_window();
    w->CopyFrom(window_pair.second->ToSessionWindowProto());
  }
  header.set_client_name(session_name);
  switch (device_type) {
    case SyncedSession::TYPE_WIN:
      header.set_device_type(sync_pb::SyncEnums_DeviceType_TYPE_WIN);
      break;
    case SyncedSession::TYPE_MACOSX:
      header.set_device_type(sync_pb::SyncEnums_DeviceType_TYPE_MAC);
      break;
    case SyncedSession::TYPE_LINUX:
      header.set_device_type(sync_pb::SyncEnums_DeviceType_TYPE_LINUX);
      break;
    case SyncedSession::TYPE_CHROMEOS:
      header.set_device_type(sync_pb::SyncEnums_DeviceType_TYPE_CROS);
      break;
    case SyncedSession::TYPE_PHONE:
      header.set_device_type(sync_pb::SyncEnums_DeviceType_TYPE_PHONE);
      break;
    case SyncedSession::TYPE_TABLET:
      header.set_device_type(sync_pb::SyncEnums_DeviceType_TYPE_TABLET);
      break;
    case SyncedSession::TYPE_OTHER:
    // Intentionally fall-through
    default:
      header.set_device_type(sync_pb::SyncEnums_DeviceType_TYPE_OTHER);
      break;
  }
  return header;
}

}  // namespace sync_sessions
