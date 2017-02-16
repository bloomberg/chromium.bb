// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SHARED_WORKER_SHARED_WORKER_MESSAGE_FILTER_H_
#define CONTENT_BROWSER_SHARED_WORKER_SHARED_WORKER_MESSAGE_FILTER_H_

#include "base/macros.h"
#include "content/browser/shared_worker/worker_storage_partition.h"
#include "content/common/content_export.h"
#include "content/public/browser/browser_message_filter.h"

class GURL;
struct ViewHostMsg_CreateWorker_Params;
struct ViewHostMsg_CreateWorker_Reply;

namespace content {
class ResourceContext;

// Handles SharedWorker related IPC messages for one renderer process by
// forwarding them to the SharedWorkerServiceImpl singleton.
class CONTENT_EXPORT SharedWorkerMessageFilter : public BrowserMessageFilter {
 public:
  using NextRoutingIDCallback = base::Callback<int(void)>;

  SharedWorkerMessageFilter(int render_process_id,
                            ResourceContext* resource_context,
                            const WorkerStoragePartition& partition,
                            const NextRoutingIDCallback& callback);

  // BrowserMessageFilter implementation.
  void OnChannelClosing() override;
  bool OnMessageReceived(const IPC::Message& message) override;

  int GetNextRoutingID();
  int render_process_id() const { return render_process_id_; }

 protected:
  // This is protected, so we can define sub classes for testing.
  ~SharedWorkerMessageFilter() override;

 private:
  // Message handlers.
  void OnCreateWorker(const ViewHostMsg_CreateWorker_Params& params,
                      ViewHostMsg_CreateWorker_Reply* reply);
  void OnRequestFileSystemAccess(int worker_route_id,
                                 const GURL& url,
                                 IPC::Message* reply_msg);
  void OnAllowIndexedDB(int worker_route_id,
                        const GURL& url,
                        const base::string16& name,
                        bool* result);

  const int render_process_id_;
  ResourceContext* const resource_context_;
  const WorkerStoragePartition partition_;
  NextRoutingIDCallback next_routing_id_callback_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(SharedWorkerMessageFilter);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SHARED_WORKER_SHARED_WORKER_MESSAGE_FILTER_H_
