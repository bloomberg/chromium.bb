// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_GLUE_SYNCED_SESSION_H_
#define CHROME_BROWSER_SYNC_GLUE_SYNCED_SESSION_H_
#pragma once

#include <string>
#include <vector>

struct SessionWindow;

namespace browser_sync {

// Defines a synced session for use by session sync. A synced session is a
// list of windows along with a unique session identifer (tag) and meta-data
// about the device being synced.
struct SyncedSession {
  // The type of device.
  enum DeviceType {
    TYPE_WIN = 1,
    TYPE_MACOSX = 2,
    TYPE_LINUX = 3,
    TYPE_CHROMEOS = 4,
    TYPE_OTHER = 5
  };

  SyncedSession();
  ~SyncedSession();

  // Unique tag for each session.
  std::string session_tag;
  // User-visible name
  std::string session_name;
  DeviceType device_type;
  std::vector<SessionWindow*> windows;
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_GLUE_SYNCED_SESSION_H_
