// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_SYSTEM_TRAY_DELEGATE_CHROMEOS_H_
#define CHROME_BROWSER_UI_ASH_SYSTEM_TRAY_DELEGATE_CHROMEOS_H_

#include <memory>

#include "ash/system/tray/system_tray_delegate.h"
#include "base/macros.h"

namespace chromeos {

// DEPRECATED. Do not add new code here. This class is being removed as part of
// the transition to mustash. New code should be added to SystemTrayClient.
// Use system_tray.mojom methods if you need to send information to ash.
// Please contact jamescook@chromium.org if you have questions or need help.
class SystemTrayDelegateChromeOS : public ash::SystemTrayDelegate {
 public:
  SystemTrayDelegateChromeOS();
  ~SystemTrayDelegateChromeOS() override;

  // Overridden from ash::SystemTrayDelegate:
  ash::NetworkingConfigDelegate* GetNetworkingConfigDelegate() const override;

 private:
  std::unique_ptr<ash::NetworkingConfigDelegate> networking_config_delegate_;

  DISALLOW_COPY_AND_ASSIGN(SystemTrayDelegateChromeOS);
};

ash::SystemTrayDelegate* CreateSystemTrayDelegate();

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_ASH_SYSTEM_TRAY_DELEGATE_CHROMEOS_H_
