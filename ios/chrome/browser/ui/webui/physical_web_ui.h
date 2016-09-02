// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_WEBUI_PHYSICAL_WEB_UI_H_
#define IOS_CHROME_BROWSER_UI_WEBUI_PHYSICAL_WEB_UI_H_

#include <string>

#include "base/macros.h"
#include "ios/web/public/webui/web_ui_ios_controller.h"

// The WebUI handler for chrome://physical-web.
class PhysicalWebUI : public web::WebUIIOSController {
 public:
  explicit PhysicalWebUI(web::WebUIIOS* web_ui);
  ~PhysicalWebUI() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(PhysicalWebUI);
};

#endif  // IOS_CHROME_BROWSER_UI_WEBUI_PHYSICAL_WEB_UI_H_
