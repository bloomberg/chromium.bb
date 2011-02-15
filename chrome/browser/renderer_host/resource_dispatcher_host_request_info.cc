// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_host/resource_dispatcher_host_request_info.h"

#include "chrome/browser/renderer_host/resource_handler.h"
#include "chrome/browser/ssl/ssl_client_auth_handler.h"
#include "chrome/browser/ui/login/login_prompt.h"
#include "webkit/blob/blob_data.h"

ResourceDispatcherHostRequestInfo::ResourceDispatcherHostRequestInfo(
    ResourceHandler* handler,
    ChildProcessInfo::ProcessType process_type,
    int child_id,
    int route_id,
    int request_id,
    ResourceType::Type resource_type,
    uint64 upload_size,
    bool is_download,
    bool allow_download,
    bool has_user_gesture,
    int host_renderer_id,
    int host_render_view_id)
    : resource_handler_(handler),
      cross_site_handler_(NULL),
      process_type_(process_type),
      child_id_(child_id),
      route_id_(route_id),
      request_id_(request_id),
      pending_data_count_(0),
      is_download_(is_download),
      allow_download_(allow_download),
      has_user_gesture_(has_user_gesture),
      pause_count_(0),
      resource_type_(resource_type),
      replace_extension_localization_templates_(false),
      last_load_state_(net::LOAD_STATE_IDLE),
      upload_size_(upload_size),
      last_upload_position_(0),
      waiting_for_upload_progress_ack_(false),
      memory_cost_(0),
      is_paused_(false),
      called_on_response_started_(false),
      has_started_reading_(false),
      paused_read_bytes_(0),
      host_renderer_id_(host_renderer_id),
      host_render_view_id_(host_render_view_id) {
}

ResourceDispatcherHostRequestInfo::~ResourceDispatcherHostRequestInfo() {
  resource_handler_->OnRequestClosed();
}

void ResourceDispatcherHostRequestInfo::set_login_handler(LoginHandler* lh) {
  login_handler_ = lh;
}

void ResourceDispatcherHostRequestInfo::set_ssl_client_auth_handler(
    SSLClientAuthHandler* s) {
  ssl_client_auth_handler_ = s;
}

void ResourceDispatcherHostRequestInfo::set_requested_blob_data(
    webkit_blob::BlobData* data) {
  requested_blob_data_ = data;
}
