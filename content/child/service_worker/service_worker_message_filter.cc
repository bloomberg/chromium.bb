// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/service_worker/service_worker_message_filter.h"

#include "ipc/ipc_message_macros.h"
#include "base/bind.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/pickle.h"
#include "content/child/service_worker/service_worker_dispatcher.h"
#include "content/child/thread_safe_sender.h"
#include "webkit/child/worker_task_runner.h"

using webkit_glue::WorkerTaskRunner;

namespace content {

ServiceWorkerMessageFilter::ServiceWorkerMessageFilter(ThreadSafeSender* sender)
    : main_thread_loop_proxy_(base::MessageLoopProxy::current()),
      thread_safe_sender_(sender) {}

ServiceWorkerMessageFilter::~ServiceWorkerMessageFilter() {}

bool ServiceWorkerMessageFilter::OnMessageReceived(const IPC::Message& msg) {
  if (IPC_MESSAGE_CLASS(msg) != ServiceWorkerMsgStart)
    return false;
  int thread_id = 0;
  bool result = PickleIterator(msg).ReadInt(&thread_id);
  DCHECK(result);
  base::Closure closure =
      base::Bind(&ServiceWorkerMessageFilter::DispatchMessage, this, msg);
  if (!thread_id) {
    main_thread_loop_proxy_->PostTask(FROM_HERE, closure);
    return true;
  }
  WorkerTaskRunner::Instance()->PostTask(thread_id, closure);
  return true;
}

void ServiceWorkerMessageFilter::DispatchMessage(const IPC::Message& msg) {
  ServiceWorkerDispatcher::ThreadSpecificInstance(thread_safe_sender_.get())
      ->OnMessageReceived(msg);
}

}  // namespace content
