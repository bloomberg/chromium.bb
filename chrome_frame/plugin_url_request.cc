// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome_frame/plugin_url_request.h"

#include "chrome/test/automation/automation_messages.h"
#include "chrome_frame/np_browser_functions.h"

PluginUrlRequest::PluginUrlRequest()
    : request_handler_(NULL),
      tab_(0),
      remote_request_id_(-1),
      post_data_len_(0),
      status_(URLRequestStatus::IO_PENDING),
      frame_busting_enabled_(false) {
}

PluginUrlRequest::~PluginUrlRequest() {
}

bool PluginUrlRequest::Initialize(PluginRequestHandler* request_handler,
    int tab,  int remote_request_id, const std::string& url,
    const std::string& method, const std::string& referrer,
    const std::string& extra_headers, net::UploadData* upload_data,
    bool enable_frame_busting) {
  request_handler_ = request_handler;
  tab_ = tab;
  remote_request_id_ = remote_request_id;
  url_ = url;
  method_ = method;
  referrer_ = referrer;
  extra_headers_ = extra_headers;

  if (upload_data) {
    // We store a pointer to UrlmonUploadDataStream and not net::UploadData
    // since UrlmonUploadDataStream implements thread safe ref counting and
    // UploadData does not.
    CComObject<UrlmonUploadDataStream>* upload_stream = NULL;
    HRESULT hr = CComObject<UrlmonUploadDataStream>::CreateInstance(
        &upload_stream);
    if (FAILED(hr)) {
      NOTREACHED();
    } else {
      post_data_len_ = upload_data->GetContentLength();
      upload_stream->AddRef();
      upload_stream->Initialize(upload_data);
      upload_data_.Attach(upload_stream);
    }
  }

  frame_busting_enabled_ = enable_frame_busting;

  return true;
}

void PluginUrlRequest::OnResponseStarted(const char* mime_type,
    const char* headers, int size, base::Time last_modified,
    const std::string& persistent_cookies,
    const std::string& redirect_url, int redirect_status) {
  const IPC::AutomationURLResponse response = {
    mime_type,
    headers ? headers : "",
    size,
    last_modified,
    persistent_cookies,
    redirect_url,
    redirect_status
  };
  request_handler_->Send(new AutomationMsg_RequestStarted(0, tab_,
      remote_request_id_, response));
}

void PluginUrlRequest::OnResponseEnd(const URLRequestStatus& status) {
  DCHECK(!status.is_io_pending());
  DCHECK(status.is_success() || status.os_error());
  request_handler_->Send(new AutomationMsg_RequestEnd(0, tab_,
      remote_request_id_, status));
}

void PluginUrlRequest::OnReadComplete(const void* buffer, int len) {
  std::string data(reinterpret_cast<const char*>(buffer), len);
  request_handler_->Send(new AutomationMsg_RequestData(0, tab_,
      remote_request_id_, data));
}

