// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chrome_web_ui.h"

#include "chrome/browser/profiles/profile.h"
#include "content/browser/tab_contents/tab_contents.h"

ChromeWebUI::ChromeWebUI(TabContents* contents)
    : WebUI(contents),
      force_bookmark_bar_visible_(false) {
}

ChromeWebUI::~ChromeWebUI() {
}

Profile* ChromeWebUI::GetProfile() const {
  return Profile::FromBrowserContext(tab_contents()->browser_context());
}
