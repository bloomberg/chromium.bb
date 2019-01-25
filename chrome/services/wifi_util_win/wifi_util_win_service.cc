// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/wifi_util_win/wifi_util_win_service.h"

#include <memory>

#include "base/bind.h"
#include "build/build_config.h"
#include "chrome/services/wifi_util_win/wifi_credentials_getter.h"
#include "mojo/public/cpp/bindings/strong_binding.h"

namespace {

void OnWifiCredentialsGetterRequest(
    service_manager::ServiceKeepalive* keepalive,
    chrome::mojom::WiFiCredentialsGetterRequest request) {
  mojo::MakeStrongBinding(
      std::make_unique<WiFiCredentialsGetter>(keepalive->CreateRef()),
      std::move(request));
}

}  // namespace

WifiUtilWinService::WifiUtilWinService(
    service_manager::mojom::ServiceRequest request)
    : service_binding_(this, std::move(request)),
      service_keepalive_(&service_binding_, base::TimeDelta()) {}

WifiUtilWinService::~WifiUtilWinService() = default;

void WifiUtilWinService::OnStart() {
  registry_.AddInterface(base::BindRepeating(&OnWifiCredentialsGetterRequest,
                                             &service_keepalive_));
}

void WifiUtilWinService::OnBindInterface(
    const service_manager::BindSourceInfo& source_info,
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle interface_pipe) {
  registry_.BindInterface(interface_name, std::move(interface_pipe));
}
