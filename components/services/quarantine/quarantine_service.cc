// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/bind.h"
#include "base/time/time.h"
#include "components/services/quarantine/quarantine_impl.h"
#include "components/services/quarantine/quarantine_service.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "services/service_manager/public/cpp/connector.h"

namespace quarantine {

namespace {

void OnQuarantineRequest(service_manager::ServiceKeepalive* keepalive,
                         mojom::QuarantineRequest request) {
  mojo::MakeStrongBinding(
      std::make_unique<QuarantineImpl>(keepalive->CreateRef()),
      std::move(request));
}

}  // namespace

QuarantineService::QuarantineService(
    service_manager::mojom::ServiceRequest request)
    : binding_(this, std::move(request)),
      keepalive_(&binding_, base::TimeDelta{}) {}

QuarantineService::~QuarantineService() = default;

void QuarantineService::OnStart() {
  registry_.AddInterface(
      base::BindRepeating(&OnQuarantineRequest, &keepalive_));
}

void QuarantineService::OnBindInterface(
    const service_manager::BindSourceInfo& source_info,
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle interface_pipe) {
  registry_.BindInterface(interface_name, std::move(interface_pipe));
}

}  // namespace quarantine
