// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome_frame/plugin_url_request.h"

#include "chrome/common/automation_messages.h"
#include "chrome_frame/np_browser_functions.h"

PluginUrlRequest::PluginUrlRequest()
    : delegate_(NULL),
      remote_request_id_(-1),
      post_data_len_(0),
      enable_frame_busting_(false),
      resource_type_(ResourceType::MAIN_FRAME),
      load_flags_(0),
      is_chunked_upload_(false),
      upload_data_(NULL) {
}

PluginUrlRequest::~PluginUrlRequest() {
}

bool PluginUrlRequest::Initialize(PluginUrlRequestDelegate* delegate,
    int remote_request_id, const std::string& url, const std::string& method,
    const std::string& referrer, const std::string& extra_headers,
    net::UploadData* upload_data, ResourceType::Type resource_type,
    bool enable_frame_busting, int load_flags) {
  delegate_ = delegate;
  remote_request_id_ = remote_request_id;
  url_ = url;
  method_ = method;
  referrer_ = referrer;
  extra_headers_ = extra_headers;
  resource_type_ = resource_type;
  load_flags_ = load_flags;

  if (upload_data && upload_data->GetContentLength()) {
    post_data_len_ = upload_data->GetContentLength();
    is_chunked_upload_ = upload_data->is_chunked();

#pragma warning(disable:4244)
    upload_data_.reserve(post_data_len_);
#pragma warning(default:4244)

    std::vector<net::UploadData::Element>::iterator element_index;
    for (element_index = upload_data->elements()->begin();
         element_index != upload_data->elements()->end();
         ++element_index) {
      std::copy(element_index->bytes().begin(), element_index->bytes().end(),
                std::back_inserter(upload_data_));
    }
  }
  enable_frame_busting_ = enable_frame_busting;
  return true;
}
