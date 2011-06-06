// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chrome_web_ui.h"

ChromeWebUI::ChromeWebUI(TabContents* contents)
    : WebUI(contents),
      force_bookmark_bar_visible_(false) {
}

ChromeWebUI::~ChromeWebUI() {
}

