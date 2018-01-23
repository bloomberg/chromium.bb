// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/browser_window_views_mac.h"

// Currently this file is only linked in when all browser windows are views, so
// the implementations just return null.

BrowserWindowController* BrowserWindowControllerForWindow(NSWindow* window) {
  return nullptr;
}

TabWindowController* TabWindowControllerForWindow(NSWindow* window) {
  return nullptr;
}
