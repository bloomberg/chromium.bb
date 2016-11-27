// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_INSTANT_UI_H_
#define CHROME_BROWSER_UI_WEBUI_INSTANT_UI_H_

#include "base/macros.h"
#include "content/public/browser/web_ui_controller.h"

namespace user_prefs {
class PrefRegistrySyncable;
}

// Provides configuration options for Instant web search and shows debug info.
class InstantUI : public content::WebUIController {
 public:
  // Constructs an instance using |web_ui| for its data sources and message
  // handlers.
  explicit InstantUI(content::WebUI* web_ui);

  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

 private:
  DISALLOW_COPY_AND_ASSIGN(InstantUI);
};

#endif  // CHROME_BROWSER_UI_WEBUI_INSTANT_UI_H_
