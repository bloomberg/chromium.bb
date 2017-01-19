// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_DEVICE_POWER_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_DEVICE_POWER_HANDLER_H_

#include "ash/common/system/chromeos/power/power_status.h"
#include "base/macros.h"
#include "base/scoped_observer.h"
#include "base/strings/string16.h"
#include "chrome/browser/ui/webui/settings/settings_page_ui_handler.h"

namespace base {
class ListValue;
}

namespace chromeos {
namespace settings {

// Chrome OS battery status and power settings handler.
class PowerHandler : public ::settings::SettingsPageUIHandler,
                     public ash::PowerStatus::Observer {
 public:
  PowerHandler();
  ~PowerHandler() override;

  // SettingsPageUIHandler implementation.
  void RegisterMessages() override;
  void OnJavascriptAllowed() override;
  void OnJavascriptDisallowed() override;

  // ash::PowerStatus::Observer implementation.
  void OnPowerStatusChanged() override;

 private:
  // Handler to request updating the power status.
  void HandleUpdatePowerStatus(const base::ListValue* args);

  // Handler to change the power source.
  void HandleSetPowerSource(const base::ListValue* args);

  // Updates the UI with the current battery status.
  void SendBatteryStatus();

  // Updates the UI with a list of available dual-role power sources.
  void SendPowerSources();

  ash::PowerStatus* power_status_;

  ScopedObserver<ash::PowerStatus, PowerHandler> power_observer_;

  DISALLOW_COPY_AND_ASSIGN(PowerHandler);
};

}  // namespace settings
}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_DEVICE_POWER_HANDLER_H_
