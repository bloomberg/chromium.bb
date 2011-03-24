// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_PROXY_SETTINGS_UI_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_PROXY_SETTINGS_UI_H_
#pragma once

#include "chrome/browser/ui/webui/options/options_ui.h"
#include "content/browser/webui/web_ui.h"

namespace chromeos {

// A WebUI to host proxy settings splitted from settings page for better
// performance.
class ProxySettingsUI : public WebUI,
                        public OptionsPageUIHandlerHost {
 public:
  explicit ProxySettingsUI(TabContents* contents);
  virtual ~ProxySettingsUI();

 private:
  // Overridden from OptionsPageUIHandlerHost:
  virtual void InitializeHandlers() OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(ProxySettingsUI);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_PROXY_SETTINGS_UI_H_
