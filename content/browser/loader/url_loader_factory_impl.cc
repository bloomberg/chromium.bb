// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/url_loader_factory_impl.h"

#include "base/memory/ptr_util.h"
#include "content/browser/loader/resource_dispatcher_host_impl.h"
#include "content/browser/loader/resource_requester_info.h"
#include "content/common/resource_request.h"
#include "content/common/url_loader.mojom.h"
#include "mojo/public/cpp/bindings/strong_binding.h"

namespace content {

namespace {

void DispatchSyncLoadResult(URLLoaderFactoryImpl::SyncLoadCallback callback,
                            const SyncLoadResult* result) {
  // |result| can be null when a loading task is aborted unexpectedly. Reply
  // with a failure result on that case.
  // TODO(tzik): Test null-result case.
  if (!result) {
    SyncLoadResult failure;
    failure.error_code = net::ERR_FAILED;
    std::move(callback).Run(failure);
    return;
  }

  std::move(callback).Run(*result);
}

} // namespace

URLLoaderFactoryImpl::URLLoaderFactoryImpl(
    scoped_refptr<ResourceRequesterInfo> requester_info,
    const scoped_refptr<base::SingleThreadTaskRunner>& io_thread_runner)
    : requester_info_(std::move(requester_info)),
      io_thread_task_runner_(io_thread_runner) {
  DCHECK((requester_info_->IsRenderer() && requester_info_->filter()) ||
         requester_info_->IsNavigationPreload());
  DCHECK(io_thread_task_runner_->BelongsToCurrentThread());
}

URLLoaderFactoryImpl::~URLLoaderFactoryImpl() {
  DCHECK(io_thread_task_runner_->BelongsToCurrentThread());
}

void URLLoaderFactoryImpl::CreateLoaderAndStart(
    mojom::URLLoaderAssociatedRequest request,
    int32_t routing_id,
    int32_t request_id,
    uint32_t options,
    const ResourceRequest& url_request,
    mojom::URLLoaderClientPtr client,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation) {
  DCHECK_EQ(options, mojom::kURLLoadOptionNone);
  CreateLoaderAndStart(
      requester_info_.get(), std::move(request), routing_id, request_id,
      url_request, std::move(client),
      static_cast<net::NetworkTrafficAnnotationTag>(traffic_annotation));
}

void URLLoaderFactoryImpl::SyncLoad(int32_t routing_id,
                                    int32_t request_id,
                                    const ResourceRequest& url_request,
                                    SyncLoadCallback callback) {
  SyncLoad(requester_info_.get(), routing_id, request_id, url_request,
           std::move(callback));
}

// static
void URLLoaderFactoryImpl::CreateLoaderAndStart(
    ResourceRequesterInfo* requester_info,
    mojom::URLLoaderAssociatedRequest request,
    int32_t routing_id,
    int32_t request_id,
    const ResourceRequest& url_request,
    mojom::URLLoaderClientPtr client,
    const net::NetworkTrafficAnnotationTag& traffic_annotation) {
  DCHECK(ResourceDispatcherHostImpl::Get()
             ->io_thread_task_runner()
             ->BelongsToCurrentThread());

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
                                    SyncLoadCallback callback) {
  DCHECK(ResourceDispatcherHostImpl::Get()
             ->io_thread_task_runner()
             ->BelongsToCurrentThread());

  ResourceDispatcherHostImpl* rdh = ResourceDispatcherHostImpl::Get();
  rdh->OnSyncLoadWithMojo(
      requester_info, routing_id, request_id, url_request,
      base::Bind(&DispatchSyncLoadResult, base::Passed(&callback)));
}

void URLLoaderFactoryImpl::Create(
    scoped_refptr<ResourceRequesterInfo> requester_info,
    mojo::InterfaceRequest<mojom::URLLoaderFactory> request,
    const scoped_refptr<base::SingleThreadTaskRunner>& io_thread_runner) {
  mojo::MakeStrongBinding(base::WrapUnique(new URLLoaderFactoryImpl(
                              std::move(requester_info), io_thread_runner)),
                          std::move(request));
}

}  // namespace content
