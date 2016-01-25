// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_WEBUI_HISTORY_HISTORY_UI_H_
#define IOS_CHROME_BROWSER_UI_WEBUI_HISTORY_HISTORY_UI_H_

#include "base/macros.h"
#include "ios/public/provider/web/web_ui_ios_controller.h"
#include "ui/base/layout.h"

namespace base {
class RefCountedMemory;
}

namespace web {
class WebUIIOS;
}

class HistoryUI : public web::WebUIIOSController {
 public:
  explicit HistoryUI(web::WebUIIOS* web_ui);
  ~HistoryUI() override;

  static base::RefCountedMemory* GetFaviconResourceBytes(
      ui::ScaleFactor scale_factor);

 private:
  DISALLOW_COPY_AND_ASSIGN(HistoryUI);
};

#endif  // IOS_CHROME_BROWSER_UI_WEBUI_HISTORY_HISTORY_UI_H_
