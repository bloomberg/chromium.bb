// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_QUOTA_MESSAGE_FILTER_H_
#define CONTENT_CHILD_QUOTA_MESSAGE_FILTER_H_

#include <map>

#include "base/synchronization/lock.h"
#include "ipc/ipc_channel_proxy.h"

namespace base {
class MessageLoopProxy;
}

namespace content {

class ThreadSafeSender;

class QuotaMessageFilter : public IPC::ChannelProxy::MessageFilter {
 public:
  explicit QuotaMessageFilter(ThreadSafeSender* thread_safe_sender);

  // IPC::Listener implementation.
  virtual bool OnMessageReceived(const IPC::Message& msg) OVERRIDE;

  // Registers { request_id, thread_id } map to the message filter.
  // This method can be called on any thread.
  void RegisterRequestID(int request_id, int thread_id);

 protected:
  virtual ~QuotaMessageFilter();

 private:
  typedef std::map<int, int> RequestIdToThreadId;

  void DispatchMessage(const IPC::Message& msg);

  scoped_refptr<base::MessageLoopProxy> main_thread_loop_proxy_;
  scoped_refptr<ThreadSafeSender> thread_safe_sender_;

  base::Lock request_id_map_lock_;
  RequestIdToThreadId request_id_map_;

  DISALLOW_COPY_AND_ASSIGN(QuotaMessageFilter);
};

}  // namespace content

#endif  // CONTENT_CHILD_QUOTA_MESSAGE_FILTER_H_
