// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/sync_resource_handler.h"

#include "base/logging.h"
#include "content/browser/debugger/devtools_netlog_observer.h"
#include "content/browser/renderer_host/global_request_id.h"
#include "content/browser/renderer_host/resource_dispatcher_host.h"
#include "content/browser/renderer_host/resource_message_filter.h"
#include "content/common/resource_messages.h"
#include "content/public/browser/resource_dispatcher_host_delegate.h"
#include "net/base/io_buffer.h"
#include "net/http/http_response_headers.h"

SyncResourceHandler::SyncResourceHandler(
    ResourceMessageFilter* filter,
    const GURL& url,
    IPC::Message* result_message,
    ResourceDispatcherHost* resource_dispatcher_host)
    : read_buffer_(new net::IOBuffer(kReadBufSize)),
      filter_(filter),
      result_message_(result_message),
      rdh_(resource_dispatcher_host) {
  result_.final_url = url;
}

SyncResourceHandler::~SyncResourceHandler() {
}

bool SyncResourceHandler::OnUploadProgress(int request_id,
                                           uint64 position,
                                           uint64 size) {
  return true;
}

bool SyncResourceHandler::OnRequestRedirected(
    int request_id,
    const GURL& new_url,
    content::ResourceResponse* response,
    bool* defer) {
  net::URLRequest* request = rdh_->GetURLRequest(
      GlobalRequestID(filter_->child_id(), request_id));
  if (rdh_->delegate())
    rdh_->delegate()->OnRequestRedirected(request, response, filter_);

  DevToolsNetLogObserver::PopulateResponseInfo(request, response);
  // TODO(darin): It would be much better if this could live in WebCore, but
  // doing so requires API changes at all levels.  Similar code exists in
  // WebCore/platform/network/cf/ResourceHandleCFNet.cpp :-(
  if (new_url.GetOrigin() != result_.final_url.GetOrigin()) {
    LOG(ERROR) << "Cross origin redirect denied";
    return false;
  }
  result_.final_url = new_url;
  return true;
}

bool SyncResourceHandler::OnResponseStarted(
    int request_id,
    content::ResourceResponse* response) {
  net::URLRequest* request = rdh_->GetURLRequest(
      GlobalRequestID(filter_->child_id(), request_id));
  if (rdh_->delegate())
    rdh_->delegate()->OnResponseStarted(request, response, filter_);

  DevToolsNetLogObserver::PopulateResponseInfo(request, response);

  // We don't care about copying the status here.
  result_.headers = response->headers;
  result_.mime_type = response->mime_type;
  result_.charset = response->charset;
  result_.download_file_path = response->download_file_path;
  result_.request_time = response->request_time;
  result_.response_time = response->response_time;
  result_.connection_id = response->connection_id;
  result_.connection_reused = response->connection_reused;
  result_.load_timing = response->load_timing;
  result_.devtools_info = response->devtools_info;
  return true;
}

bool SyncResourceHandler::OnWillStart(int request_id,
                                      const GURL& url,
                                      bool* defer) {
  return true;
}

bool SyncResourceHandler::OnWillRead(int request_id, net::IOBuffer** buf,
                                     int* buf_size, int min_size) {
  DCHECK(min_size == -1);
  *buf = read_buffer_.get();
  *buf_size = kReadBufSize;
  return true;
}

bool SyncResourceHandler::OnReadCompleted(int request_id, int* bytes_read) {
  if (!*bytes_read)
    return true;
  result_.data.append(read_buffer_->data(), *bytes_read);
  return true;
}

bool SyncResourceHandler::OnResponseCompleted(
    int request_id,
    const net::URLRequestStatus& status,
    const std::string& security_info) {
  result_.status = status;

  net::URLRequest* request = rdh_->GetURLRequest(
      GlobalRequestID(filter_->child_id(), request_id));
  result_.encoded_data_length =
      DevToolsNetLogObserver::GetAndResetEncodedDataLength(request);

  ResourceHostMsg_SyncLoad::WriteReplyParams(result_message_, result_);
  filter_->Send(result_message_);
  result_message_ = NULL;
  return true;
}

void SyncResourceHandler::OnRequestClosed() {
  if (!result_message_)
    return;

  result_message_->set_reply_error();
  filter_->Send(result_message_);
}
