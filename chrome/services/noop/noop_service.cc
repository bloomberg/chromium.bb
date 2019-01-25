// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/noop/noop_service.h"
#include "base/bind.h"

namespace chrome {

NoopService::NoopService(service_manager::mojom::ServiceRequest request)
    : service_binding_(this, std::move(request)) {
  registry_.AddInterface<mojom::Noop>(base::BindRepeating(
      &NoopService::BindNoopRequest, base::Unretained(this)));
}

NoopService::~NoopService() = default;

void NoopService::OnBindInterface(
    const service_manager::BindSourceInfo& source_info,
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle interface_pipe) {
  registry_.BindInterface(interface_name, std::move(interface_pipe));
}

void NoopService::BindNoopRequest(mojom::NoopRequest request) {
  bindings_.AddBinding(this, std::move(request));
}

}  // namespace chrome
