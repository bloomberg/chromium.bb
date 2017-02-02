// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/stream_resource_handler.h"

#include "base/bind.h"
#include "base/logging.h"
#include "content/browser/loader/resource_controller.h"
#include "net/url_request/url_request_status.h"

namespace content {

StreamResourceHandler::StreamResourceHandler(net::URLRequest* request,
                                             StreamRegistry* registry,
                                             const GURL& origin)
    : ResourceHandler(request) {
  writer_.InitializeStream(registry, origin,
                           base::Bind(&StreamResourceHandler::OutOfBandCancel,
                                      base::Unretained(this), net::ERR_ABORTED,
                                      true /* tell_renderer */));
}

StreamResourceHandler::~StreamResourceHandler() {
}

void StreamResourceHandler::OnRequestRedirected(
    const net::RedirectInfo& redirect_info,
    ResourceResponse* resp,
    std::unique_ptr<ResourceController> controller) {
  controller->Resume();
}

void StreamResourceHandler::OnResponseStarted(
    ResourceResponse* resp,
    std::unique_ptr<ResourceController> controller) {
  controller->Resume();
}

void StreamResourceHandler::OnWillStart(
    const GURL& url,
    std::unique_ptr<ResourceController> controller) {
  controller->Resume();
}

bool StreamResourceHandler::OnWillRead(scoped_refptr<net::IOBuffer>* buf,
                                       int* buf_size) {
  writer_.OnWillRead(buf, buf_size, -1);
  return true;
}

void StreamResourceHandler::OnReadCompleted(
    int bytes_read,
    std::unique_ptr<ResourceController> controller) {
  writer_.OnReadCompleted(bytes_read,
                          base::Bind(&ResourceController::Resume,
                                     base::Passed(std::move(controller))));
}

void StreamResourceHandler::OnResponseCompleted(
    const net::URLRequestStatus& status,
    std::unique_ptr<ResourceController> controller) {
  writer_.Finalize(status.error());
  controller->Resume();
}

void StreamResourceHandler::OnDataDownloaded(int bytes_downloaded) {
  NOTREACHED();
}

}  // namespace content
