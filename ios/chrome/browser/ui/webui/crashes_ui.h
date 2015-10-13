// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_WEBUI_CRASHES_UI_H_
#define IOS_CHROME_BROWSER_UI_WEBUI_CRASHES_UI_H_

#include "base/macros.h"
#include "ios/public/provider/web/web_ui_ios_controller.h"
#include "ui/base/layout.h"

namespace base {
class RefCountedMemory;
}

namespace web {
class WebUIIOS;
}

class CrashesUI : public web::WebUIIOSController {
 public:
  explicit CrashesUI(web::WebUIIOS* web_ui);

  static base::RefCountedMemory* GetFaviconResourceBytes(
      ui::ScaleFactor scale_factor);

 private:
  DISALLOW_COPY_AND_ASSIGN(CrashesUI);
};

#endif  // CHROME_BROWSER_UI_WEBUI_CRASHES_UI_H_
