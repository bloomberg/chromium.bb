// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/bluetooth/bluetooth_message_filter.h"

#include "content/child/thread_safe_sender.h"
#include "content/renderer/bluetooth/bluetooth_dispatcher.h"
#include "ipc/ipc_message_macros.h"

namespace content {

BluetoothMessageFilter::BluetoothMessageFilter(ThreadSafeSender* sender)
    : WorkerThreadMessageFilter(sender) {
}

BluetoothMessageFilter::~BluetoothMessageFilter() {
}

bool BluetoothMessageFilter::ShouldHandleMessage(
    const IPC::Message& msg) const {
  return IPC_MESSAGE_CLASS(msg) == BluetoothMsgStart;
}

void BluetoothMessageFilter::OnFilteredMessageReceived(
    const IPC::Message& msg) {
  BluetoothDispatcher::GetOrCreateThreadSpecificInstance(thread_safe_sender())
      ->OnMessageReceived(msg);
}

bool BluetoothMessageFilter::GetWorkerThreadIdForMessage(
    const IPC::Message& msg,
    int* ipc_thread_id) {
  return base::PickleIterator(msg).ReadInt(ipc_thread_id);
}

}  // namespace content
