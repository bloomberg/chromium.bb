// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/navigation_resource_handler.h"

#include <memory>

#include "base/bind.h"
#include "base/logging.h"
#include "content/browser/loader/navigation_url_loader_impl_core.h"
#include "content/browser/loader/netlog_observer.h"
#include "content/browser/loader/resource_controller.h"
#include "content/browser/loader/resource_loader.h"
#include "content/browser/loader/resource_request_info_impl.h"
#include "content/browser/resource_context_impl.h"
#include "content/browser/streams/stream.h"
#include "content/browser/streams/stream_context.h"
#include "content/public/browser/navigation_data.h"
#include "content/public/browser/resource_dispatcher_host_delegate.h"
#include "content/public/browser/ssl_status.h"
#include "content/public/browser/stream_handle.h"
#include "content/public/common/resource_response.h"
#include "net/base/net_errors.h"
#include "net/url_request/url_request.h"

namespace content {

void NavigationResourceHandler::GetSSLStatusForRequest(
    const net::SSLInfo& ssl_info,
    SSLStatus* ssl_status) {
  DCHECK(ssl_info.cert);
  *ssl_status = SSLStatus(ssl_info);
}

NavigationResourceHandler::NavigationResourceHandler(
    net::URLRequest* request,
    std::unique_ptr<ResourceHandler> next_handler,
    NavigationURLLoaderImplCore* core,
    ResourceDispatcherHostDelegate* resource_dispatcher_host_delegate,
    std::unique_ptr<StreamHandle> stream_handle)
    : LayeredResourceHandler(request, std::move(next_handler)),
      core_(core),
      stream_handle_(std::move(stream_handle)),
      resource_dispatcher_host_delegate_(resource_dispatcher_host_delegate) {
  core_->set_resource_handler(this);
}

NavigationResourceHandler::~NavigationResourceHandler() {
  if (core_) {
    core_->NotifyRequestFailed(false, net::ERR_ABORTED);
    DetachFromCore();
  }
}

void NavigationResourceHandler::Cancel() {
  if (core_) {
    DetachFromCore();
    if (has_controller()) {
      CancelAndIgnore();
    } else {
      OutOfBandCancel(net::ERR_ABORTED, true /* tell_renderer */);
    }
  }
}

void NavigationResourceHandler::FollowRedirect() {
  DCHECK(response_);
  DCHECK(redirect_info_);
  DCHECK(has_controller());
  next_handler_->OnRequestRedirected(*redirect_info_, response_.get(),
                                     ReleaseController());
  response_ = nullptr;
  redirect_info_ = nullptr;
}

void NavigationResourceHandler::ProceedWithResponse() {
  DCHECK(response_);
  DCHECK(has_controller());
  // Detach from the loader; at this point, the request is now owned by the
  // StreamHandle sent in OnResponseStarted.
  DetachFromCore();
  next_handler_->OnResponseStarted(response_.get(), ReleaseController());
  response_ = nullptr;
}

void NavigationResourceHandler::OnRequestRedirected(
    const net::RedirectInfo& redirect_info,
    ResourceResponse* response,
    std::unique_ptr<ResourceController> controller) {
  DCHECK(!has_controller());

  // The UI thread already cancelled the navigation. Do not proceed.
  if (!core_) {
    controller->CancelAndIgnore();
    return;
  }

  NetLogObserver::PopulateResponseInfo(request(), response);
  response->head.encoded_data_length = request()->GetTotalReceivedBytes();
  core_->NotifyRequestRedirected(redirect_info, response);

  HoldController(std::move(controller));
  response_ = response;
  redirect_info_ = base::MakeUnique<net::RedirectInfo>(redirect_info);
}

void NavigationResourceHandler::OnResponseStarted(
    ResourceResponse* response,
    std::unique_ptr<ResourceController> controller) {
  DCHECK(!has_controller());

  // The UI thread already cancelled the navigation. Do not proceed.
  if (!core_) {
    controller->CancelAndIgnore();
    return;
  }

  ResourceRequestInfoImpl* info = GetRequestInfo();

  NetLogObserver::PopulateResponseInfo(request(), response);
  response->head.encoded_data_length = request()->raw_header_size();

  std::unique_ptr<NavigationData> cloned_data;
  if (resource_dispatcher_host_delegate_) {
    // Ask the embedder for a NavigationData instance.
    NavigationData* navigation_data =
        resource_dispatcher_host_delegate_->GetNavigationData(request());

    // Clone the embedder's NavigationData before moving it to the UI thread.
    if (navigation_data)
      cloned_data = navigation_data->Clone();
  }

  SSLStatus ssl_status;
  if (request()->ssl_info().cert.get())
    GetSSLStatusForRequest(request()->ssl_info(), &ssl_status);

  core_->NotifyResponseStarted(
      response, std::move(stream_handle_), ssl_status, std::move(cloned_data),
      info->GetGlobalRequestID(), info->IsDownload(), info->is_stream());
  HoldController(std::move(controller));
  response_ = response;
}

void NavigationResourceHandler::OnResponseCompleted(
    const net::URLRequestStatus& status,
    std::unique_ptr<ResourceController> controller) {
  if (core_) {
    DCHECK_NE(net::OK, status.error());
    core_->NotifyRequestFailed(request()->response_info().was_cached,
                               status.error());
    DetachFromCore();
  }
  next_handler_->OnResponseCompleted(status, std::move(controller));
}

void NavigationResourceHandler::DetachFromCore() {
  DCHECK(core_);
  core_->set_resource_handler(nullptr);
  core_ = nullptr;
}

}  // namespace content
