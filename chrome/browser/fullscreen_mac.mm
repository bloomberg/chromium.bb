// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/fullscreen.h"

#import <Cocoa/Cocoa.h>

// Replicate specific 10.7 SDK declarations for building with prior SDKs.
#if !defined(MAC_OS_X_VERSION_10_7) || \
    MAC_OS_X_VERSION_MAX_ALLOWED < MAC_OS_X_VERSION_10_7

enum {
  NSApplicationPresentationFullScreen = 1 << 10
};

#endif  // MAC_OS_X_VERSION_10_7

bool IsFullScreenMode() {
  // Check if the main display has been captured (by games in particular).
  if (CGDisplayIsCaptured(CGMainDisplayID()))
    return true;

  NSApplicationPresentationOptions options =
      [NSApp currentSystemPresentationOptions];

  bool dock_hidden = (options & NSApplicationPresentationHideDock) ||
                     (options & NSApplicationPresentationAutoHideDock);

  bool menu_hidden = (options & NSApplicationPresentationHideMenuBar) ||
                     (options & NSApplicationPresentationAutoHideMenuBar);

  // If both dock and menu bar are hidden, that is the equivalent of the Carbon
  // SystemUIMode (or Info.plist's LSUIPresentationMode) kUIModeAllHidden.
  if (dock_hidden && menu_hidden)
    return true;

  if (options & NSApplicationPresentationFullScreen)
    return true;

  return false;
}
