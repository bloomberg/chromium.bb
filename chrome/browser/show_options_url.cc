// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/show_options_url.h"

#include "chrome/browser/browser.h"
#include "chrome/browser/browser_window.h"
#include "chrome/browser/profile.h"

namespace browser {

void ShowOptionsURL(Profile* profile, const GURL& url) {
  // We open a new browser window so the Options dialog doesn't get lost behind
  // other windows.
  Browser* browser = Browser::Create(profile);
  browser->AddSelectedTabWithURL(url, PageTransition::LINK);
  browser->window()->Show();
}

}  // namespace browser
