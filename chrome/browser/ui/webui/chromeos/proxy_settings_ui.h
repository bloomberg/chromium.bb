// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_PROXY_SETTINGS_UI_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_PROXY_SETTINGS_UI_H_
#pragma once

#include "chrome/browser/ui/webui/options/options_ui.h"
#include "content/public/browser/web_ui_controller.h"

namespace chromeos {

class CoreChromeOSOptionsHandler;
class ProxyHandler;

// A WebUI to host proxy settings splitted from settings page for better
// performance.
class ProxySettingsUI : public content::WebUIController,
                        public OptionsPageUIHandlerHost {
 public:
  explicit ProxySettingsUI(content::WebUI* web_ui);
  virtual ~ProxySettingsUI();

 private:
  // Overridden from OptionsPageUIHandlerHost:
  virtual void InitializeHandlers() OVERRIDE;

  ProxyHandler* proxy_handler_;  // Weak ptr.
  CoreChromeOSOptionsHandler* core_handler_; // WeakPtr.

  DISALLOW_COPY_AND_ASSIGN(ProxySettingsUI);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_PROXY_SETTINGS_UI_H_
