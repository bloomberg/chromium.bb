// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/kiosk_next_home/kiosk_next_home_interface_broker_impl.h"

#include <utility>

#include "services/identity/public/mojom/constants.mojom.h"
#include "services/identity/public/mojom/identity_accessor.mojom.h"
#include "services/service_manager/public/cpp/connector.h"

namespace chromeos {
namespace kiosk_next_home {

KioskNextHomeInterfaceBrokerImpl::KioskNextHomeInterfaceBrokerImpl(
    service_manager::Connector* connector)
    : connector_(connector->Clone()) {}

KioskNextHomeInterfaceBrokerImpl::~KioskNextHomeInterfaceBrokerImpl() = default;

void KioskNextHomeInterfaceBrokerImpl::GetIdentityAccessor(
    ::identity::mojom::IdentityAccessorRequest request) {
  connector_->BindInterface(::identity::mojom::kServiceName,
                            std::move(request));
}

void KioskNextHomeInterfaceBrokerImpl::BindRequest(
    mojom::KioskNextHomeInterfaceBrokerRequest request) {
  bindings_.AddBinding(this, std::move(request));
}

}  // namespace kiosk_next_home
}  // namespace chromeos
