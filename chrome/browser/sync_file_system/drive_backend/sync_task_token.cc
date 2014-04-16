// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/drive_backend/sync_task_token.h"

#include "base/bind.h"
#include "chrome/browser/sync_file_system/drive_backend/sync_task_manager.h"
#include "chrome/browser/sync_file_system/drive_backend/task_dependency_manager.h"

namespace sync_file_system {
namespace drive_backend {

const int64 SyncTaskToken::kForegroundTaskTokenID = 0;
const int64 SyncTaskToken::kMinimumBackgroundTaskTokenID = 1;

// static
scoped_ptr<SyncTaskToken> SyncTaskToken::CreateForForegroundTask(
    const base::WeakPtr<SyncTaskManager>& manager) {
  return make_scoped_ptr(new SyncTaskToken(
      manager,
      kForegroundTaskTokenID,
      scoped_ptr<BlockingFactor>()));
}

// static
scoped_ptr<SyncTaskToken> SyncTaskToken::CreateForBackgroundTask(
    const base::WeakPtr<SyncTaskManager>& manager,
    int64 token_id,
    scoped_ptr<BlockingFactor> blocking_factor) {
  return make_scoped_ptr(new SyncTaskToken(
      manager,
      token_id,
      blocking_factor.Pass()));
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
  if (manager_.get() && manager_->IsRunningTask(token_id_)) {
    NOTREACHED()
        << "Unexpected TaskToken deletion from: " << location_.ToString();

    // Reinitializes the token.
    SyncTaskManager::NotifyTaskDone(
        make_scoped_ptr(new SyncTaskToken(
            manager_, token_id_, blocking_factor_.Pass())),
        SYNC_STATUS_OK);
  }
}

// static
SyncStatusCallback SyncTaskToken::WrapToCallback(
    scoped_ptr<SyncTaskToken> token) {
  return base::Bind(&SyncTaskManager::NotifyTaskDone, base::Passed(&token));
}

void SyncTaskToken::set_blocking_factor(
    scoped_ptr<BlockingFactor> blocking_factor) {
  blocking_factor_ = blocking_factor.Pass();
}

const BlockingFactor* SyncTaskToken::blocking_factor() const {
  return blocking_factor_.get();
}

void SyncTaskToken::clear_blocking_factor() {
  blocking_factor_.reset();
}

SyncTaskToken::SyncTaskToken(const base::WeakPtr<SyncTaskManager>& manager,
                             int64 token_id,
                             scoped_ptr<BlockingFactor> blocking_factor)
    : manager_(manager),
      token_id_(token_id),
      blocking_factor_(blocking_factor.Pass()) {
}

}  // namespace drive_backend
}  // namespace sync_file_system
