// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_SERVICE_WORKER_SERVICE_WORKER_MESSAGE_SENDER_H_
#define CONTENT_CHILD_SERVICE_WORKER_SERVICE_WORKER_MESSAGE_SENDER_H_

#include "base/memory/ref_counted.h"
#include "content/common/content_export.h"

namespace IPC {
class Message;
}

namespace content {

class ThreadSafeSender;

// A thin wrapper of ThreadSafeSender. Tests can inject a mock for IPC messaging
// in this layer.
class CONTENT_EXPORT ServiceWorkerMessageSender
    : public base::RefCountedThreadSafe<ServiceWorkerMessageSender> {
 public:
  explicit ServiceWorkerMessageSender(ThreadSafeSender* sender);

  virtual bool Send(IPC::Message* message);

  ThreadSafeSender* thread_safe_sender() { return thread_safe_sender_.get(); }

 protected:
  virtual ~ServiceWorkerMessageSender();

 private:
  friend class base::RefCountedThreadSafe<ServiceWorkerMessageSender>;

  scoped_refptr<ThreadSafeSender> thread_safe_sender_;

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerMessageSender);
};

}  // namespace content

#endif  // CONTENT_CHILD_SERVICE_WORKER_SERVICE_WORKER_MESSAGE_SENDER_H_
