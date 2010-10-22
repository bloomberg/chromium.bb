// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_host/resource_request_details.h"


ResourceRequestDetails::ResourceRequestDetails(const URLRequest* request,
                                               int cert_id)
    : url_(request->url()),
      original_url_(request->original_url()),
      method_(request->method()),
      referrer_(request->referrer()),
      has_upload_(request->has_upload()),
      load_flags_(request->load_flags()),
      status_(request->status()),
      ssl_cert_id_(cert_id),
      ssl_cert_status_(request->ssl_info().cert_status) {
  const ResourceDispatcherHostRequestInfo* info =
      ResourceDispatcherHost::InfoForRequest(request);
  DCHECK(info);
  resource_type_ = info->resource_type();
  frame_origin_ = info->frame_origin();
  main_frame_origin_ = info->main_frame_origin();

  // If request is from the worker process on behalf of a renderer, use
  // the renderer process id, since it consumes the notification response
  // such as ssl state etc.
  const WorkerProcessHost::WorkerInstance* worker_instance =
      WorkerService::GetInstance()->FindWorkerInstance(info->child_id());
  if (worker_instance) {
    DCHECK(!worker_instance->worker_document_set()->IsEmpty());
    const WorkerDocumentSet::DocumentInfoSet& parents =
        worker_instance->worker_document_set()->documents();
    // TODO(atwilson): need to notify all associated renderers in the case
    // of ssl state change (http://crbug.com/25357). For now, just notify
    // the first one (works for dedicated workers and shared workers with
    // a single process).
    origin_child_id_ = parents.begin()->renderer_id();
  } else {
    origin_child_id_ = info->child_id();
  }
}

ResourceRequestDetails::~ResourceRequestDetails() {}

ResourceRedirectDetails::ResourceRedirectDetails(const URLRequest* request,
                                                 int cert_id,
                                                 const GURL& new_url)
    : ResourceRequestDetails(request, cert_id),
      new_url_(new_url) {
}

ResourceRedirectDetails::~ResourceRedirectDetails() {}
