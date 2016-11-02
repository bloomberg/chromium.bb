// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_WEBUI_POPULAR_SITES_INTERNALS_UI_H_
#define IOS_CHROME_BROWSER_UI_WEBUI_POPULAR_SITES_INTERNALS_UI_H_

#include <string>

#include "base/macros.h"
#include "ios/web/public/webui/web_ui_ios_controller.h"

// The WebUI handler for chrome://physical-web.
class PopularSitesInternalsUI : public web::WebUIIOSController {
 public:
  explicit PopularSitesInternalsUI(web::WebUIIOS* web_ui);
  ~PopularSitesInternalsUI() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(PopularSitesInternalsUI);
};

#endif  // IOS_CHROME_BROWSER_UI_WEBUI_POPULAR_SITES_INTERNALS_UI_H_
