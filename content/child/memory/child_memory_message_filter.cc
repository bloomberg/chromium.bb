// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/memory/child_memory_message_filter.h"

#include "base/memory/memory_pressure_listener.h"
#include "content/common/memory_messages.h"

namespace content {

ChildMemoryMessageFilter::ChildMemoryMessageFilter() {}

ChildMemoryMessageFilter::~ChildMemoryMessageFilter() {}

bool ChildMemoryMessageFilter::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(ChildMemoryMessageFilter, message)
    IPC_MESSAGE_HANDLER(MemoryMsg_SetPressureNotificationsSuppressed,
                        OnSetPressureNotificationsSuppressed)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void ChildMemoryMessageFilter::OnSetPressureNotificationsSuppressed(
    bool suppressed) {
  // Enable/disable suppressing memory notifications in the child process.
  base::MemoryPressureListener::SetNotificationsSuppressed(suppressed);
}

}  // namespace content
