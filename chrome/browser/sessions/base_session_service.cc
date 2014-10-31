// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sessions/base_session_service.h"

#include "base/bind.h"
#include "base/threading/thread.h"
#include "chrome/browser/sessions/base_session_service_delegate.h"
#include "chrome/browser/sessions/session_backend.h"

// BaseSessionService ---------------------------------------------------------

namespace {

// Helper used by ScheduleGetLastSessionCommands. It runs callback on TaskRunner
// thread if it's not canceled.
void RunIfNotCanceled(
    const base::CancelableTaskTracker::IsCanceledCallback& is_canceled,
    const BaseSessionService::InternalGetCommandsCallback& callback,
    ScopedVector<SessionCommand> commands) {
  if (is_canceled.Run())
    return;
  callback.Run(commands.Pass());
}

void PostOrRunInternalGetCommandsCallback(
    base::TaskRunner* task_runner,
    const BaseSessionService::InternalGetCommandsCallback& callback,
    ScopedVector<SessionCommand> commands) {
  if (task_runner->RunsTasksOnCurrentThread()) {
    callback.Run(commands.Pass());
  } else {
    task_runner->PostTask(FROM_HERE,
                          base::Bind(callback, base::Passed(&commands)));
  }
}

}  // namespace

// Delay between when a command is received, and when we save it to the
// backend.
static const int kSaveDelayMS = 2500;

// static
const int BaseSessionService::max_persist_navigation_count = 6;

BaseSessionService::BaseSessionService(
    SessionType type,
    const base::FilePath& path,
    scoped_ptr<BaseSessionServiceDelegate> delegate)
    : pending_reset_(false),
      commands_since_reset_(0),
      delegate_(delegate.Pass()),
      sequence_token_(delegate_->GetBlockingPool()->GetSequenceToken()),
      weak_factory_(this) {
  backend_ = new SessionBackend(type, path);
  DCHECK(backend_.get());
}

BaseSessionService::~BaseSessionService() {
}

void BaseSessionService::DeleteLastSession() {
  RunTaskOnBackendThread(
      FROM_HERE,
      base::Bind(&SessionBackend::DeleteLastSession, backend()));
}

void BaseSessionService::ScheduleCommand(scoped_ptr<SessionCommand> command) {
  DCHECK(command);
  commands_since_reset_++;
  pending_commands_.push_back(command.release());
  StartSaveTimer();
}

void BaseSessionService::StartSaveTimer() {
  // Don't start a timer when testing.
  if (delegate_->ShouldUseDelayedSave() && base::MessageLoop::current() &&
      !weak_factory_.HasWeakPtrs()) {
    base::MessageLoop::current()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&BaseSessionService::Save, weak_factory_.GetWeakPtr()),
        base::TimeDelta::FromMilliseconds(kSaveDelayMS));
  }
}

void BaseSessionService::Save() {
  DCHECK(backend());

  if (pending_commands_.empty())
    return;

  RunTaskOnBackendThread(
      FROM_HERE,
      base::Bind(&SessionBackend::AppendCommands, backend(),
                 base::Passed(&pending_commands_),
                 pending_reset_));

  if (pending_reset_) {
    commands_since_reset_ = 0;
    pending_reset_ = false;
  }
}

bool BaseSessionService::ShouldTrackEntry(const GURL& url) {
  return url.is_valid() && delegate_->ShouldTrackEntry(url);
}

base::CancelableTaskTracker::TaskId
BaseSessionService::ScheduleGetLastSessionCommands(
    const InternalGetCommandsCallback& callback,
    base::CancelableTaskTracker* tracker) {
  base::CancelableTaskTracker::IsCanceledCallback is_canceled;
  base::CancelableTaskTracker::TaskId id =
      tracker->NewTrackedTaskId(&is_canceled);

  InternalGetCommandsCallback run_if_not_canceled =
      base::Bind(&RunIfNotCanceled, is_canceled, callback);

  InternalGetCommandsCallback callback_runner =
      base::Bind(&PostOrRunInternalGetCommandsCallback,
                 base::MessageLoopProxy::current(), run_if_not_canceled);

  RunTaskOnBackendThread(
      FROM_HERE,
      base::Bind(&SessionBackend::ReadLastSessionCommands, backend(),
                 is_canceled, callback_runner));
  return id;
}

void BaseSessionService::RunTaskOnBackendThread(
    const tracked_objects::Location& from_here,
    const base::Closure& task) {
  base::SequencedWorkerPool* pool = delegate_->GetBlockingPool();
  if (!pool->IsShutdownInProgress()) {
    pool->PostSequencedWorkerTask(sequence_token_, from_here, task);
  } else {
    // Fall back to executing on the main thread if the sequence
    // worker pool has been requested to shutdown (around shutdown
    // time).
    task.Run();
  }
}
