// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine/ui_model_worker.h"

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "components/sync/base/scoped_event_signal.h"

namespace syncer {

namespace {

class ScopedEventSignalWithWorker {
 public:
  ScopedEventSignalWithWorker(scoped_refptr<UIModelWorker> ui_model_worker,
                              base::WaitableEvent* event)
      : ui_model_worker_(std::move(ui_model_worker)),
        scoped_event_signal_(event) {}

  ScopedEventSignalWithWorker(ScopedEventSignalWithWorker&&) = default;
  ScopedEventSignalWithWorker& operator=(ScopedEventSignalWithWorker&&) =
      default;

  bool IsStopped() const { return ui_model_worker_->IsStopped(); }

 private:
  // This reference prevents the event in |scoped_event_signal_| from being
  // signaled after being deleted.
  scoped_refptr<UIModelWorker> ui_model_worker_;

  ScopedEventSignal scoped_event_signal_;

  DISALLOW_COPY_AND_ASSIGN(ScopedEventSignalWithWorker);
};

void CallDoWorkAndSignalEvent(
    const WorkCallback& work,
    ScopedEventSignalWithWorker scoped_event_signal_with_worker,
    SyncerError* error_info) {
  if (!scoped_event_signal_with_worker.IsStopped())
    *error_info = work.Run();
  // The event in |scoped_event_signal_with_worker| is signaled at the end of
  // this scope.
}

}  // namespace

UIModelWorker::UIModelWorker(
    scoped_refptr<base::SingleThreadTaskRunner> ui_thread)
    : ui_thread_(std::move(ui_thread)),
      work_done_or_abandoned_(base::WaitableEvent::ResetPolicy::MANUAL,
                              base::WaitableEvent::InitialState::NOT_SIGNALED),
      stop_requested_(base::WaitableEvent::ResetPolicy::MANUAL,
                      base::WaitableEvent::InitialState::NOT_SIGNALED) {
  sequence_checker_.DetachFromSequence();
}

SyncerError UIModelWorker::DoWorkAndWaitUntilDoneImpl(
    const WorkCallback& work) {
  DCHECK(sequence_checker_.CalledOnValidSequence());
  DCHECK(!ui_thread_->BelongsToCurrentThread());

  SyncerError error_info;
  work_done_or_abandoned_.Reset();

  if (!ui_thread_->PostTask(
          FROM_HERE,
          base::Bind(&CallDoWorkAndSignalEvent, work,
                     base::Passed(syncer::ScopedEventSignalWithWorker(
                         this, &work_done_or_abandoned_)),
                     &error_info))) {
    DLOG(WARNING) << "Could not post work to UI loop.";
    error_info = CANNOT_DO_WORK;
    return error_info;
  }

  base::WaitableEvent* events[] = {&work_done_or_abandoned_, &stop_requested_};
  base::WaitableEvent::WaitMany(events, arraysize(events));

  return error_info;
}

void UIModelWorker::RequestStop() {
  DCHECK(ui_thread_->BelongsToCurrentThread());
  ModelSafeWorker::RequestStop();
  stop_requested_.Signal();
}

ModelSafeGroup UIModelWorker::GetModelSafeGroup() {
  return GROUP_UI;
}

bool UIModelWorker::IsOnModelThread() {
  return ui_thread_->BelongsToCurrentThread();
}

UIModelWorker::~UIModelWorker() {}

}  // namespace syncer
