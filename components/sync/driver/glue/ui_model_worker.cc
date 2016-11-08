// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/driver/glue/ui_model_worker.h"

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/synchronization/waitable_event.h"
#include "components/sync/base/scoped_event_signal.h"

namespace syncer {

namespace {

void CallDoWorkAndSignalEvent(const WorkCallback& work,
                              syncer::ScopedEventSignal scoped_event_signal,
                              SyncerError* error_info) {
  *error_info = work.Run();
  // The event in |scoped_event_signal| is signaled at the end of this scope.
}

}  // namespace

UIModelWorker::UIModelWorker(
    scoped_refptr<base::SingleThreadTaskRunner> ui_thread)
    : ui_thread_(std::move(ui_thread)) {}

SyncerError UIModelWorker::DoWorkAndWaitUntilDoneImpl(
    const WorkCallback& work) {
  SyncerError error_info;
  if (ui_thread_->BelongsToCurrentThread()) {
    DLOG(WARNING) << "DoWorkAndWaitUntilDone called from "
                  << "ui_loop_. Probably a nested invocation?";
    return work.Run();
  }

  // Signaled when the task is deleted, i.e. after it runs or when it is
  // abandoned.
  base::WaitableEvent work_done_or_abandoned(
      base::WaitableEvent::ResetPolicy::AUTOMATIC,
      base::WaitableEvent::InitialState::NOT_SIGNALED);

  if (!ui_thread_->PostTask(FROM_HERE,
                            base::Bind(&CallDoWorkAndSignalEvent, work,
                                       base::Passed(syncer::ScopedEventSignal(
                                           &work_done_or_abandoned)),
                                       &error_info))) {
    DLOG(WARNING) << "Could not post work to UI loop.";
    error_info = CANNOT_DO_WORK;
    return error_info;
  }
  work_done_or_abandoned.Wait();

  return error_info;
}

ModelSafeGroup UIModelWorker::GetModelSafeGroup() {
  return GROUP_UI;
}

bool UIModelWorker::IsOnModelThread() {
  return ui_thread_->BelongsToCurrentThread();
}

UIModelWorker::~UIModelWorker() {}

}  // namespace syncer
