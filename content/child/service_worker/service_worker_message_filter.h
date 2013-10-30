// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_SERVICE_WORKER_SERVICE_WORKER_MESSAGE_FILTER_H_
#define CONTENT_CHILD_SERVICE_WORKER_SERVICE_WORKER_MESSAGE_FILTER_H_

#include <map>

#include "content/common/content_export.h"
#include "ipc/ipc_channel_proxy.h"

namespace content {

class ThreadSafeSender;
class MessageLoopProxy;

class CONTENT_EXPORT ServiceWorkerMessageFilter
    : public IPC::ChannelProxy::MessageFilter {
 public:
  explicit ServiceWorkerMessageFilter(ThreadSafeSender* thread_safe_sender);

  // IPC::Listener implementation
  virtual bool OnMessageReceived(const IPC::Message& msg) OVERRIDE;

 protected:
  virtual ~ServiceWorkerMessageFilter();

 private:
  void DispatchMessage(const IPC::Message& msg);

  scoped_refptr<base::MessageLoopProxy> main_thread_loop_proxy_;
  scoped_refptr<ThreadSafeSender> thread_safe_sender_;

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerMessageFilter);
};

}  // namespace content

#endif  // CONTENT_CHILD_SERVICE_WORKER_SERVICE_WORKER_MESSAGE_FILTER_H_
