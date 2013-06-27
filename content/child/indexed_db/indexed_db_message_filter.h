// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_INDEXED_DB_INDEXED_DB_MESSAGE_FILTER_H_
#define CONTENT_CHILD_INDEXED_DB_INDEXED_DB_MESSAGE_FILTER_H_

#include "ipc/ipc_channel_proxy.h"

struct IndexedDBDatabaseMetadata;
struct IndexedDBMsg_CallbacksUpgradeNeeded_Params;

namespace base {
class MessageLoopProxy;
}  // namespace base

namespace content {
class IndexedDBDispatcher;
class ThreadSafeSender;

class IndexedDBMessageFilter : public IPC::ChannelProxy::MessageFilter {
 public:
  explicit IndexedDBMessageFilter(ThreadSafeSender* thread_safe_sender);

  // IPC::Listener implementation.
  virtual bool OnMessageReceived(const IPC::Message& msg) OVERRIDE;

 protected:
  virtual ~IndexedDBMessageFilter();

 private:
  void DispatchMessage(const IPC::Message& msg);
  void OnStaleMessageReceived(const IPC::Message& msg);
  void OnStaleSuccessIDBDatabase(int32 ipc_thread_id,
                                 int32 ipc_callbacks_id,
                                 int32 ipc_database_callbacks_id,
                                 int32 ipc_object_id,
                                 const IndexedDBDatabaseMetadata&);
  void OnStaleUpgradeNeeded(const IndexedDBMsg_CallbacksUpgradeNeeded_Params&);

  scoped_refptr<base::MessageLoopProxy> main_thread_loop_proxy_;
  scoped_refptr<ThreadSafeSender> thread_safe_sender_;

  DISALLOW_COPY_AND_ASSIGN(IndexedDBMessageFilter);
};

}  // namespace content

#endif  // CONTENT_CHILD_INDEXED_DB_INDEXED_DB_MESSAGE_FILTER_H_
