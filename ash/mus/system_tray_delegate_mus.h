// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_MUS_SYSTEM_TRAY_DELEGATE_MUS_H_
#define ASH_MUS_SYSTEM_TRAY_DELEGATE_MUS_H_

#include <memory>

#include "ash/system/tray/system_tray_delegate.h"
#include "base/macros.h"

namespace ash {

class NetworkingConfigDelegate;

// Handles the settings displayed in the system tray menu. For the classic ash
// version see SystemTrayDelegateChromeOS.
//
// TODO: Replace with SystemTrayController. http://crbug.com/647412.
class SystemTrayDelegateMus : public SystemTrayDelegate {
 public:
  SystemTrayDelegateMus();
  ~SystemTrayDelegateMus() override;

 private:
  // SystemTrayDelegate:
  NetworkingConfigDelegate* GetNetworkingConfigDelegate() const override;

  std::unique_ptr<NetworkingConfigDelegate> networking_config_delegate_;

  DISALLOW_COPY_AND_ASSIGN(SystemTrayDelegateMus);
};

}  // namespace ash

#endif  // ASH_MUS_SYSTEM_TRAY_DELEGATE_MUS_H_
