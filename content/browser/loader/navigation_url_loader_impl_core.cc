// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/navigation_url_loader_impl_core.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/time/time.h"
#include "base/values.h"
#include "content/browser/frame_host/navigation_request_info.h"
#include "content/browser/loader/navigation_resource_handler.h"
#include "content/browser/loader/resource_dispatcher_host_impl.h"
#include "content/browser/service_worker/service_worker_navigation_handle_core.h"
#include "content/common/navigation_params.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/global_request_id.h"
#include "content/public/browser/navigation_ui_data.h"
#include "content/public/browser/ssl_status.h"
#include "content/public/browser/stream_handle.h"
#include "content/public/common/resource_response.h"
#include "net/base/net_errors.h"
#include "net/url_request/redirect_info.h"
#include "net/url_request/url_request_context_getter.h"
#include "storage/browser/fileapi/file_system_context.h"

namespace content {

NavigationURLLoaderImplCore::NavigationURLLoaderImplCore(
    const base::WeakPtr<NavigationURLLoaderImpl>& loader)
    : loader_(loader), resource_handler_(nullptr), weak_factory_(this) {
  // This object is created on the UI thread but otherwise is accessed and
  // deleted on the IO thread.
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

NavigationURLLoaderImplCore::~NavigationURLLoaderImplCore() {}

void NavigationURLLoaderImplCore::Start(
    ResourceContext* resource_context,
    net::URLRequestContextGetter* url_request_context_getter,
    storage::FileSystemContext* upload_file_system_context,
    ServiceWorkerNavigationHandleCore* service_worker_handle_core,
    AppCacheNavigationHandleCore* appcache_handle_core,
    std::unique_ptr<NavigationRequestInfo> request_info,
    std::unique_ptr<NavigationUIData> navigation_ui_data) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  base::WeakPtr<NavigationURLLoaderImplCore> weak_this =
      weak_factory_.GetWeakPtr();

  // The ResourceDispatcherHostImpl can be null in unit tests.
  if (ResourceDispatcherHostImpl::Get()) {
    ResourceDispatcherHostImpl::Get()->BeginNavigationRequest(
        resource_context, url_request_context_getter->GetURLRequestContext(),
        upload_file_system_context, *request_info,
        std::move(navigation_ui_data), this, mojom::URLLoaderClientPtr(),
        mojom::URLLoaderRequest(), service_worker_handle_core,
        appcache_handle_core);
  }

  // Careful, |this| could be destroyed at this point. Don't notify start if
  // that's the case (i.e. the request failed).
  if (!weak_this)
    return;
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::BindOnce(&NavigationURLLoaderImpl::NotifyRequestStarted, loader_,
                     base::TimeTicks::Now()));
}

void NavigationURLLoaderImplCore::FollowRedirect() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (resource_handler_)
    resource_handler_->FollowRedirect();
}

void NavigationURLLoaderImplCore::ProceedWithResponse() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (resource_handler_)
    resource_handler_->ProceedWithResponse();
}

void NavigationURLLoaderImplCore::CancelRequestIfNeeded() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (resource_handler_)
    resource_handler_->Cancel();
}

void NavigationURLLoaderImplCore::NotifyRequestRedirected(
    const net::RedirectInfo& redirect_info,
    ResourceResponse* response) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  TRACE_EVENT_ASYNC_END0("navigation", "Navigation redirectDelay", this);

  // Make a copy of the ResourceResponse before it is passed to another thread.
  //
  // TODO(davidben): This copy could be avoided if ResourceResponse weren't
  // reference counted and the loader stack passed unique ownership of the
  // response. https://crbug.com/416050
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::BindOnce(&NavigationURLLoaderImpl::NotifyRequestRedirected, loader_,
                     redirect_info, response->DeepCopy()));

  // TODO(carlosk): extend this trace to support non-PlzNavigate navigations.
  // For the trace below we're using the NavigationURLLoaderImplCore as the
  // async trace id and reporting the new redirect URL as a parameter.
  TRACE_EVENT_ASYNC_BEGIN2("navigation", "Navigation redirectDelay", this,
                           "&NavigationURLLoaderImplCore", this, "New URL",
                           redirect_info.new_url.spec());
}

void NavigationURLLoaderImplCore::NotifyResponseStarted(
    ResourceResponse* response,
    std::unique_ptr<StreamHandle> body,
    const net::SSLInfo& ssl_info,
    base::Value navigation_data,
    const GlobalRequestID& request_id,
    bool is_download,
    bool is_stream) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  TRACE_EVENT_ASYNC_END0("navigation", "Navigation redirectDelay", this);
  TRACE_EVENT_ASYNC_END2("navigation", "Navigation timeToResponseStarted", this,
                         "&NavigationURLLoaderImplCore", this, "success", true);

  // If, by the time the task reaches the UI thread, |loader_| has already been
  // destroyed, NotifyResponseStarted will not run. |body| will be destructed
  // and the request released at that point.

  // Make a copy of the ResourceResponse before it is passed to another thread.
  //
  // TODO(davidben): This copy could be avoided if ResourceResponse weren't
  // reference counted and the loader stack passed unique ownership of the
  // response. https://crbug.com/416050
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::BindOnce(&NavigationURLLoaderImpl::NotifyResponseStarted, loader_,
                     response->DeepCopy(), base::Passed(&body), ssl_info,
                     std::move(navigation_data), request_id, is_download,
                     is_stream));
}

void NavigationURLLoaderImplCore::NotifyRequestFailed(
    bool in_cache,
    int net_error,
    const base::Optional<net::SSLInfo>& ssl_info) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  TRACE_EVENT_ASYNC_END0("navigation", "Navigation redirectDelay", this);
  TRACE_EVENT_ASYNC_END2("navigation", "Navigation timeToResponseStarted", this,
                         "&NavigationURLLoaderImplCore", this, "success",
                         false);

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::BindOnce(&NavigationURLLoaderImpl::NotifyRequestFailed, loader_,
                     in_cache, net_error, ssl_info));
}

}  // namespace content
