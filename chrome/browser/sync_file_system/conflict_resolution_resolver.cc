// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/conflict_resolution_resolver.h"

#include "base/logging.h"
#include "base/time/time.h"

namespace sync_file_system {

ConflictResolutionResolver::ConflictResolutionResolver(
    ConflictResolutionPolicy policy)
    : policy_(policy) {}

ConflictResolutionResolver::~ConflictResolutionResolver() {}

ConflictResolution ConflictResolutionResolver::Resolve(
      SyncFileType local_file_type,
      const base::Time& local_update_time,
      SyncFileType remote_file_type,
      const base::Time& remote_update_time) {
  // Currently we always prioritize directories over files regardless of
  // conflict resolution policy.
  if (remote_file_type == SYNC_FILE_TYPE_DIRECTORY)
    return CONFLICT_RESOLUTION_REMOTE_WIN;

  if (policy_ == CONFLICT_RESOLUTION_POLICY_MANUAL)
    return CONFLICT_RESOLUTION_MARK_CONFLICT;

  // Remote time is null, cannot determine the resolution.
  if (remote_update_time.is_null())
    return CONFLICT_RESOLUTION_UNKNOWN;

  // Local time should be always given.
  DCHECK(!local_update_time.is_null());

  DCHECK_EQ(CONFLICT_RESOLUTION_POLICY_LAST_WRITE_WIN, policy_);
  if (local_update_time >= remote_update_time ||
      remote_file_type == SYNC_FILE_TYPE_UNKNOWN) {
    return CONFLICT_RESOLUTION_LOCAL_WIN;
  }

  return CONFLICT_RESOLUTION_REMOTE_WIN;
}

}  // namespace sync_file_system
