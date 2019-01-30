// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/fuchsia/service_provider_impl.h"

#include <utility>

namespace base {
namespace fuchsia {

ServiceProviderImpl::ServiceProviderImpl(
    fidl::InterfaceHandle<::fuchsia::io::Directory> service_directory)
    : directory_(std::move(service_directory)) {}

ServiceProviderImpl::~ServiceProviderImpl() = default;

void ServiceProviderImpl::AddBinding(
    fidl::InterfaceRequest<::fuchsia::sys::ServiceProvider> request) {
  bindings_.AddBinding(this, std::move(request));
}

void ServiceProviderImpl::ConnectToService(std::string service_name,
                                           zx::channel client_handle) {
  directory_.ConnectToServiceUnsafe(service_name.c_str(),
                                    std::move(client_handle));
}

}  // namespace fuchsia
}  // namespace base
