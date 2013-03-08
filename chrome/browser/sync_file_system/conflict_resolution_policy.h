// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_FILE_SYSTEM_CONFLICT_RESOLUTION_POLICY_H_
#define CHROME_BROWSER_SYNC_FILE_SYSTEM_CONFLICT_RESOLUTION_POLICY_H_

namespace sync_file_system {

enum ConflictResolutionPolicy {
  // The service automatically resolves a conflict by choosing the one
  // that is updated more recently.
  CONFLICT_RESOLUTION_LAST_WRITE_WIN,

  // The service does nothing but just leaves conflicting files as
  // 'conflicted' state.
  CONFLICT_RESOLUTION_MANUAL,
};

}  // namespace sync_file_system

#endif  // CHROME_BROWSER_SYNC_FILE_SYSTEM_CONFLICT_RESOLUTION_POLICY_H_
