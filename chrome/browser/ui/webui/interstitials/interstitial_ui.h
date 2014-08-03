// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_INTERSTITIALS_INTERSTITIAL_UI_H_
#define CHROME_BROWSER_UI_WEBUI_INTERSTITIALS_INTERSTITIAL_UI_H_

#include <string>

#include "content/public/browser/url_data_source.h"
#include "content/public/browser/web_ui_controller.h"

class Profile;

namespace content {
class WebContents;
}

class InterstitialUI : public content::WebUIController {
 public:
  explicit InterstitialUI(content::WebUI* web_ui);

  virtual ~InterstitialUI();

 private:
  DISALLOW_COPY_AND_ASSIGN(InterstitialUI);
};

#endif  // CHROME_BROWSER_UI_WEBUI_INTERSTITIALS_INTERSTITIAL_UI_H_
