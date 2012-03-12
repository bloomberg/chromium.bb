// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/resource_request_details.h"

#include "content/browser/renderer_host/resource_request_info_impl.h"
#include "content/browser/worker_host/worker_service_impl.h"
#include "net/url_request/url_request.h"

using content::ResourceRequestInfoImpl;
using content::WorkerServiceImpl;

ResourceRequestDetails::ResourceRequestDetails(const net::URLRequest* request,
                                               int cert_id)
    : url_(request->url()),
      original_url_(request->original_url()),
      method_(request->method()),
      referrer_(request->referrer()),
      has_upload_(request->has_upload()),
      load_flags_(request->load_flags()),
      status_(request->status()),
      ssl_cert_id_(cert_id),
      ssl_cert_status_(request->ssl_info().cert_status),
      socket_address_(request->GetSocketAddress()) {
  const ResourceRequestInfoImpl* info =
      ResourceRequestInfoImpl::ForRequest(request);
  resource_type_ = info->GetResourceType();
  frame_id_ = info->GetFrameID();

  // If request is from the worker process on behalf of a renderer, use
  // the renderer process id, since it consumes the notification response
  // such as ssl state etc.
  // TODO(atwilson): need to notify all associated renderers in the case
  // of ssl state change (http://crbug.com/25357). For now, just notify
  // the first one (works for dedicated workers and shared workers with
  // a single process).
  int temp;
  if (!WorkerServiceImpl::GetInstance()->GetRendererForWorker(
          info->GetChildID(), &origin_child_id_, &temp)) {
    origin_child_id_ = info->GetChildID();
  }
}

ResourceRequestDetails::~ResourceRequestDetails() {}

ResourceRedirectDetails::ResourceRedirectDetails(const net::URLRequest* request,
                                                 int cert_id,
                                                 const GURL& new_url)
    : ResourceRequestDetails(request, cert_id),
      new_url_(new_url) {
}

ResourceRedirectDetails::~ResourceRedirectDetails() {}
