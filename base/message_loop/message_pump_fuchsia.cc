// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop/message_pump_fuchsia.h"

#include "base/logging.h"

namespace base {

MessagePumpFuchsia::FileDescriptorWatcher::FileDescriptorWatcher(
    const tracked_objects::Location& from_here)
    : created_from_location_(from_here) {
  NOTIMPLEMENTED();
}

MessagePumpFuchsia::FileDescriptorWatcher::~FileDescriptorWatcher() {}

bool MessagePumpFuchsia::FileDescriptorWatcher::StopWatchingFileDescriptor() {
  NOTIMPLEMENTED();
  return false;
}

MessagePumpFuchsia::MessagePumpFuchsia() {}

MessagePumpFuchsia::~MessagePumpFuchsia() {}

bool MessagePumpFuchsia::WatchFileDescriptor(int fd,
                                             bool persistent,
                                             int mode,
                                             FileDescriptorWatcher* controller,
                                             Watcher* delegate) {
  NOTIMPLEMENTED();
  return false;
}

void MessagePumpFuchsia::Run(Delegate* delegate) {
  NOTIMPLEMENTED();
}

void MessagePumpFuchsia::Quit() {
  NOTIMPLEMENTED();
}

void MessagePumpFuchsia::ScheduleWork() {
  NOTIMPLEMENTED();
}

void MessagePumpFuchsia::ScheduleDelayedWork(
    const TimeTicks& delayed_work_time) {
  NOTIMPLEMENTED();
}

}  // namespace base
