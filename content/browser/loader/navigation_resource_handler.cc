// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/navigation_resource_handler.h"

#include <memory>

#include "base/bind.h"
#include "base/logging.h"
#include "base/optional.h"
#include "base/values.h"
#include "content/browser/loader/navigation_url_loader_impl_core.h"
#include "content/browser/loader/resource_controller.h"
#include "content/browser/loader/resource_loader.h"
#include "content/browser/loader/resource_request_info_impl.h"
#include "content/browser/resource_context_impl.h"
#include "content/browser/streams/stream.h"
#include "content/browser/streams/stream_context.h"
#include "content/public/browser/resource_dispatcher_host_delegate.h"
#include "content/public/browser/stream_handle.h"
#include "content/public/common/resource_response.h"
#include "net/base/net_errors.h"
#include "net/http/transport_security_state.h"
#include "net/ssl/ssl_info.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "url/gurl.h"

namespace content {

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
    core_->NotifyRequestFailed(false, net::ERR_ABORTED, base::nullopt);
    DetachFromCore();
  }
}

void NavigationResourceHandler::Cancel() {
  if (core_) {
    DetachFromCore();
    if (has_controller()) {
      LayeredResourceHandler::Cancel();
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
    controller->Cancel();
    return;
  }

  response->head.encoded_data_length = request()->GetTotalReceivedBytes();
  core_->NotifyRequestRedirected(redirect_info, response);

  HoldController(std::move(controller));
  response_ = response;
  redirect_info_ = std::make_unique<net::RedirectInfo>(redirect_info);
}

void NavigationResourceHandler::OnResponseStarted(
    ResourceResponse* response,
    std::unique_ptr<ResourceController> controller) {
  DCHECK(!has_controller());

  // The UI thread already cancelled the navigation. Do not proceed.
  if (!core_) {
    controller->Cancel();
    return;
  }

  ResourceRequestInfoImpl* info = GetRequestInfo();

  response->head.encoded_data_length = request()->raw_header_size();

  base::Value navigation_data;
  if (resource_dispatcher_host_delegate_) {
    // Ask the embedder for a NavigationData instance.
    navigation_data =
        resource_dispatcher_host_delegate_->GetNavigationData(request());
  }

  core_->NotifyResponseStarted(
      response, std::move(stream_handle_), request()->ssl_info(),
      std::move(navigation_data), info->GetGlobalRequestID(),
      info->IsDownload(), info->is_stream());
  HoldController(std::move(controller));
  response_ = response;
}

void NavigationResourceHandler::OnResponseCompleted(
    const net::URLRequestStatus& status,
    std::unique_ptr<ResourceController> controller) {
  if (core_) {
    int net_error = status.error();
    DCHECK_NE(net::OK, net_error);

    base::Optional<net::SSLInfo> ssl_info;
    if (net::IsCertStatusError(request()->ssl_info().cert_status)) {
      ssl_info = request()->ssl_info();
    }

    core_->NotifyRequestFailed(request()->response_info().was_cached, net_error,
                               ssl_info);
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
