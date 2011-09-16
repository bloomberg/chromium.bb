// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROME_WEB_UI_H_
#define CHROME_BROWSER_UI_WEBUI_CHROME_WEB_UI_H_
#pragma once

#include "content/browser/webui/web_ui.h"

class Profile;

class ChromeWebUI : public WebUI {
 public:
  explicit ChromeWebUI(TabContents* contents);
  virtual ~ChromeWebUI();

  // Returns the profile for this WebUI.
  Profile* GetProfile() const;

  // Returns true if the bookmark bar should be forced to being visible,
  // overriding the user's preference.
  bool force_bookmark_bar_visible() const {
    return force_bookmark_bar_visible_;
  }

 protected:
  void set_force_bookmark_bar_visible(bool value) {
    force_bookmark_bar_visible_ = value;
  }

 private:
  bool force_bookmark_bar_visible_;

  DISALLOW_COPY_AND_ASSIGN(ChromeWebUI);
};

#endif  // CHROME_BROWSER_UI_WEBUI_CHROME_WEB_UI_H_
