// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#ifndef CHROME_BROWSER_UI_WEBUI_ABOUT_PAGE_ABOUT_PAGE_UI_H_
#define CHROME_BROWSER_UI_WEBUI_ABOUT_PAGE_ABOUT_PAGE_UI_H_
#pragma once

#include "content/browser/webui/web_ui.h"

class AboutPageUI : public WebUI {
 public:
  explicit AboutPageUI(content::WebContents* contents);
  virtual ~AboutPageUI();

 private:
  DISALLOW_COPY_AND_ASSIGN(AboutPageUI);
};

#endif  // CHROME_BROWSER_UI_WEBUI_ABOUT_PAGE_ABOUT_PAGE_UI_H_
