// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/navigator_connect/navigator_connect_dispatcher.h"

#include "content/child/navigator_connect/navigator_connect_provider.h"
#include "content/common/navigator_connect_messages.h"

namespace content {

NavigatorConnectDispatcher::NavigatorConnectDispatcher(ThreadSafeSender* sender)
    : WorkerThreadMessageFilter(sender) {
}

NavigatorConnectDispatcher::~NavigatorConnectDispatcher() {
}

bool NavigatorConnectDispatcher::ShouldHandleMessage(
    const IPC::Message& msg) const {
  return IPC_MESSAGE_CLASS(msg) == NavigatorConnectMsgStart;
}

void NavigatorConnectDispatcher::OnFilteredMessageReceived(
    const IPC::Message& msg) {
  NavigatorConnectProvider::ThreadSpecificInstance(
      thread_safe_sender(), main_thread_task_runner())->OnMessageReceived(msg);
}

bool NavigatorConnectDispatcher::GetWorkerThreadIdForMessage(
    const IPC::Message& msg,
    int* ipc_thread_id) {
  return PickleIterator(msg).ReadInt(ipc_thread_id);
}

}  // namespace content
