// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_list.h"

#include "chrome/browser/browser_shutdown.h"
#import "chrome/browser/chrome_browser_application_mac.h"

// static
void BrowserList::AllBrowsersClosedAndAppExiting() {
  // Last browser is closed, so call back to controller to shutdown the app.
  chrome_browser_application_mac::Terminate();
}
