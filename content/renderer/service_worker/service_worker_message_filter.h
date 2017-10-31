// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_SERVICE_WORKER_SERVICE_WORKER_MESSAGE_FILTER_H_
#define CONTENT_RENDERER_SERVICE_WORKER_SERVICE_WORKER_MESSAGE_FILTER_H_

#include "base/macros.h"
#include "content/common/content_export.h"
#include "content/renderer/worker_thread_message_filter.h"

namespace content {

class CONTENT_EXPORT ServiceWorkerMessageFilter
    : public WorkerThreadMessageFilter {
 public:
  explicit ServiceWorkerMessageFilter(ThreadSafeSender* thread_safe_sender);

 protected:
  ~ServiceWorkerMessageFilter() override;

 private:
  // WorkerThreadMessageFilter:
  bool ShouldHandleMessage(const IPC::Message& msg) const override;
  void OnFilteredMessageReceived(const IPC::Message& msg) override;
  bool GetWorkerThreadIdForMessage(const IPC::Message& msg,
                                   int* ipc_thread_id) override;

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerMessageFilter);
};

}  // namespace content

#endif  // CONTENT_RENDERER_SERVICE_WORKER_SERVICE_WORKER_MESSAGE_FILTER_H_
