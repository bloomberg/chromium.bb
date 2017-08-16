// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/system_tray_delegate_chromeos.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "chrome/browser/ui/ash/networking_config_delegate_chromeos.h"

namespace chromeos {

SystemTrayDelegateChromeOS::SystemTrayDelegateChromeOS()
    : networking_config_delegate_(
          base::MakeUnique<NetworkingConfigDelegateChromeos>()) {}

SystemTrayDelegateChromeOS::~SystemTrayDelegateChromeOS() {}

ash::NetworkingConfigDelegate*
SystemTrayDelegateChromeOS::GetNetworkingConfigDelegate() const {
  return networking_config_delegate_.get();
}

ash::SystemTrayDelegate* CreateSystemTrayDelegate() {
  return new SystemTrayDelegateChromeOS();
}

}  // namespace chromeos
