// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/resource_request_info_impl.h"

#include "content/browser/worker_host/worker_service_impl.h"
#include "content/common/net/url_request_user_data.h"
#include "content/public/browser/global_request_id.h"
#include "net/url_request/url_request.h"
#include "webkit/blob/blob_data.h"

namespace content {

// ----------------------------------------------------------------------------
// ResourceRequestInfo

// static
const ResourceRequestInfo* ResourceRequestInfo::ForRequest(
    const net::URLRequest* request) {
  return ResourceRequestInfoImpl::ForRequest(request);
}

// static
void ResourceRequestInfo::AllocateForTesting(
    net::URLRequest* request,
    ResourceType::Type resource_type,
    ResourceContext* context,
    int render_process_id,
    int render_view_id) {
  ResourceRequestInfoImpl* info =
      new ResourceRequestInfoImpl(
          PROCESS_TYPE_RENDERER,             // process_type
          render_process_id,                 // child_id
          render_view_id,                    // route_id
          0,                                 // origin_pid
          0,                                 // request_id
          resource_type == ResourceType::MAIN_FRAME,  // is_main_frame
          0,                                 // frame_id
          false,                             // parent_is_main_frame
          0,                                 // parent_frame_id
          resource_type,                     // resource_type
          PAGE_TRANSITION_LINK,              // transition_type
          false,                             // is_download
          false,                             // is_stream
          true,                              // allow_download
          false,                             // has_user_gesture
          WebKit::WebReferrerPolicyDefault,  // referrer_policy
          context,                           // context
          false);                            // is_async
  info->AssociateWithRequest(request);
}

// static
bool ResourceRequestInfo::GetRenderViewForRequest(
    const net::URLRequest* request,
    int* render_process_id,
    int* render_view_id) {
  URLRequestUserData* user_data = static_cast<URLRequestUserData*>(
      request->GetUserData(URLRequestUserData::kUserDataKey));
  if (!user_data)
    return false;
  *render_process_id = user_data->render_process_id();
  *render_view_id = user_data->render_view_id();
  return true;
}

// ----------------------------------------------------------------------------
// ResourceRequestInfoImpl

// static
ResourceRequestInfoImpl* ResourceRequestInfoImpl::ForRequest(
    net::URLRequest* request) {
  return static_cast<ResourceRequestInfoImpl*>(request->GetUserData(NULL));
}

// static
const ResourceRequestInfoImpl* ResourceRequestInfoImpl::ForRequest(
    const net::URLRequest* request) {
  return ForRequest(const_cast<net::URLRequest*>(request));
}

ResourceRequestInfoImpl::ResourceRequestInfoImpl(
    ProcessType process_type,
    int child_id,
    int route_id,
    int origin_pid,
    int request_id,
    bool is_main_frame,
    int64 frame_id,
    bool parent_is_main_frame,
    int64 parent_frame_id,
    ResourceType::Type resource_type,
    PageTransition transition_type,
    bool is_download,
    bool is_stream,
    bool allow_download,
    bool has_user_gesture,
    WebKit::WebReferrerPolicy referrer_policy,
    ResourceContext* context,
    bool is_async)
    : cross_site_handler_(NULL),
      process_type_(process_type),
      child_id_(child_id),
      route_id_(route_id),
      origin_pid_(origin_pid),
      request_id_(request_id),
      is_main_frame_(is_main_frame),
      frame_id_(frame_id),
      parent_is_main_frame_(parent_is_main_frame),
      parent_frame_id_(parent_frame_id),
      is_download_(is_download),
      is_stream_(is_stream),
      allow_download_(allow_download),
      has_user_gesture_(has_user_gesture),
      was_ignored_by_handler_(false),
      resource_type_(resource_type),
      transition_type_(transition_type),
      memory_cost_(0),
      referrer_policy_(referrer_policy),
      context_(context),
      is_async_(is_async) {
}

ResourceRequestInfoImpl::~ResourceRequestInfoImpl() {
}

ResourceContext* ResourceRequestInfoImpl::GetContext() const {
  return context_;
}

int ResourceRequestInfoImpl::GetChildID() const {
  return child_id_;
}

int ResourceRequestInfoImpl::GetRouteID() const {
  return route_id_;
}

int ResourceRequestInfoImpl::GetOriginPID() const {
  return origin_pid_;
}

int ResourceRequestInfoImpl::GetRequestID() const {
  return request_id_;
}

bool ResourceRequestInfoImpl::IsMainFrame() const {
  return is_main_frame_;
}

int64 ResourceRequestInfoImpl::GetFrameID() const {
  return frame_id_;
}

bool ResourceRequestInfoImpl::ParentIsMainFrame() const {
  return parent_is_main_frame_;
}

int64 ResourceRequestInfoImpl::GetParentFrameID() const {
  return parent_frame_id_;
}

ResourceType::Type ResourceRequestInfoImpl::GetResourceType() const {
  return resource_type_;
}

WebKit::WebReferrerPolicy ResourceRequestInfoImpl::GetReferrerPolicy() const {
  return referrer_policy_;
}

PageTransition ResourceRequestInfoImpl::GetPageTransition() const {
  return transition_type_;
}

bool ResourceRequestInfoImpl::HasUserGesture() const {
  return has_user_gesture_;
}

bool ResourceRequestInfoImpl::WasIgnoredByHandler() const {
  return was_ignored_by_handler_;
}

bool ResourceRequestInfoImpl::GetAssociatedRenderView(
    int* render_process_id,
    int* render_view_id) const {
  // If the request is from the worker process, find a content that owns the
  // worker.
  if (process_type_ == PROCESS_TYPE_WORKER) {
    // Need to display some related UI for this network request - pick an
    // arbitrary parent to do so.
    if (!WorkerServiceImpl::GetInstance()->GetRendererForWorker(
            child_id_, render_process_id, render_view_id)) {
      *render_process_id = -1;
      *render_view_id = -1;
      return false;
    }
  } else {
    *render_process_id = child_id_;
    *render_view_id = route_id_;
  }
  return true;
}

bool ResourceRequestInfoImpl::IsAsync() const {
  return is_async_;
}

void ResourceRequestInfoImpl::AssociateWithRequest(net::URLRequest* request) {
  request->SetUserData(NULL, this);
  int render_process_id;
  int render_view_id;
  if (GetAssociatedRenderView(&render_process_id, &render_view_id)) {
    request->SetUserData(
        URLRequestUserData::kUserDataKey,
        new URLRequestUserData(render_process_id, render_view_id));
  }
}

GlobalRequestID ResourceRequestInfoImpl::GetGlobalRequestID() const {
  return GlobalRequestID(child_id_, request_id_);
}

void ResourceRequestInfoImpl::set_requested_blob_data(
    webkit_blob::BlobData* data) {
  requested_blob_data_ = data;
}

}  // namespace content
