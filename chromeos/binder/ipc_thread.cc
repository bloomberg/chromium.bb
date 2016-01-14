// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/binder/ipc_thread.h"

#include "chromeos/binder/command_broker.h"
#include "chromeos/binder/driver.h"

namespace binder {

IpcThread::IpcThread()
    : base::Thread("BinderThread"),
      driver_(new Driver()),
      watcher_(new base::MessageLoopForIO::FileDescriptorWatcher()) {}

IpcThread::~IpcThread() {
  Stop();
}

bool IpcThread::Start() {
  DCHECK(!initialized_);
  base::Thread::Options options;
  options.message_loop_type = base::MessageLoop::TYPE_IO;
  return StartWithOptions(options);
}

void IpcThread::OnFileCanReadWithoutBlocking(int fd) {
  DCHECK(initialized_);
  bool success = command_broker_->PollCommands();
  LOG_IF(ERROR, !success) << "PollCommands() failed.";
}

void IpcThread::OnFileCanWriteWithoutBlocking(int fd) {
  NOTREACHED();
}

void IpcThread::Init() {
  DCHECK(!initialized_);
  if (!driver_->Initialize()) {
    LOG(ERROR) << "Failed to initialize driver.";
    return;
  }
  command_broker_.reset(new CommandBroker(driver_.get()));
  if (!command_broker_->EnterLooper()) {
    LOG(ERROR) << "Failed to enter looper.";
    return;
  }
  if (!base::MessageLoopForIO::current()->WatchFileDescriptor(
          driver_->GetFD(), true, base::MessageLoopForIO::WATCH_READ,
          watcher_.get(), this)) {
    LOG(ERROR) << "Failed to initialize watcher.";
    return;
  }
  initialized_ = true;
}

void IpcThread::CleanUp() {
  DCHECK(initialized_);
  if (!command_broker_->ExitLooper()) {
    LOG(ERROR) << "Failed to exit looper.";
  }
  if (!driver_->NotifyCurrentThreadExiting()) {
    LOG(ERROR) << "Failed to send thread exit.";
  }
  watcher_.reset();
  command_broker_.reset();
  driver_.reset();
}

}  // namespace binder
