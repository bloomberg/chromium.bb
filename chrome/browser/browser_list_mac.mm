// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_list.h"

#include "chrome/browser/browser_shutdown.h"
#import "chrome/browser/chrome_browser_application_mac.h"

// static
void BrowserList::AllBrowsersClosed() {
  // Only terminate after all browsers are closed if trying quit.
  if (browser_shutdown::IsTryingToQuit())
    chrome_browser_application_mac::Terminate();
}
