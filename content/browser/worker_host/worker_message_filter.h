// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WORKER_HOST_WORKER_MESSAGE_FILTER_H_
#define CONTENT_BROWSER_WORKER_HOST_WORKER_MESSAGE_FILTER_H_

#include "base/callback.h"
#include "content/browser/worker_host/worker_storage_partition.h"
#include "content/public/browser/browser_message_filter.h"

class ResourceDispatcherHost;
struct ViewHostMsg_CreateWorker_Params;

namespace content {
class ResourceContext;
}  // namespace content

class WorkerMessageFilter : public content::BrowserMessageFilter {
 public:
  typedef base::Callback<int(void)> NextRoutingIDCallback;

  // |next_routing_id| is owned by this object.  It can be used up until
  // OnChannelClosing.
  WorkerMessageFilter(int render_process_id,
                      content::ResourceContext* resource_context,
                      const WorkerStoragePartition& partition,
                      const NextRoutingIDCallback& callback);

  // content::BrowserMessageFilter implementation.
  virtual void OnChannelClosing() OVERRIDE;
  virtual bool OnMessageReceived(const IPC::Message& message,
                                 bool* message_was_ok) OVERRIDE;

  int GetNextRoutingID();
  int render_process_id() const { return render_process_id_; }

 private:
  virtual ~WorkerMessageFilter();

  // Message handlers.
  void OnCreateWorker(const ViewHostMsg_CreateWorker_Params& params,
                      int* route_id);
  void OnLookupSharedWorker(const ViewHostMsg_CreateWorker_Params& params,
                           bool* exists,
                           int* route_id,
                           bool* url_error);
  void OnForwardToWorker(const IPC::Message& message);
  void OnDocumentDetached(unsigned long long document_id);
  void OnCreateMessagePort(int* route_id, int* message_port_id);

  int render_process_id_;
  content::ResourceContext* const resource_context_;
  WorkerStoragePartition partition_;

  // This is guaranteed to be valid until OnChannelClosing is closed, and it's
  // not used after.
  NextRoutingIDCallback next_routing_id_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(WorkerMessageFilter);
};

#endif  // CONTENT_BROWSER_WORKER_HOST_WORKER_MESSAGE_FILTER_H_
