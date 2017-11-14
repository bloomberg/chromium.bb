// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/wifi_util_win/wifi_util_win_service.h"

#include <memory>

#include "build/build_config.h"
#include "chrome/services/wifi_util_win/wifi_credentials_getter.h"
#include "mojo/public/cpp/bindings/strong_binding.h"

namespace chrome {

namespace {

void OnWifiCredentialsGetterRequest(
    service_manager::ServiceContextRefFactory* ref_factory,
    chrome::mojom::WiFiCredentialsGetterRequest request) {
  mojo::MakeStrongBinding(
      std::make_unique<WiFiCredentialsGetter>(ref_factory->CreateRef()),
      std::move(request));
}

}  // namespace

WifiUtilWinService::WifiUtilWinService() = default;

WifiUtilWinService::~WifiUtilWinService() = default;

std::unique_ptr<service_manager::Service> WifiUtilWinService::CreateService() {
  return std::make_unique<WifiUtilWinService>();
}

void WifiUtilWinService::OnStart() {
  ref_factory_ = std::make_unique<service_manager::ServiceContextRefFactory>(
      base::Bind(&service_manager::ServiceContext::RequestQuit,
                 base::Unretained(context())));
  registry_.AddInterface(
      base::Bind(&OnWifiCredentialsGetterRequest, ref_factory_.get()));
}

void WifiUtilWinService::OnBindInterface(
    const service_manager::BindSourceInfo& source_info,
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle interface_pipe) {
  registry_.BindInterface(interface_name, std::move(interface_pipe));
}

}  //  namespace chrome
