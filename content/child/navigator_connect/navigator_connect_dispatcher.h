// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_NAVIGATOR_CONNECT_NAVIGATOR_CONNECT_DISPATCHER_H_
#define CONTENT_CHILD_NAVIGATOR_CONNECT_NAVIGATOR_CONNECT_DISPATCHER_H_

#include "content/child/worker_thread_message_filter.h"

namespace content {

// Receives IPC messages from the browser process and dispatches them to the
// correct thread specific NavigatorConnectProvider.
class NavigatorConnectDispatcher : public WorkerThreadMessageFilter {
 public:
  explicit NavigatorConnectDispatcher(ThreadSafeSender* thread_safe_sender);

 private:
  ~NavigatorConnectDispatcher() override;

  // WorkerThreadMessageFilter:
  bool ShouldHandleMessage(const IPC::Message& msg) const override;
  void OnFilteredMessageReceived(const IPC::Message& msg) override;
  bool GetWorkerThreadIdForMessage(const IPC::Message& msg,
                                   int* ipc_thread_id) override;

  DISALLOW_COPY_AND_ASSIGN(NavigatorConnectDispatcher);
};

}  // namespace content

#endif  // CONTENT_CHILD_NAVIGATOR_CONNECT_NAVIGATOR_CONNECT_DISPATCHER_H_
