// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/network/network_service.h"

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "content/network/cache_url_loader.h"
#include "content/network/network_service_url_loader_factory_impl.h"
#include "services/service_manager/public/cpp/bind_source_info.h"

namespace content {

NetworkService::NetworkService(
    std::unique_ptr<service_manager::BinderRegistry> registry)
    : registry_(std::move(registry)) {
  registry_->AddInterface<mojom::URLLoaderFactory>(base::Bind(
      &NetworkService::CreateURLLoaderFactory, base::Unretained(this)));
  registry_->AddInterface<mojom::NetworkService>(base::Bind(
      &NetworkService::CreateNetworkService, base::Unretained(this)));
}

NetworkService::~NetworkService() = default;

void NetworkService::OnBindInterface(
    const service_manager::BindSourceInfo& source_info,
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle interface_pipe) {
  registry_->BindInterface(source_info, interface_name,
                           std::move(interface_pipe));
}

void NetworkService::CreateURLLoaderFactory(
    const service_manager::BindSourceInfo& source_info,
    mojom::URLLoaderFactoryRequest request) {
  loader_factory_bindings_.AddBinding(
      base::MakeUnique<NetworkServiceURLLoaderFactoryImpl>(&context_),
      std::move(request));
}

void NetworkService::CreateNetworkService(
    const service_manager::BindSourceInfo& source_info,
    mojom::NetworkServiceRequest request) {
  network_service_bindings_.AddBinding(this, std::move(request));
}

void NetworkService::HandleViewCacheRequest(const ResourceRequest& request,
                                            mojom::URLLoaderClientPtr client) {
  StartCacheURLLoader(request, context_.url_request_context(),
                      std::move(client));
}

}  // namespace content
