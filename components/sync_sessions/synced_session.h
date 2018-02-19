// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_SESSIONS_SYNCED_SESSION_H_
#define COMPONENTS_SYNC_SESSIONS_SYNCED_SESSION_H_

#include <map>
#include <memory>
#include <set>
#include <string>

#include "base/macros.h"
#include "base/time/time.h"
#include "components/sessions/core/session_id.h"
#include "components/sessions/core/session_types.h"
#include "components/sync/protocol/session_specifics.pb.h"
#include "components/sync/protocol/sync_enums.pb.h"

namespace sync_sessions {

// A Sync wrapper for a SessionWindow.
struct SyncedSessionWindow {
  SyncedSessionWindow();
  ~SyncedSessionWindow();

  // Convert this object into its sync protocol buffer equivalent.
  sync_pb::SessionWindow ToSessionWindowProto() const;

  // Type of the window. See session_specifics.proto.
  sync_pb::SessionWindow::BrowserType window_type;

  // The SessionWindow this object wraps.
  sessions::SessionWindow wrapped_window;

 private:
  DISALLOW_COPY_AND_ASSIGN(SyncedSessionWindow);
};

// Defines a synced session for use by session sync. A synced session is a
// list of windows along with a unique session identifer (tag) and meta-data
// about the device being synced.
struct SyncedSession {
  SyncedSession();
  ~SyncedSession();

  // Unique tag for each session.
  std::string session_tag;
  // User-visible name
  std::string session_name;

  // Type of device this session is from.
  sync_pb::SyncEnums::DeviceType device_type;

  // Last time this session was modified remotely. This is the max of the header
  // and all children tab mtimes.
  base::Time modified_time;

  // Map of windows that make up this session.
  std::map<SessionID::id_type, std::unique_ptr<SyncedSessionWindow>> windows;

  // A tab node id is part of the identifier for the sync tab objects. Tab node
  // ids are not used for interacting with the model/browser tabs. However, when
  // when we want to delete a foreign session, we use these values to inform
  // sync which tabs to delete. We are extracting these tab node ids from
  // individual session (tab, not header) specifics, but store them here in the
  // SyncedSession during runtime. We do this because tab node ids may be reused
  // for different tabs, and tracking which tab id is currently associated with
  // each tab node id is both difficult and unnecessary. See comments at
  // SyncedSessionTracker::GetTabImpl for a concrete example of id reuse.
  std::set<int> tab_node_ids;

  // Convert this object to its protocol buffer equivalent. Shallow conversion,
  // does not create SessionTab protobufs.
  sync_pb::SessionHeader ToSessionHeaderProto() const;

 private:
  DISALLOW_COPY_AND_ASSIGN(SyncedSession);
};

}  // namespace sync_sessions

#endif  // COMPONENTS_SYNC_SESSIONS_SYNCED_SESSION_H_
