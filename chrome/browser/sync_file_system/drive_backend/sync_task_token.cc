// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/drive_backend/sync_task_token.h"

#include "base/bind.h"
#include "chrome/browser/sync_file_system/drive_backend/sync_task_manager.h"

namespace sync_file_system {
namespace drive_backend {

SyncTaskToken::SyncTaskToken(const base::WeakPtr<SyncTaskManager>& manager)
    : manager_(manager) {
}

void SyncTaskToken::UpdateTask(const tracked_objects::Location& location,
                               const SyncStatusCallback& callback) {
  DCHECK(callback_.is_null());
  location_ = location;
  callback_ = callback;
  DVLOG(2) << "Token updated: " << location_.ToString();
}

SyncTaskToken::~SyncTaskToken() {
  // All task on Client must hold TaskToken instance to ensure
  // no other tasks are running. Also, as soon as a task finishes to work,
  // it must return the token to TaskManager.
  // Destroying a token with valid |client| indicates the token was
  // dropped by a task without returning.
  if (manager_.get() && manager_->HasClient()) {
    NOTREACHED()
        << "Unexpected TaskToken deletion from: " << location_.ToString();

    // Reinitializes the token.
    SyncTaskManager::NotifyTaskDone(
        make_scoped_ptr(new SyncTaskToken(manager_)), SYNC_STATUS_OK);
  }
}

// static
SyncStatusCallback SyncTaskToken::WrapToCallback(
    scoped_ptr<SyncTaskToken> token) {
  return base::Bind(&SyncTaskManager::NotifyTaskDone, base::Passed(&token));
}

}  // namespace drive_backend
}  // namespace sync_file_system
