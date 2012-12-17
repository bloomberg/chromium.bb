// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/indexed_db/indexed_db_message_filter.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/message_loop_proxy.h"
#include "base/pickle.h"
#include "content/common/indexed_db/indexed_db_dispatcher.h"
#include "content/common/indexed_db/indexed_db_messages.h"
#include "webkit/glue/worker_task_runner.h"

using webkit_glue::WorkerTaskRunner;

namespace content {

IndexedDBMessageFilter::IndexedDBMessageFilter() :
    main_thread_loop_proxy_(base::MessageLoopProxy::current()) {
}

bool IndexedDBMessageFilter::OnMessageReceived(const IPC::Message& msg) {
  if (IPC_MESSAGE_CLASS(msg) != IndexedDBMsgStart)
    return false;
  int ipc_thread_id = -1;
  bool result = PickleIterator(msg).ReadInt(&ipc_thread_id);
  DCHECK(result);
  base::Closure closure = base::Bind(
      &IndexedDBMessageFilter::DispatchMessage, this, msg);
  if (ipc_thread_id)
    WorkerTaskRunner::Instance()->PostTask(ipc_thread_id, closure);
  else
    main_thread_loop_proxy_->PostTask(FROM_HERE, closure);
  return true;
}

IndexedDBMessageFilter::~IndexedDBMessageFilter() {}

void IndexedDBMessageFilter::DispatchMessage(const IPC::Message& msg) {
  IndexedDBDispatcher::ThreadSpecificInstance()->OnMessageReceived(msg);
}

}  // namespace content
