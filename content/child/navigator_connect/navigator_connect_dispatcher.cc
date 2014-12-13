// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/navigator_connect/navigator_connect_dispatcher.h"

#include "base/message_loop/message_loop_proxy.h"
#include "content/child/navigator_connect/navigator_connect_provider.h"
#include "content/child/thread_safe_sender.h"
#include "content/child/worker_thread_task_runner.h"
#include "content/common/navigator_connect_messages.h"

namespace content {

NavigatorConnectDispatcher::NavigatorConnectDispatcher(ThreadSafeSender* sender)
    : main_thread_loop_proxy_(base::MessageLoopProxy::current()),
      thread_safe_sender_(sender) {
}

NavigatorConnectDispatcher::~NavigatorConnectDispatcher() {
}

base::TaskRunner* NavigatorConnectDispatcher::OverrideTaskRunnerForMessage(
    const IPC::Message& msg) {
  if (IPC_MESSAGE_CLASS(msg) != NavigatorConnectMsgStart)
    return NULL;
  int ipc_thread_id = 0;
  const bool success = PickleIterator(msg).ReadInt(&ipc_thread_id);
  DCHECK(success);
  if (!ipc_thread_id)
    return main_thread_loop_proxy_.get();
  return new WorkerThreadTaskRunner(ipc_thread_id);
}

bool NavigatorConnectDispatcher::OnMessageReceived(const IPC::Message& msg) {
  if (IPC_MESSAGE_CLASS(msg) != NavigatorConnectMsgStart)
    return false;
  NavigatorConnectProvider::ThreadSpecificInstance(thread_safe_sender_.get(),
                                                   main_thread_loop_proxy_)
      ->OnMessageReceived(msg);
  return true;
}

}  // namespace content
