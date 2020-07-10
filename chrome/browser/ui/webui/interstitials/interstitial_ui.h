// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_INTERSTITIALS_INTERSTITIAL_UI_H_
#define CHROME_BROWSER_UI_WEBUI_INTERSTITIALS_INTERSTITIAL_UI_H_

#include <string>

#include "base/macros.h"
#include "content/public/browser/web_ui_controller.h"

// Handler for chrome://interstitials demonstration pages. This class is not
// used in displaying any real interstitials.
class InterstitialUI : public content::WebUIController {
 public:
  explicit InterstitialUI(content::WebUI* web_ui);

  ~InterstitialUI() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(InterstitialUI);
};

#endif  // CHROME_BROWSER_UI_WEBUI_INTERSTITIALS_INTERSTITIAL_UI_H_
