// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/quota_message_filter.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/pickle.h"
#include "content/child/quota_dispatcher.h"
#include "content/child/thread_safe_sender.h"
#include "content/common/quota_messages.h"
#include "webkit/child/worker_task_runner.h"

using webkit_glue::WorkerTaskRunner;

namespace content {

QuotaMessageFilter::QuotaMessageFilter(
    ThreadSafeSender* thread_safe_sender)
    : main_thread_loop_proxy_(base::MessageLoopProxy::current()),
      thread_safe_sender_(thread_safe_sender) {
}

bool QuotaMessageFilter::OnMessageReceived(const IPC::Message& msg) {
  if (IPC_MESSAGE_CLASS(msg) != QuotaMsgStart)
    return false;
  int request_id = -1;
  bool result = PickleIterator(msg).ReadInt(&request_id);
  DCHECK(result);
  base::Closure closure = base::Bind(
      &QuotaMessageFilter::DispatchMessage, this, msg);
  int thread_id = 0;
  {
    base::AutoLock lock(request_id_map_lock_);
    RequestIdToThreadId::iterator found = request_id_map_.find(request_id);
    if (found != request_id_map_.end()) {
      thread_id = found->second;
      request_id_map_.erase(found);
    }
  }
  if (!thread_id) {
    main_thread_loop_proxy_->PostTask(FROM_HERE, closure);
    return true;
  }
  WorkerTaskRunner::Instance()->PostTask(thread_id, closure);
  return true;
}

void QuotaMessageFilter::RegisterRequestID(int request_id, int thread_id) {
  base::AutoLock lock(request_id_map_lock_);
  request_id_map_[request_id] = thread_id;
}

QuotaMessageFilter::~QuotaMessageFilter() {}

void QuotaMessageFilter::DispatchMessage(const IPC::Message& msg) {
  QuotaDispatcher::ThreadSpecificInstance(thread_safe_sender_.get(), this)
      ->OnMessageReceived(msg);
}

}  // namespace content
