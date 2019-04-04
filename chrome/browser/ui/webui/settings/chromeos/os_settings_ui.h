// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_OS_SETTINGS_UI_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_OS_SETTINGS_UI_H_

#include "base/macros.h"
#include "content/public/browser/web_ui_controller.h"

namespace chromeos {
namespace settings {

// The WebUI handler for chrome://os-settings.
class OSSettingsUI : public content::WebUIController {
 public:
  explicit OSSettingsUI(content::WebUI* web_ui);
  ~OSSettingsUI() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(OSSettingsUI);
};

}  // namespace settings
}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_OS_SETTINGS_UI_H_
