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
    const GURL& url,
    const net::SSLInfo& ssl_info,
    int child_id,
    SSLStatus* ssl_status) {
  DCHECK(ssl_info.cert);
  *ssl_status = SSLStatus(ssl_info);
}

NavigationResourceHandler::NavigationResourceHandler(
    net::URLRequest* request,
    NavigationURLLoaderImplCore* core,
    ResourceDispatcherHostDelegate* resource_dispatcher_host_delegate)
    : ResourceHandler(request),
      core_(core),
      resource_dispatcher_host_delegate_(resource_dispatcher_host_delegate) {
  core_->set_resource_handler(this);
  writer_.set_immediate_mode(true);
}

NavigationResourceHandler::~NavigationResourceHandler() {
  if (core_) {
    core_->NotifyRequestFailed(false, net::ERR_ABORTED);
    DetachFromCore();
  }
}

void NavigationResourceHandler::Cancel() {
  if (core_) {
    core_ = nullptr;
    OutOfBandCancel(net::ERR_ABORTED, true /* tell_renderer */);
  }
}

void NavigationResourceHandler::FollowRedirect() {
  Resume();
}

void NavigationResourceHandler::ProceedWithResponse() {
  // Detach from the loader; at this point, the request is now owned by the
  // StreamHandle sent in OnResponseStarted.
  DetachFromCore();
  Resume();
}

void NavigationResourceHandler::OnRequestRedirected(
    const net::RedirectInfo& redirect_info,
    ResourceResponse* response,
    std::unique_ptr<ResourceController> controller) {
  DCHECK(core_);
  DCHECK(!has_controller());

  // TODO(davidben): Perform a CSP check here, and anything else that would have
  // been done renderer-side.
  NetLogObserver::PopulateResponseInfo(request(), response);
  response->head.encoded_data_length = request()->GetTotalReceivedBytes();
  core_->NotifyRequestRedirected(redirect_info, response);

  HoldController(std::move(controller));
}

void NavigationResourceHandler::OnResponseStarted(
    ResourceResponse* response,
    std::unique_ptr<ResourceController> controller) {
  DCHECK(core_);
  DCHECK(!has_controller());

  ResourceRequestInfoImpl* info = GetRequestInfo();

  StreamContext* stream_context =
      GetStreamContextForResourceContext(info->GetContext());
  writer_.InitializeStream(
      stream_context->registry(), request()->url().GetOrigin(),
      base::Bind(&NavigationResourceHandler::OutOfBandCancel,
                 base::Unretained(this), net::ERR_ABORTED,
                 true /* tell_renderer */));

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
  if (request()->ssl_info().cert.get()) {
    GetSSLStatusForRequest(request()->url(), request()->ssl_info(),
                           info->GetChildID(), &ssl_status);
  }

  core_->NotifyResponseStarted(response, writer_.stream()->CreateHandle(),
                               ssl_status, std::move(cloned_data),
                               info->GetGlobalRequestID(), info->IsDownload(),
                               info->is_stream());
  // Don't defer stream based requests. This includes requests initiated via
  // mime type sniffing, etc.
  // TODO(ananta)
  // Make sure that the requests go through the throttle checks. Currently this
  // does not work as the InterceptingResourceHandler is above us and hence it
  // does not expect the old handler to defer the request.
  // TODO(clamy): We should also make the downloads wait on the
  // NavigationThrottle checks be performed. Similarly to streams, it doesn't
  // work because of the InterceptingResourceHandler.
  // TODO(clamy): This NavigationResourceHandler should be split in two, with
  // one part that wait on the NavigationThrottle to execute located between the
  // MIME sniffing and the ResourceThrotlle, and one part that write the
  // response to the stream being the leaf ResourceHandler.
  if (info->is_stream() || info->IsDownload()) {
    controller->Resume();
  } else {
    HoldController(std::move(controller));
  }
}

void NavigationResourceHandler::OnWillStart(
    const GURL& url,
    std::unique_ptr<ResourceController> controller) {
  DCHECK(!has_controller());
  controller->Resume();
}

bool NavigationResourceHandler::OnWillRead(scoped_refptr<net::IOBuffer>* buf,
                                           int* buf_size) {
  DCHECK(!has_controller());
  writer_.OnWillRead(buf, buf_size, -1);
  return true;
}

void NavigationResourceHandler::OnReadCompleted(
    int bytes_read,
    std::unique_ptr<ResourceController> controller) {
  DCHECK(!has_controller());
  writer_.OnReadCompleted(bytes_read,
                          base::Bind(&ResourceController::Resume,
                                     base::Passed(std::move(controller))));
}

void NavigationResourceHandler::OnResponseCompleted(
    const net::URLRequestStatus& status,
    std::unique_ptr<ResourceController> controller) {
  // If the request has already committed, close the stream and leave it as-is.
  if (writer_.stream()) {
    writer_.Finalize(status.error());
    controller->Resume();
    return;
  }

  if (core_) {
    DCHECK_NE(net::OK, status.error());
    core_->NotifyRequestFailed(request()->response_info().was_cached,
                               status.error());
    DetachFromCore();
  }
  controller->Resume();
}

void NavigationResourceHandler::OnDataDownloaded(int bytes_downloaded) {
  NOTREACHED();
}

void NavigationResourceHandler::DetachFromCore() {
  DCHECK(core_);
  core_->set_resource_handler(nullptr);
  core_ = nullptr;
}

}  // namespace content
