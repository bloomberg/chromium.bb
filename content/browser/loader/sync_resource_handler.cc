// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/sync_resource_handler.h"

#include "base/callback_helpers.h"
#include "base/logging.h"
#include "content/browser/loader/resource_controller.h"
#include "content/browser/loader/resource_dispatcher_host_impl.h"
#include "content/browser/loader/resource_request_info_impl.h"
#include "content/common/resource_messages.h"
#include "content/public/browser/resource_dispatcher_host_delegate.h"
#include "content/public/browser/resource_request_info.h"
#include "net/base/io_buffer.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/redirect_info.h"

namespace content {

SyncResourceHandler::SyncResourceHandler(
    net::URLRequest* request,
    const SyncLoadResultCallback& result_handler,
    ResourceDispatcherHostImpl* resource_dispatcher_host)
    : ResourceHandler(request),
      read_buffer_(new net::IOBuffer(kReadBufSize)),
      result_handler_(result_handler),
      rdh_(resource_dispatcher_host),
      total_transfer_size_(0) {
  result_.final_url = request->url();
}

SyncResourceHandler::~SyncResourceHandler() {
  if (result_handler_)
    result_handler_.Run(nullptr);
}

void SyncResourceHandler::OnRequestRedirected(
    const net::RedirectInfo& redirect_info,
    ResourceResponse* response,
    std::unique_ptr<ResourceController> controller) {
  if (rdh_->delegate()) {
    rdh_->delegate()->OnRequestRedirected(
        redirect_info.new_url, request(), GetRequestInfo()->GetContext(),
        response);
  }

  // TODO(darin): It would be much better if this could live in WebCore, but
  // doing so requires API changes at all levels.  Similar code exists in
  // WebCore/platform/network/cf/ResourceHandleCFNet.cpp :-(
  if (redirect_info.new_url.GetOrigin() != result_.final_url.GetOrigin()) {
    LOG(ERROR) << "Cross origin redirect denied";
    controller->Cancel();
    return;
  }
  result_.final_url = redirect_info.new_url;

  total_transfer_size_ += request()->GetTotalReceivedBytes();
  controller->Resume();
}

void SyncResourceHandler::OnResponseStarted(
    ResourceResponse* response,
    std::unique_ptr<ResourceController> controller) {
  ResourceRequestInfoImpl* info = GetRequestInfo();
  DCHECK(info->requester_info()->IsRenderer());
  if (!info->requester_info()->filter()) {
    controller->Cancel();
    return;
  }

  if (rdh_->delegate()) {
    rdh_->delegate()->OnResponseStarted(request(), info->GetContext(),
                                        response);
  }

  // We don't care about copying the status here.
  result_.headers = response->head.headers;
  result_.mime_type = response->head.mime_type;
  result_.charset = response->head.charset;
  result_.download_file_path = response->head.download_file_path;
  result_.request_time = response->head.request_time;
  result_.response_time = response->head.response_time;
  result_.load_timing = response->head.load_timing;
  result_.devtools_info = response->head.devtools_info;
  result_.socket_address = response->head.socket_address;
  controller->Resume();
}

void SyncResourceHandler::OnWillStart(
    const GURL& url,
    std::unique_ptr<ResourceController> controller) {
  controller->Resume();
}

void SyncResourceHandler::OnWillRead(
    scoped_refptr<net::IOBuffer>* buf,
    int* buf_size,
    std::unique_ptr<ResourceController> controller) {
  *buf = read_buffer_.get();
  *buf_size = kReadBufSize;
  controller->Resume();
}

void SyncResourceHandler::OnReadCompleted(
    int bytes_read,
    std::unique_ptr<ResourceController> controller) {
  if (bytes_read)
    result_.data.append(read_buffer_->data(), bytes_read);
  controller->Resume();
}

void SyncResourceHandler::OnResponseCompleted(
    const net::URLRequestStatus& status,
    std::unique_ptr<ResourceController> controller) {
  result_.error_code = status.error();

  int total_transfer_size = request()->GetTotalReceivedBytes();
  result_.encoded_data_length = total_transfer_size_ + total_transfer_size;
  result_.encoded_body_length = request()->GetRawBodyBytes();

  base::ResetAndReturn(&result_handler_).Run(&result_);

  controller->Resume();
}

void SyncResourceHandler::OnDataDownloaded(int bytes_downloaded) {
  // Sync requests don't involve ResourceMsg_DataDownloaded messages
  // being sent back to renderers as progress is made.
}

}  // namespace content
