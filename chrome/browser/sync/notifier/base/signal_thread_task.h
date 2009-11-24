// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_NOTIFIER_BASE_SIGNAL_THREAD_TASK_H_
#define CHROME_BROWSER_SYNC_NOTIFIER_BASE_SIGNAL_THREAD_TASK_H_

#include "talk/base/common.h"
#include "talk/base/signalthread.h"
#include "talk/base/sigslot.h"
#include "talk/base/task.h"

namespace notifier {

template<class T>
class SignalThreadTask : public talk_base::Task,
                         public sigslot::has_slots<> {
 public:
  // Takes ownership of signal_thread.
  SignalThreadTask(talk_base::Task* task_parent, T** signal_thread)
    : talk_base::Task(task_parent),
      signal_thread_(NULL),
      finished_(false) {
    SetSignalThread(signal_thread);
  }

  virtual ~SignalThreadTask() {
    ClearSignalThread();
  }

  virtual void Stop() {
    Task::Stop();
    ClearSignalThread();
  }

  virtual int ProcessStart() {
    ASSERT(GetState() == talk_base::Task::STATE_START);
    signal_thread_->SignalWorkDone.connect(
        this,
        &SignalThreadTask<T>::OnWorkDone);
    signal_thread_->Start();
    return talk_base::Task::STATE_RESPONSE;
  }

  int ProcessResponse() {
    if (!finished_) {
      return talk_base::Task::STATE_BLOCKED;
    }
    SignalWorkDone(signal_thread_);
    ClearSignalThread();
    return talk_base::Task::STATE_DONE;
  }

  sigslot::signal1<T*> SignalWorkDone;

 private:
  // Takes ownership of signal_thread.
  void SetSignalThread(T** signal_thread) {
    ASSERT(!signal_thread_ && signal_thread && *signal_thread);
    // Verify that no one is listening to the signal thread for work done.
    // They should be using this class instead.
    ASSERT((*signal_thread)->SignalWorkDone.is_empty());

    signal_thread_ = *signal_thread;

    // Helps callers not to use signal thread after this point since this class
    // has taken ownership (and avoid the error of doing
    // signal_thread->Start()).
    *signal_thread = NULL;
  }

  void OnWorkDone(talk_base::SignalThread* signal_thread) {
    ASSERT(signal_thread == signal_thread_);
    finished_ = true;
    Wake();
  }

  void ClearSignalThread() {
    if (signal_thread_) {
      // Don't wait on the thread destruction, or we may deadlock.
      signal_thread_->Destroy(false);
      signal_thread_ = NULL;
    }
  }

  T* signal_thread_;
  bool finished_;
  DISALLOW_COPY_AND_ASSIGN(SignalThreadTask);
};

}  // namespace notifier

#endif  // CHROME_BROWSER_SYNC_NOTIFIER_BASE_SIGNAL_THREAD_TASK_H_
