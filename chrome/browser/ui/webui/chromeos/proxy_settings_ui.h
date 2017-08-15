// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_PROXY_SETTINGS_UI_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_PROXY_SETTINGS_UI_H_

#include "base/macros.h"
#include "ui/web_dialogs/web_dialog_ui.h"

namespace chromeos {

// A WebUI to host a subset of the network details page to allow setting of
// proxy, IP address, and nameservers in the login screen. (Historically the
// dialog only contained proxy settings).
class ProxySettingsUI : public ui::WebDialogUI {
 public:
  explicit ProxySettingsUI(content::WebUI* web_ui);
  ~ProxySettingsUI() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(ProxySettingsUI);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_PROXY_SETTINGS_UI_H_
