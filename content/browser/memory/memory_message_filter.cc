// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/memory/memory_message_filter.h"

#include "content/browser/memory/memory_pressure_controller.h"
#include "content/common/memory_messages.h"

namespace content {

MemoryMessageFilter::MemoryMessageFilter()
    : BrowserMessageFilter(MemoryMsgStart) {}

MemoryMessageFilter::~MemoryMessageFilter() {}

void MemoryMessageFilter::OnFilterAdded(IPC::Sender* sender) {
  MemoryPressureController::GetInstance()->OnMemoryMessageFilterAdded(this);
}

void MemoryMessageFilter::OnChannelClosing() {
  MemoryPressureController::GetInstance()->OnMemoryMessageFilterRemoved(this);
}

bool MemoryMessageFilter::OnMessageReceived(const IPC::Message& message) {
  return false;
}

void MemoryMessageFilter::SendSetPressureNotificationsSuppressed(
    bool suppressed) {
  Send(new MemoryMsg_SetPressureNotificationsSuppressed(suppressed));
}

void MemoryMessageFilter::SendSimulatePressureNotification(
    base::MemoryPressureListener::MemoryPressureLevel level) {
  Send(new MemoryMsg_SimulatePressureNotification(level));
}

void MemoryMessageFilter::SendPressureNotification(
    base::MemoryPressureListener::MemoryPressureLevel level) {
  Send(new MemoryMsg_PressureNotification(level));
}

}  // namespace content
