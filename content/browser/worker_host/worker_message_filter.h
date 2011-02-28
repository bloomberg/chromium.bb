// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WORKER_HOST_WORKER_MESSAGE_FILTER_H_
#define CONTENT_BROWSER_WORKER_HOST_WORKER_MESSAGE_FILTER_H_

#include "base/callback.h"
#include "content/browser/browser_message_filter.h"

class ResourceDispatcherHost;
class URLRequestContextGetter;
struct ViewHostMsg_CreateWorker_Params;

class WorkerMessageFilter : public BrowserMessageFilter {
 public:
  // |next_routing_id| is owned by this object.  It can be used up until
  // OnChannelClosing.
  WorkerMessageFilter(
      int render_process_id,
      URLRequestContextGetter* request_context,
      ResourceDispatcherHost* resource_dispatcher_host,
      CallbackWithReturnValue<int>::Type* next_routing_id);

  // BrowserMessageFilter implementation.
  virtual void OnChannelClosing();
  virtual bool OnMessageReceived(const IPC::Message& message,
                                 bool* message_was_ok);

  int GetNextRoutingID();
  int render_process_id() const { return render_process_id_; }
  ResourceDispatcherHost* resource_dispatcher_host() const {
    return resource_dispatcher_host_;
  }

 private:
  ~WorkerMessageFilter();

  // Message handlers.
  void OnCreateWorker(const ViewHostMsg_CreateWorker_Params& params,
                      int* route_id);
  void OnLookupSharedWorker(const ViewHostMsg_CreateWorker_Params& params,
                           bool* exists,
                           int* route_id,
                           bool* url_error);
  void OnCancelCreateDedicatedWorker(int route_id);
  void OnForwardToWorker(const IPC::Message& message);
  void OnDocumentDetached(unsigned long long document_id);
  void OnCreateMessagePort(int* route_id, int* message_port_id);

  int render_process_id_;
  scoped_refptr<URLRequestContextGetter> request_context_;
  ResourceDispatcherHost* resource_dispatcher_host_;

  // This is guaranteed to be valid until OnChannelClosing is closed, and it's
  // not used after.
  scoped_ptr<CallbackWithReturnValue<int>::Type> next_routing_id_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(WorkerMessageFilter);
};

#endif  // CONTENT_BROWSER_WORKER_HOST_WORKER_MESSAGE_FILTER_H_
