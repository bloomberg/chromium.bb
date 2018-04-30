// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_NOTIFICATIONS_NOTIFICATION_DISPATCHER_H_
#define CONTENT_RENDERER_NOTIFICATIONS_NOTIFICATION_DISPATCHER_H_

#include <map>

#include "base/macros.h"
#include "base/single_thread_task_runner.h"
#include "base/synchronization/lock.h"
#include "content/renderer/worker_thread_message_filter.h"

namespace content {

class NotificationDispatcher : public WorkerThreadMessageFilter {
 public:
  NotificationDispatcher(
      ThreadSafeSender* thread_safe_sender,
      scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner);

  // Generates and stores a new process-unique notification request ID mapped to
  // |thread_id|, and returns the generated request ID. This method can be
  // called on any thread.
  int GenerateNotificationRequestId(int thread_id);

 protected:
  ~NotificationDispatcher() override;

 private:
  // WorkerThreadMessageFilter:
  bool ShouldHandleMessage(const IPC::Message& msg) const override;
  void OnFilteredMessageReceived(const IPC::Message& msg) override;
  bool GetWorkerThreadIdForMessage(const IPC::Message& msg,
                                   int* ipc_thread_id) override;

  using NotificationRequestIdToThreadId = std::map<int, int>;

  base::Lock notification_request_id_map_lock_;
  NotificationRequestIdToThreadId notification_request_id_map_;
  int next_notification_request_id_ = 0;

  DISALLOW_COPY_AND_ASSIGN(NotificationDispatcher);
};

}  // namespace content

#endif  // CONTENT_RENDERER_NOTIFICATIONS_NOTIFICATION_DISPATCHER_H_
