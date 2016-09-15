// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/associated_interface_provider_impl.h"

namespace content {

AssociatedInterfaceProviderImpl::AssociatedInterfaceProviderImpl(
    mojom::AssociatedInterfaceProviderAssociatedPtr proxy)
    : proxy_(std::move(proxy)) {
}

AssociatedInterfaceProviderImpl::~AssociatedInterfaceProviderImpl() {}

void AssociatedInterfaceProviderImpl::GetInterface(
    const std::string& name,
    mojo::ScopedInterfaceEndpointHandle handle) {
  mojom::AssociatedInterfaceAssociatedRequest request;
  request.Bind(std::move(handle));
  return proxy_->GetAssociatedInterface(name, std::move(request));
}

mojo::AssociatedGroup* AssociatedInterfaceProviderImpl::GetAssociatedGroup() {
  return proxy_.associated_group();
}

}  // namespace content
