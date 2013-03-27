// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/sync_resource_handler.h"

#include "base/logging.h"
#include "content/browser/devtools/devtools_netlog_observer.h"
#include "content/browser/loader/resource_dispatcher_host_impl.h"
#include "content/browser/loader/resource_message_filter.h"
#include "content/common/resource_messages.h"
#include "content/public/browser/global_request_id.h"
#include "content/public/browser/resource_dispatcher_host_delegate.h"
#include "net/base/io_buffer.h"
#include "net/http/http_response_headers.h"

namespace content {

SyncResourceHandler::SyncResourceHandler(
    ResourceMessageFilter* filter,
    net::URLRequest* request,
    IPC::Message* result_message,
    ResourceDispatcherHostImpl* resource_dispatcher_host)
    : read_buffer_(new net::IOBuffer(kReadBufSize)),
      filter_(filter),
      request_(request),
      result_message_(result_message),
      rdh_(resource_dispatcher_host) {
  result_.final_url = request_->url();
}

SyncResourceHandler::~SyncResourceHandler() {
  if (result_message_) {
    result_message_->set_reply_error();
    filter_->Send(result_message_);
  }
}

bool SyncResourceHandler::OnUploadProgress(int request_id,
                                           uint64 position,
                                           uint64 size) {
  return true;
}

bool SyncResourceHandler::OnRequestRedirected(
    int request_id,
    const GURL& new_url,
    ResourceResponse* response,
    bool* defer) {
  if (rdh_->delegate()) {
    rdh_->delegate()->OnRequestRedirected(new_url, request_,
                                          filter_->resource_context(),
                                          response);
  }

  DevToolsNetLogObserver::PopulateResponseInfo(request_, response);
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
    ResourceResponse* response,
    bool* defer) {
  if (rdh_->delegate()) {
    rdh_->delegate()->OnResponseStarted(request_, filter_->resource_context(),
                                        response, filter_);
  }

  DevToolsNetLogObserver::PopulateResponseInfo(request_, response);

  // We don't care about copying the status here.
  result_.headers = response->head.headers;
  result_.mime_type = response->head.mime_type;
  result_.charset = response->head.charset;
  result_.download_file_path = response->head.download_file_path;
  result_.request_time = response->head.request_time;
  result_.response_time = response->head.response_time;
  result_.load_timing = response->head.load_timing;
  result_.devtools_info = response->head.devtools_info;
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

bool SyncResourceHandler::OnReadCompleted(int request_id, int bytes_read,
                                          bool* defer) {
  if (!bytes_read)
    return true;
  result_.data.append(read_buffer_->data(), bytes_read);
  return true;
}

bool SyncResourceHandler::OnResponseCompleted(
    int request_id,
    const net::URLRequestStatus& status,
    const std::string& security_info) {
  result_.error_code = status.error();

  result_.encoded_data_length =
      DevToolsNetLogObserver::GetAndResetEncodedDataLength(request_);

  ResourceHostMsg_SyncLoad::WriteReplyParams(result_message_, result_);
  filter_->Send(result_message_);
  result_message_ = NULL;
  return true;
}

void SyncResourceHandler::OnDataDownloaded(
    int request_id,
    int bytes_downloaded) {
  // Sync requests don't involve ResourceMsg_DataDownloaded messages
  // being sent back to renderers as progress is made.
}

}  // namespace content
