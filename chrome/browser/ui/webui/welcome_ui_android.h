// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_WELCOME_UI_ANDROID_H_
#define CHROME_BROWSER_UI_WEBUI_WELCOME_UI_ANDROID_H_

#include "content/public/browser/web_ui_controller.h"

// The WebUI for chrome://welcome
class WelcomeUI : public content::WebUIController {
 public:
  explicit WelcomeUI(content::WebUI* web_ui);
  virtual ~WelcomeUI();
 private:
  DISALLOW_COPY_AND_ASSIGN(WelcomeUI);
};

#endif  // CHROME_BROWSER_UI_WEBUI_WELCOME_UI_ANDROID_H_
