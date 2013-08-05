// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_FILE_SYSTEM_CONFLICT_RESOLUTION_RESOLVER_H_
#define CHROME_BROWSER_SYNC_FILE_SYSTEM_CONFLICT_RESOLUTION_RESOLVER_H_

#include "chrome/browser/sync_file_system/conflict_resolution_policy.h"
#include "chrome/browser/sync_file_system/sync_file_type.h"

namespace base {
class Time;
}

namespace sync_file_system {

enum ConflictResolution {
  CONFLICT_RESOLUTION_UNKNOWN,
  CONFLICT_RESOLUTION_MARK_CONFLICT,
  CONFLICT_RESOLUTION_LOCAL_WIN,
  CONFLICT_RESOLUTION_REMOTE_WIN,
};

class ConflictResolutionResolver {
 public:
  explicit ConflictResolutionResolver(ConflictResolutionPolicy policy);
  ~ConflictResolutionResolver();

  // Determine the ConflictResolution.
  // This may return CONFLICT_RESOLUTION_UNKNOWN if NULL |remote_update_time|
  // is given.
  // It is invalid to give NULL |local_update_time|.
  ConflictResolution Resolve(
        SyncFileType local_file_type,
        const base::Time& local_update_time,
        SyncFileType remote_file_type,
        const base::Time& remote_update_time);

  ConflictResolutionPolicy policy() const { return policy_; }
  void set_policy(ConflictResolutionPolicy policy) { policy_ = policy; }

 private:
  ConflictResolutionPolicy policy_;
};

}  // namespace sync_file_system

#endif  // CHROME_BROWSER_SYNC_FILE_SYSTEM_CONFLICT_RESOLUTION_RESOLVER_H_
