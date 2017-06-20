// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/navigation_url_loader_impl.h"

#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/trace_event/trace_event.h"
#include "content/browser/appcache/appcache_navigation_handle.h"
#include "content/browser/appcache/appcache_navigation_handle_core.h"
#include "content/browser/frame_host/navigation_request_info.h"
#include "content/browser/loader/navigation_url_loader_delegate.h"
#include "content/browser/loader/navigation_url_loader_impl_core.h"
#include "content/browser/service_worker/service_worker_navigation_handle.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/global_request_id.h"
#include "content/public/browser/navigation_data.h"
#include "content/public/browser/navigation_ui_data.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/stream_handle.h"

namespace content {

NavigationURLLoaderImpl::NavigationURLLoaderImpl(
    ResourceContext* resource_context,
    StoragePartition* storage_partition,
    std::unique_ptr<NavigationRequestInfo> request_info,
    std::unique_ptr<NavigationUIData> navigation_ui_data,
    ServiceWorkerNavigationHandle* service_worker_handle,
    AppCacheNavigationHandle* appcache_handle,
    NavigationURLLoaderDelegate* delegate)
    : delegate_(delegate), weak_factory_(this) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  core_ = new NavigationURLLoaderImplCore(weak_factory_.GetWeakPtr());

  // TODO(carlosk): extend this trace to support non-PlzNavigate navigations.
  // For the trace below we're using the NavigationURLLoaderImplCore as the
  // async trace id, |navigation_start| as the timestamp and reporting the
  // FrameTreeNode id as a parameter.
  TRACE_EVENT_ASYNC_BEGIN_WITH_TIMESTAMP1(
      "navigation", "Navigation timeToResponseStarted", core_.get(),
      request_info->common_params.navigation_start, "FrameTreeNode id",
      request_info->frame_tree_node_id);
  ServiceWorkerNavigationHandleCore* service_worker_handle_core =
      service_worker_handle ? service_worker_handle->core() : nullptr;
  AppCacheNavigationHandleCore* appcache_handle_core =
      appcache_handle ? appcache_handle->core() : nullptr;
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&NavigationURLLoaderImplCore::Start, core_, resource_context,
                 storage_partition->GetURLRequestContext(),
                 base::Unretained(storage_partition->GetFileSystemContext()),
                 service_worker_handle_core, appcache_handle_core,
                 base::Passed(&request_info),
                 base::Passed(&navigation_ui_data)));
}

NavigationURLLoaderImpl::~NavigationURLLoaderImpl() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&NavigationURLLoaderImplCore::CancelRequestIfNeeded, core_));
}

void NavigationURLLoaderImpl::FollowRedirect() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&NavigationURLLoaderImplCore::FollowRedirect, core_));
}

void NavigationURLLoaderImpl::ProceedWithResponse() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&NavigationURLLoaderImplCore::ProceedWithResponse, core_));
}

void NavigationURLLoaderImpl::NotifyRequestRedirected(
    const net::RedirectInfo& redirect_info,
    const scoped_refptr<ResourceResponse>& response) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  delegate_->OnRequestRedirected(redirect_info, response);
}

void NavigationURLLoaderImpl::NotifyResponseStarted(
    const scoped_refptr<ResourceResponse>& response,
    std::unique_ptr<StreamHandle> body,
    const SSLStatus& ssl_status,
    std::unique_ptr<NavigationData> navigation_data,
    const GlobalRequestID& request_id,
    bool is_download,
    bool is_stream) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  delegate_->OnResponseStarted(
      response, std::move(body), mojo::ScopedDataPipeConsumerHandle(),
      ssl_status, std::move(navigation_data), request_id, is_download,
      is_stream, mojom::URLLoaderFactoryPtrInfo());
}

void NavigationURLLoaderImpl::NotifyRequestFailed(bool in_cache,
                                                  int net_error) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  delegate_->OnRequestFailed(in_cache, net_error);
}

void NavigationURLLoaderImpl::NotifyRequestStarted(base::TimeTicks timestamp) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  delegate_->OnRequestStarted(timestamp);
}

}  // namespace content
