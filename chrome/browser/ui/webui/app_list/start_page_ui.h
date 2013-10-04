// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_APP_LIST_START_PAGE_UI_H_
#define CHROME_BROWSER_UI_WEBUI_APP_LIST_START_PAGE_UI_H_

#include "base/basictypes.h"
#include "content/public/browser/web_ui_controller.h"

namespace app_list {

// StartPageUI for the app launcher start page.
class StartPageUI : public content::WebUIController {
 public:
  explicit StartPageUI(content::WebUI* web_ui);
  virtual ~StartPageUI();

 private:
  // Initializes the data source used for this webui.
  void InitDataSource();

  DISALLOW_COPY_AND_ASSIGN(StartPageUI);
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_WEBUI_APP_LIST_START_PAGE_UI_H_
