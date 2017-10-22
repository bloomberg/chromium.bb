// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_SERVICE_WORKER_SERVICE_WORKER_MESSAGE_FILTER_H_
#define CONTENT_RENDERER_SERVICE_WORKER_SERVICE_WORKER_MESSAGE_FILTER_H_

#include <set>
#include <vector>

#include "base/macros.h"
#include "content/common/content_export.h"
#include "content/renderer/worker_thread_message_filter.h"

struct ServiceWorkerMsg_MessageToDocument_Params;
struct ServiceWorkerMsg_SetControllerServiceWorker_Params;

namespace content {

struct ServiceWorkerVersionAttributes;

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

  // ChildMessageFilter:
  void OnStaleMessageReceived(const IPC::Message& msg) override;

  // Message handlers for stale messages.
  void OnStaleSetVersionAttributes(
      int thread_id,
      int registration_handle_id,
      int changed_mask,
      const ServiceWorkerVersionAttributes& attrs);
  void OnStaleSetControllerServiceWorker(
      const ServiceWorkerMsg_SetControllerServiceWorker_Params& params);
  void OnStaleMessageToDocument(
      const ServiceWorkerMsg_MessageToDocument_Params& params);

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerMessageFilter);
};

}  // namespace content

#endif  // CONTENT_RENDERER_SERVICE_WORKER_SERVICE_WORKER_MESSAGE_FILTER_H_
