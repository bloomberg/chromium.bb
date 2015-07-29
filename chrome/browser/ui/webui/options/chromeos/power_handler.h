// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_OPTIONS_CHROMEOS_POWER_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_OPTIONS_CHROMEOS_POWER_HANDLER_H_

#include "ash/system/chromeos/power/power_status.h"
#include "base/macros.h"
#include "base/strings/string16.h"
#include "chrome/browser/ui/webui/options/options_ui.h"

namespace chromeos {
namespace options {

// Shows battery status and power settings.
class PowerHandler : public ::options::OptionsPageUIHandler,
                     public ash::PowerStatus::Observer {
 public:
  PowerHandler();
  ~PowerHandler() override;

  // OptionsPageUIHandler implementation.
  void GetLocalizedValues(base::DictionaryValue* localized_strings) override;
  void InitializePage() override;
  void RegisterMessages() override;

  // ash::PowerStatus::Observer implementation.
  void OnPowerStatusChanged() override;

private:
  // Gets the battery status text, showing the percentage and time remaining if
  // calculated. Returns an empty string if there is no battery.
  base::string16 GetStatusValue() const;

  // Handler to request updating the power status.
  void UpdatePowerStatus(const base::ListValue* args);

  // Handler to change the power source.
  void SetPowerSource(const base::ListValue* args);

  // Updates the UI with the latest power source information.
  void UpdatePowerSources();

  // Whether the UI should show the power status.
  bool show_power_status_;

  DISALLOW_COPY_AND_ASSIGN(PowerHandler);
};

}  // namespace options
}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_OPTIONS_CHROMEOS_POWER_HANDLER_H_
