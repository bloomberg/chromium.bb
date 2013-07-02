// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/indexed_db/indexed_db_message_filter.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/pickle.h"
#include "content/child/indexed_db/indexed_db_dispatcher.h"
#include "content/child/thread_safe_sender.h"
#include "content/common/indexed_db/indexed_db_messages.h"
#include "webkit/child/worker_task_runner.h"

using webkit_glue::WorkerTaskRunner;

namespace content {

IndexedDBMessageFilter::IndexedDBMessageFilter(
    ThreadSafeSender* thread_safe_sender)
    : main_thread_loop_proxy_(base::MessageLoopProxy::current()),
      thread_safe_sender_(thread_safe_sender) {
}

bool IndexedDBMessageFilter::OnMessageReceived(const IPC::Message& msg) {
  if (IPC_MESSAGE_CLASS(msg) != IndexedDBMsgStart)
    return false;
  int ipc_thread_id = -1;
  bool result = PickleIterator(msg).ReadInt(&ipc_thread_id);
  DCHECK(result);
  base::Closure closure = base::Bind(
      &IndexedDBMessageFilter::DispatchMessage, this, msg);
  if (!ipc_thread_id) {
    main_thread_loop_proxy_->PostTask(FROM_HERE, closure);
    return true;
  }
  if (WorkerTaskRunner::Instance()->PostTask(ipc_thread_id, closure))
    return true;

  // Message for a terminated worker - perform necessary cleanup
  OnStaleMessageReceived(msg);
  return true;
}

IndexedDBMessageFilter::~IndexedDBMessageFilter() {}

void IndexedDBMessageFilter::DispatchMessage(const IPC::Message& msg) {
  IndexedDBDispatcher::ThreadSpecificInstance(thread_safe_sender_.get())
      ->OnMessageReceived(msg);
}

void IndexedDBMessageFilter::OnStaleMessageReceived(const IPC::Message& msg) {
  IPC_BEGIN_MESSAGE_MAP(IndexedDBMessageFilter, msg)
    IPC_MESSAGE_HANDLER(IndexedDBMsg_CallbacksSuccessIDBDatabase,
                        OnStaleSuccessIDBDatabase)
    IPC_MESSAGE_HANDLER(IndexedDBMsg_CallbacksUpgradeNeeded,
                        OnStaleUpgradeNeeded)
  IPC_END_MESSAGE_MAP()
}

void IndexedDBMessageFilter::OnStaleSuccessIDBDatabase(
    int32 ipc_thread_id,
    int32 ipc_callbacks_id,
    int32 ipc_database_callbacks_id,
    int32 ipc_database_id,
    const IndexedDBDatabaseMetadata& idb_metadata) {
  thread_safe_sender_->Send(
      new IndexedDBHostMsg_DatabaseClose(ipc_database_id));
}

void IndexedDBMessageFilter::OnStaleUpgradeNeeded(
    const IndexedDBMsg_CallbacksUpgradeNeeded_Params& p) {
  thread_safe_sender_->Send(
      new IndexedDBHostMsg_DatabaseClose(p.ipc_database_id));
}

}  // namespace content
