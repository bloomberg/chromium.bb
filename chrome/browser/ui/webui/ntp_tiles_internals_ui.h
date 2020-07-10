// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_NTP_TILES_INTERNALS_UI_H_
#define CHROME_BROWSER_UI_WEBUI_NTP_TILES_INTERNALS_UI_H_

#include "base/macros.h"
#include "content/public/browser/web_ui_controller.h"

// The implementation for the chrome://ntp-tiles-internals page.
class NTPTilesInternalsUI : public content::WebUIController {
 public:
  explicit NTPTilesInternalsUI(content::WebUI* web_ui);
  ~NTPTilesInternalsUI() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(NTPTilesInternalsUI);
};

#endif  // CHROME_BROWSER_UI_WEBUI_NTP_TILES_INTERNALS_UI_H_
