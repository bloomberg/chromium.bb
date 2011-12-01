// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/resource_dispatcher_host_request_info.h"

#include "content/browser/renderer_host/resource_dispatcher_host_login_delegate.h"
#include "content/browser/renderer_host/resource_handler.h"
#include "content/browser/ssl/ssl_client_auth_handler.h"
#include "webkit/blob/blob_data.h"

ResourceDispatcherHostRequestInfo::ResourceDispatcherHostRequestInfo(
    ResourceHandler* handler,
    content::ProcessType process_type,
    int child_id,
    int route_id,
    int origin_pid,
    int request_id,
    bool is_main_frame,
    int64 frame_id,
    bool parent_is_main_frame,
    int64 parent_frame_id,
    ResourceType::Type resource_type,
    content::PageTransition transition_type,
    uint64 upload_size,
    bool is_download,
    bool allow_download,
    bool has_user_gesture,
    const content::ResourceContext* context)
    : resource_handler_(handler),
      cross_site_handler_(NULL),
      process_type_(process_type),
      child_id_(child_id),
      route_id_(route_id),
      origin_pid_(origin_pid),
      request_id_(request_id),
      is_main_frame_(is_main_frame),
      frame_id_(frame_id),
      parent_is_main_frame_(parent_is_main_frame),
      parent_frame_id_(parent_frame_id),
      pending_data_count_(0),
      is_download_(is_download),
      allow_download_(allow_download),
      has_user_gesture_(has_user_gesture),
      pause_count_(0),
      resource_type_(resource_type),
      transition_type_(transition_type),
      upload_size_(upload_size),
      last_upload_position_(0),
      waiting_for_upload_progress_ack_(false),
      memory_cost_(0),
      context_(context),
      is_paused_(false),
      called_on_response_started_(false),
      has_started_reading_(false),
      paused_read_bytes_(0) {
}

ResourceDispatcherHostRequestInfo::~ResourceDispatcherHostRequestInfo() {
  resource_handler_->OnRequestClosed();
}

void ResourceDispatcherHostRequestInfo::set_login_delegate(
    ResourceDispatcherHostLoginDelegate* ld) {
  login_delegate_ = ld;
}

void ResourceDispatcherHostRequestInfo::set_ssl_client_auth_handler(
    SSLClientAuthHandler* s) {
  ssl_client_auth_handler_ = s;
}

void ResourceDispatcherHostRequestInfo::set_requested_blob_data(
    webkit_blob::BlobData* data) {
  requested_blob_data_ = data;
}
