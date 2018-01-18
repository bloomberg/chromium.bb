// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/url_loader_factory_impl.h"

#include "base/memory/ptr_util.h"
#include "content/browser/loader/resource_dispatcher_host_impl.h"
#include "content/browser/loader/resource_requester_info.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/interfaces/url_loader.mojom.h"

namespace content {

URLLoaderFactoryImpl::URLLoaderFactoryImpl(
    scoped_refptr<ResourceRequesterInfo> requester_info,
    const scoped_refptr<base::SingleThreadTaskRunner>& io_thread_runner)
    : requester_info_(std::move(requester_info)),
      io_thread_task_runner_(io_thread_runner) {
  DCHECK((requester_info_->IsRenderer() && requester_info_->filter()) ||
         requester_info_->IsNavigationPreload());
  DCHECK(io_thread_task_runner_->BelongsToCurrentThread());
  bindings_.set_connection_error_handler(base::Bind(
      &URLLoaderFactoryImpl::OnConnectionError, base::Unretained(this)));
}

URLLoaderFactoryImpl::~URLLoaderFactoryImpl() {
  DCHECK(io_thread_task_runner_->BelongsToCurrentThread());
}

void URLLoaderFactoryImpl::CreateLoaderAndStart(
    network::mojom::URLLoaderRequest request,
    int32_t routing_id,
    int32_t request_id,
    uint32_t options,
    const network::ResourceRequest& url_request,
    network::mojom::URLLoaderClientPtr client,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation) {
  CreateLoaderAndStart(
      requester_info_.get(), std::move(request), routing_id, request_id,
      options, url_request, std::move(client),
      static_cast<net::NetworkTrafficAnnotationTag>(traffic_annotation));
}

void URLLoaderFactoryImpl::Clone(
    network::mojom::URLLoaderFactoryRequest request) {
  bindings_.AddBinding(this, std::move(request));
}

// static
void URLLoaderFactoryImpl::CreateLoaderAndStart(
    ResourceRequesterInfo* requester_info,
    network::mojom::URLLoaderRequest request,
    int32_t routing_id,
    int32_t request_id,
    uint32_t options,
    const network::ResourceRequest& url_request,
    network::mojom::URLLoaderClientPtr client,
    const net::NetworkTrafficAnnotationTag& traffic_annotation) {
  DCHECK(ResourceDispatcherHostImpl::Get()
             ->io_thread_task_runner()
             ->BelongsToCurrentThread());

  ResourceDispatcherHostImpl* rdh = ResourceDispatcherHostImpl::Get();
  rdh->OnRequestResourceWithMojo(requester_info, routing_id, request_id,
                                 options, url_request, std::move(request),
                                 std::move(client), traffic_annotation);
}

// static
void URLLoaderFactoryImpl::Create(
    scoped_refptr<ResourceRequesterInfo> requester_info,
    network::mojom::URLLoaderFactoryRequest request,
    const scoped_refptr<base::SingleThreadTaskRunner>& io_thread_runner) {
  // This instance is effectively reference counted by the number of pipes open
  // to it and will get deleted when all clients drop their connections.
  // Please see OnConnectionError() for details.
  auto* impl =
      new URLLoaderFactoryImpl(std::move(requester_info), io_thread_runner);
  impl->Clone(std::move(request));
}

void URLLoaderFactoryImpl::OnConnectionError() {
  if (bindings_.empty())
    delete this;
}

}  // namespace content
