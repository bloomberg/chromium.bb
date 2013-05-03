// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_pump_linux.h"

#include "base/logging.h"
#include "base/message_loop.h"

namespace base {

MessagePumpLinux::MessagePumpLinux()
    : MessagePumpLibevent() {
}

MessagePumpLinux::~MessagePumpLinux() {
}

void MessagePumpLinux::AddObserver(MessagePumpObserver* /* observer */) {
  NOTIMPLEMENTED();
}

void MessagePumpLinux::RemoveObserver(MessagePumpObserver* /* observer */) {
  NOTIMPLEMENTED();
}

// static
MessagePumpLinux* MessagePumpLinux::Current() {
  MessageLoopForUI* loop = MessageLoopForUI::current();
  return static_cast<MessagePumpLinux*>(loop->pump_ui());
}

void MessagePumpLinux::AddDispatcherForRootWindow(
    MessagePumpDispatcher* dispatcher) {
  // Only one root window is supported.
  DCHECK(dispatcher_.size() == 0);
  dispatcher_.insert(dispatcher_.begin(),dispatcher);
}

void MessagePumpLinux::RemoveDispatcherForRootWindow(
      MessagePumpDispatcher* dispatcher) {
  DCHECK(dispatcher_.size() == 1);
  dispatcher_.pop_back();
}

bool MessagePumpLinux::Dispatch(const base::NativeEvent& dev) {
  if (dispatcher_.size() > 0)
    return dispatcher_[0]->Dispatch(dev);
  else
    return true;
}

// This code assumes that the caller tracks the lifetime of the |dispatcher|.
void MessagePumpLinux::RunWithDispatcher(
    Delegate* delegate, MessagePumpDispatcher* dispatcher) {
  dispatcher_.push_back(dispatcher);
  Run(delegate);
  dispatcher_.pop_back();
}

}  // namespace base
