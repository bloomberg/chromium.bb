// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_NOTIFICATIONS_NOTIFICATION_DISPATCHER_H_
#define CONTENT_CHILD_NOTIFICATIONS_NOTIFICATION_DISPATCHER_H_

#include <map>

#include "base/memory/ref_counted.h"
#include "base/synchronization/lock.h"
#include "content/child/child_message_filter.h"

namespace base {
class MessageLoopProxy;
}

namespace content {

class ThreadSafeSender;

class NotificationDispatcher : public ChildMessageFilter {
 public:
  explicit NotificationDispatcher(ThreadSafeSender* thread_safe_sender);

  // Generates a, process-unique new notification Id mapped to |thread_id|, and
  // return the notification Id. This method can be called on any thread.
  int GenerateNotificationId(int thread_id);

 protected:
  ~NotificationDispatcher() override;

 private:
  bool ShouldHandleMessage(const IPC::Message& msg);

  // ChildMessageFilter implementation.
  base::TaskRunner* OverrideTaskRunnerForMessage(const IPC::Message& msg)
      override;
  bool OnMessageReceived(const IPC::Message& msg) override;

  scoped_refptr<base::MessageLoopProxy> main_thread_loop_proxy_;
  scoped_refptr<ThreadSafeSender> thread_safe_sender_;

  using NotificationIdToThreadId = std::map<int, int>;

  base::Lock notification_id_map_lock_;
  NotificationIdToThreadId notification_id_map_;
  int next_notification_id_;

  DISALLOW_COPY_AND_ASSIGN(NotificationDispatcher);
};

}  // namespace content

#endif  // CONTENT_CHILD_NOTIFICATIONS_NOTIFICATION_DISPATCHER_H_
