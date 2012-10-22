// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_INDEXED_DB_INDEXED_DB_MESSAGE_FILTER_H_
#define CONTENT_COMMON_INDEXED_DB_INDEXED_DB_MESSAGE_FILTER_H_

#include "ipc/ipc_channel_proxy.h"

namespace base {
class MessageLoopProxy;
}  // namespace base

namespace content {
class IndexedDBDispatcher;

class IndexedDBMessageFilter : public IPC::ChannelProxy::MessageFilter {
 public:
  IndexedDBMessageFilter();

  // IPC::Listener implementation.
  virtual bool OnMessageReceived(const IPC::Message& msg) OVERRIDE;

 protected:
  virtual ~IndexedDBMessageFilter();

 private:
  void DispatchMessage(const IPC::Message& msg);
  scoped_refptr<base::MessageLoopProxy> main_thread_loop_proxy_;

  DISALLOW_COPY_AND_ASSIGN(IndexedDBMessageFilter);
};

}  // namespace content

#endif  // CONTENT_COMMON_INDEXED_DB_INDEXED_DB_DISPATCHER_H_
