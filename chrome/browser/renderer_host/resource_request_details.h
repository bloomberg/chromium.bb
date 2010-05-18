// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The ResourceRequestDetails object contains additional details about a
// resource request.  It copies many of the publicly accessible member variables
// of URLRequest, but exists on the UI thread.

#ifndef CHROME_BROWSER_RENDERER_HOST_RESOURCE_REQUEST_DETAILS_H_
#define CHROME_BROWSER_RENDERER_HOST_RESOURCE_REQUEST_DETAILS_H_

#include <string>

#include "chrome/browser/cert_store.h"
#include "chrome/browser/renderer_host/resource_dispatcher_host.h"
#include "chrome/browser/renderer_host/resource_dispatcher_host_request_info.h"
#include "chrome/browser/worker_host/worker_service.h"
#include "googleurl/src/gurl.h"
#include "net/url_request/url_request_status.h"

class URLRequest;

// Details about a resource request notification.
class ResourceRequestDetails {
 public:
  ResourceRequestDetails(const URLRequest* request, int cert_id)
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

  virtual ~ResourceRequestDetails() {}

  const GURL& url() const { return url_; }
  const GURL& original_url() const { return original_url_; }
  const std::string& method() const { return method_; }
  const std::string& referrer() const { return referrer_; }
  const std::string& frame_origin() const { return frame_origin_; }
  const std::string& main_frame_origin() const { return main_frame_origin_; }
  bool has_upload() const { return has_upload_; }
  int load_flags() const { return load_flags_; }
  int origin_child_id() const { return origin_child_id_; }
  const URLRequestStatus& status() const { return status_; }
  int ssl_cert_id() const { return ssl_cert_id_; }
  int ssl_cert_status() const { return ssl_cert_status_; }
  ResourceType::Type resource_type() const { return resource_type_; }

 private:
  GURL url_;
  GURL original_url_;
  std::string method_;
  std::string referrer_;
  std::string frame_origin_;
  std::string main_frame_origin_;
  bool has_upload_;
  int load_flags_;
  int origin_child_id_;
  URLRequestStatus status_;
  int ssl_cert_id_;
  int ssl_cert_status_;
  ResourceType::Type resource_type_;
};

// Details about a redirection of a resource request.
class ResourceRedirectDetails : public ResourceRequestDetails {
 public:
  ResourceRedirectDetails(const URLRequest* request,
                          int cert_id,
                          const GURL& new_url)
      : ResourceRequestDetails(request, cert_id),
        new_url_(new_url) {}

  // The URL to which we are being redirected.
  const GURL& new_url() const { return new_url_; }

 private:
  GURL new_url_;
};

#endif  // CHROME_BROWSER_RENDERER_HOST_RESOURCE_REQUEST_DETAILS_H_
