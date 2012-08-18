// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_PERFORMANCE_MONITOR_WEB_UI_H_
#define CHROME_BROWSER_UI_WEBUI_PERFORMANCE_MONITOR_WEB_UI_H_

#include "content/public/browser/web_ui_controller.h"

namespace performance_monitor {

class WebUI : public content::WebUIController {
 public:
  explicit WebUI(content::WebUI* web_ui);

 private:
  DISALLOW_COPY_AND_ASSIGN(WebUI);
};

}  // namespace performance_monitor

#endif  // CHROME_BROWSER_UI_WEBUI_PERFORMANCE_MONITOR_WEB_UI_H_
