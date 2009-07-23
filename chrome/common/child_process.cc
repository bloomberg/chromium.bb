// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/child_process.h"

#include "base/message_loop.h"
#include "chrome/common/child_thread.h"

ChildProcess* ChildProcess::child_process_;

ChildProcess::ChildProcess()
    : ref_count_(0),
      shutdown_event_(true, false),
      io_thread_("Chrome_ChildIOThread") {
  DCHECK(!child_process_);
  child_process_ = this;

  io_thread_.StartWithOptions(base::Thread::Options(MessageLoop::TYPE_IO, 0));
}

ChildProcess::~ChildProcess() {
  DCHECK(child_process_ == this);

  // Signal this event before destroying the child process.  That way all
  // background threads can cleanup.
  // For example, in the renderer the RenderThread instances will be able to
  // notice shutdown before the render process begins waiting for them to exit.
  shutdown_event_.Signal();

  // Kill the main thread object before nulling child_process_, since
  // destruction code might depend on it.
  main_thread_.reset();

  child_process_ = NULL;
}

void ChildProcess::AddRefProcess() {
  DCHECK(!main_thread_.get() ||  // null in unittests.
         MessageLoop::current() == main_thread_->message_loop());
  ref_count_++;
}

void ChildProcess::ReleaseProcess() {
  DCHECK(!main_thread_.get() ||  // null in unittests.
         MessageLoop::current() == main_thread_->message_loop());
  DCHECK(ref_count_);
  DCHECK(child_process_);
  if (--ref_count_)
    return;

  if (main_thread_.get())  // null in unittests.
    main_thread_->OnProcessFinalRelease();
}

base::WaitableEvent* ChildProcess::GetShutDownEvent() {
  DCHECK(child_process_);
  return &child_process_->shutdown_event_;
}
