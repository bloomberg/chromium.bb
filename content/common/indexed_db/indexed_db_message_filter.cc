// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/indexed_db/indexed_db_message_filter.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/message_loop_proxy.h"
#include "content/common/indexed_db/indexed_db_dispatcher.h"
#include "content/common/indexed_db/indexed_db_messages.h"
#include "webkit/glue/worker_task_runner.h"

using webkit_glue::WorkerTaskRunner;

IndexedDBMessageFilter::IndexedDBMessageFilter() :
    main_thread_loop_proxy_(base::MessageLoopProxy::current()) {
}

bool IndexedDBMessageFilter::OnMessageReceived(const IPC::Message& msg) {
  if (IPC_MESSAGE_CLASS(msg) != IndexedDBMsgStart)
    return false;
  int thread_id = IPC::MessageIterator(msg).NextInt();
  base::Closure closure = base::Bind(
      &IndexedDBMessageFilter::DispatchMessage, this, msg);
  if (thread_id)
    WorkerTaskRunner::Instance()->PostTask(thread_id, closure);
  else
    main_thread_loop_proxy_->PostTask(FROM_HERE, closure);
  return true;
}

IndexedDBMessageFilter::~IndexedDBMessageFilter() {}

void IndexedDBMessageFilter::DispatchMessage(const IPC::Message& msg) {
  IndexedDBDispatcher::ThreadSpecificInstance()->OnMessageReceived(msg);
}
