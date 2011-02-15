// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_WEBUI_MOBILE_SETUP_UI_H_
#define CHROME_BROWSER_CHROMEOS_WEBUI_MOBILE_SETUP_UI_H_
#pragma once

#include "chrome/browser/dom_ui/web_ui.h"

// A custom WebUI that defines datasource for mobile setup registration page
// that is used in Chrome OS activate modem and perform plan subscription tasks.
class MobileSetupUI : public WebUI {
 public:
  explicit MobileSetupUI(TabContents* contents);

 private:
  DISALLOW_COPY_AND_ASSIGN(MobileSetupUI);
};

#endif  // CHROME_BROWSER_CHROMEOS_WEBUI_MOBILE_SETUP_UI_H_
