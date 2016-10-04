// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/mus/system_tray_delegate_mus.h"

#include "ash/common/system/networking_config_delegate.h"
#include "ash/mus/vpn_delegate_mus.h"

namespace ash {
namespace {

// TODO(mash): Provide a real implementation, perhaps by folding its behavior
// into an ash-side network information cache. http://crbug.com/651157
class StubNetworkingConfigDelegate : public NetworkingConfigDelegate {
 public:
  StubNetworkingConfigDelegate() {}
  ~StubNetworkingConfigDelegate() override {}

 private:
  // NetworkingConfigDelegate:
  std::unique_ptr<const ExtensionInfo> LookUpExtensionForNetwork(
      const std::string& service_path) override {
    return nullptr;
  }

  DISALLOW_COPY_AND_ASSIGN(StubNetworkingConfigDelegate);
};

}  // namespace

SystemTrayDelegateMus::SystemTrayDelegateMus()
    : networking_config_delegate_(new StubNetworkingConfigDelegate),
      vpn_delegate_(new VPNDelegateMus) {}

SystemTrayDelegateMus::~SystemTrayDelegateMus() {
}

NetworkingConfigDelegate* SystemTrayDelegateMus::GetNetworkingConfigDelegate()
    const {
  return networking_config_delegate_.get();
}

VPNDelegate* SystemTrayDelegateMus::GetVPNDelegate() const {
  return vpn_delegate_.get();
}

}  // namespace ash
