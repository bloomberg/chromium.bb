// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/process_background_message_filter.h"

#include "base/process/process.h"
#include "base/thread_task_runner_handle.h"
#include "content/common/child_process_messages.h"

namespace content {

ProcessBackgroundMessageFilter::ProcessBackgroundMessageFilter()
    : main_thread_task_runner_(base::ThreadTaskRunnerHandle::Get()) {
}

ProcessBackgroundMessageFilter::~ProcessBackgroundMessageFilter() {}

bool ProcessBackgroundMessageFilter::OnMessageReceived(
    const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(ProcessBackgroundMessageFilter, message)
    IPC_MESSAGE_HANDLER(ChildProcessMsg_SetProcessBackgrounded,
                        OnProcessBackgrounded)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void ProcessBackgroundMessageFilter::SetTimerSlack(bool background) {
  DCHECK(thread_checker_.CalledOnValidThread());
  // Set timer slack to maximum on main thread when in background.
  base::MessageLoop::current()->SetTimerSlack(
      background ? base::TIMER_SLACK_MAXIMUM : base::TIMER_SLACK_NONE);
}

void ProcessBackgroundMessageFilter::OnProcessBackgrounded(bool background) {
  DCHECK(base::MessageLoopForIO::IsCurrent());
  // Post to the main thread to set the timer slack.
  main_thread_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(
          &ProcessBackgroundMessageFilter::SetTimerSlack, this, background));
}

}  // namespace content
