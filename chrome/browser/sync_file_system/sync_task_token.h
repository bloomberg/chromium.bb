// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_FILE_SYSTEM_SYNC_TASK_TOKEN_H_
#define CHROME_BROWSER_SYNC_FILE_SYSTEM_SYNC_TASK_TOKEN_H_

#include "base/callback.h"
#include "base/location.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/sync_file_system/sync_callbacks.h"

namespace sync_file_system {

class SyncTaskManager;

// Represents a running sequence of SyncTasks.  Owned by a callback chain that
// should run exclusively, and held by SyncTaskManager when no task is running.
class SyncTaskToken {
 public:
  explicit SyncTaskToken(const base::WeakPtr<SyncTaskManager>& manager);

  void UpdateTask(const tracked_objects::Location& location,
                  const SyncStatusCallback& callback);

  const tracked_objects::Location& location() const { return location_; }
  ~SyncTaskToken();

  SyncTaskManager* manager() { return manager_.get(); }
  const SyncStatusCallback& callback() { return callback_; }
  void clear_callback() { callback_.Reset(); }

  static SyncStatusCallback WrapToCallback(scoped_ptr<SyncTaskToken> token);

 private:
  base::WeakPtr<SyncTaskManager> manager_;
  tracked_objects::Location location_;
  SyncStatusCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(SyncTaskToken);
};


}  // namespace sync_file_system

#endif  // CHROME_BROWSER_SYNC_FILE_SYSTEM_SYNC_TASK_TOKEN_H_
