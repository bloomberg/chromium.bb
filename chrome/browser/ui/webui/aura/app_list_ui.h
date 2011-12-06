// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_AURA_APP_LIST_UI_H_
#define CHROME_BROWSER_UI_WEBUI_AURA_APP_LIST_UI_H_
#pragma once

#include "chrome/browser/ui/webui/chrome_web_ui.h"

class AppListUIDelegate;

class AppListUI : public ChromeWebUI {
 public:
  explicit AppListUI(TabContents* contents);

  AppListUIDelegate* delegate() const {
    return delegate_;
  }

  void set_delegate(AppListUIDelegate* delegate) {
    delegate_ = delegate;
  }

 private:
  AppListUIDelegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(AppListUI);
};

#endif  // CHROME_BROWSER_UI_WEBUI_AURA_APP_LIST_UI_H_
