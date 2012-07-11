// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_HELP_HELP_UI_H_
#define CHROME_BROWSER_UI_WEBUI_HELP_HELP_UI_H_

#include "content/public/browser/web_ui_controller.h"

class HelpUI : public content::WebUIController {
 public:
  explicit HelpUI(content::WebUI* web_ui);
  virtual ~HelpUI();

 private:
  DISALLOW_COPY_AND_ASSIGN(HelpUI);
};

#endif  // CHROME_BROWSER_UI_WEBUI_HELP_HELP_UI_H_
