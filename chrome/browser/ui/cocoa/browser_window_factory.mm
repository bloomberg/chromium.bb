// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "chrome/browser/ui/cocoa/browser_window_controller.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"

// Create the controller for the Browser, which handles loading the browser
// window from the nib. The controller takes ownership of |browser|.
// static
BrowserWindow* BrowserWindow::CreateBrowserWindow(Browser* browser) {
  BrowserWindowController* controller =
      [[BrowserWindowController alloc] initWithBrowser:browser];
  return [controller browserWindow];
}

// static
chrome::HostDesktopType BrowserWindow::AdjustHostDesktopType(
    chrome::HostDesktopType desktop_type) {
  return desktop_type;
}
