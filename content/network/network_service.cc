// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/network/network_service.h"

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "content/network/network_service_url_loader_factory_impl.h"
#include "services/service_manager/public/cpp/bind_source_info.h"

namespace content {

NetworkService::NetworkService(
    std::unique_ptr<service_manager::BinderRegistry> registry)
    : registry_(std::move(registry)) {
  registry_->AddInterface<mojom::URLLoaderFactory>(this);
}

NetworkService::~NetworkService() = default;

void NetworkService::OnBindInterface(
    const service_manager::BindSourceInfo& source_info,
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle interface_pipe) {
  registry_->BindInterface(source_info, interface_name,
                           std::move(interface_pipe));
}

void NetworkService::Create(const service_manager::Identity& remote_identity,
                            mojom::URLLoaderFactoryRequest request) {
  loader_factory_bindings_.AddBinding(
      base::MakeUnique<NetworkServiceURLLoaderFactoryImpl>(&context_),
      std::move(request));
}

}  // namespace content
