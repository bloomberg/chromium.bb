// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DBUS_CHROME_DISPLAY_POWER_SERVICE_PROVIDER_DELEGATE_H_
#define CHROME_BROWSER_CHROMEOS_DBUS_CHROME_DISPLAY_POWER_SERVICE_PROVIDER_DELEGATE_H_

#include <memory>

#include "base/macros.h"
#include "chromeos/dbus/services/display_power_service_provider.h"

namespace ash {
class ScreenDimmer;
}

namespace chromeos {

// Chrome's implementation of DisplayPowerServiceProvider::Delegate
class ChromeDisplayPowerServiceProviderDelegate
    : public DisplayPowerServiceProvider::Delegate {
 public:
  ChromeDisplayPowerServiceProviderDelegate();
  ~ChromeDisplayPowerServiceProviderDelegate() override;

  // DisplayPowerServiceProvider::Delegate overrides:
  void SetDisplayPower(DisplayPowerState power_state,
                       const ResponseCallback& callback) override;
  void SetDimming(bool dimmed) override;

 private:
  std::unique_ptr<ash::ScreenDimmer> screen_dimmer_;

  DISALLOW_COPY_AND_ASSIGN(ChromeDisplayPowerServiceProviderDelegate);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_DBUS_CHROME_DISPLAY_POWER_SERVICE_PROVIDER_DELEGATE_H_
