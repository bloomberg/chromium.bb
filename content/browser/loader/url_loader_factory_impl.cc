// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/url_loader_factory_impl.h"

#include "content/browser/loader/resource_dispatcher_host_impl.h"
#include "content/browser/loader/resource_requester_info.h"
#include "content/common/resource_request.h"
#include "content/common/url_loader.mojom.h"
#include "content/public/browser/browser_thread.h"
#include "mojo/public/cpp/bindings/strong_binding.h"

namespace content {

namespace {

void DispatchSyncLoadResult(
    const URLLoaderFactoryImpl::SyncLoadCallback& callback,
    const SyncLoadResult* result) {
  // |result| can be null when a loading task is aborted unexpectedly. Reply
  // with a failure result on that case.
  // TODO(tzik): Test null-result case.
  if (!result) {
    SyncLoadResult failure;
    failure.error_code = net::ERR_FAILED;
    callback.Run(failure);
    return;
  }

  callback.Run(*result);
}

} // namespace

URLLoaderFactoryImpl::URLLoaderFactoryImpl(
    scoped_refptr<ResourceRequesterInfo> requester_info)
    : requester_info_(std::move(requester_info)) {
  DCHECK(requester_info_->IsRenderer());
  DCHECK(requester_info_->filter());
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
}

URLLoaderFactoryImpl::~URLLoaderFactoryImpl() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
}

void URLLoaderFactoryImpl::CreateLoaderAndStart(
    mojom::URLLoaderAssociatedRequest request,
    int32_t routing_id,
    int32_t request_id,
    const ResourceRequest& url_request,
    mojom::URLLoaderClientAssociatedPtrInfo client_ptr_info) {
  CreateLoaderAndStart(requester_info_.get(), std::move(request), routing_id,
                       request_id, url_request, std::move(client_ptr_info));
}

void URLLoaderFactoryImpl::SyncLoad(int32_t routing_id,
                                    int32_t request_id,
                                    const ResourceRequest& url_request,
                                    const SyncLoadCallback& callback) {
  SyncLoad(requester_info_.get(), routing_id, request_id, url_request,
           callback);
}

// static
void URLLoaderFactoryImpl::CreateLoaderAndStart(
    ResourceRequesterInfo* requester_info,
    mojom::URLLoaderAssociatedRequest request,
    int32_t routing_id,
    int32_t request_id,
    const ResourceRequest& url_request,
    mojom::URLLoaderClientAssociatedPtrInfo client_ptr_info) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  mojom::URLLoaderClientAssociatedPtr client;
  client.Bind(std::move(client_ptr_info));

  ResourceDispatcherHostImpl* rdh = ResourceDispatcherHostImpl::Get();
  rdh->OnRequestResourceWithMojo(requester_info, routing_id, request_id,
                                 url_request, std::move(request),
                                 std::move(client));
}

// static
void URLLoaderFactoryImpl::SyncLoad(ResourceRequesterInfo* requester_info,
                                    int32_t routing_id,
                                    int32_t request_id,
                                    const ResourceRequest& url_request,
                                    const SyncLoadCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  ResourceDispatcherHostImpl* rdh = ResourceDispatcherHostImpl::Get();
  rdh->OnSyncLoadWithMojo(requester_info, routing_id, request_id, url_request,
                          base::Bind(&DispatchSyncLoadResult, callback));
}

void URLLoaderFactoryImpl::Create(
    scoped_refptr<ResourceRequesterInfo> requester_info,
    mojo::InterfaceRequest<mojom::URLLoaderFactory> request) {
  mojo::MakeStrongBinding(
      base::WrapUnique(new URLLoaderFactoryImpl(std::move(requester_info))),
      std::move(request));
}

}  // namespace content
