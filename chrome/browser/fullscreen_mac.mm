// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/fullscreen.h"

#import <Cocoa/Cocoa.h>

#include "base/command_line.h"
#include "base/mac/sdk_forward_declarations.h"
#include "chrome/common/chrome_switches.h"

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

namespace chrome {
namespace mac {

bool SupportsSystemFullscreen() {
  // TODO(spqchan): Clean this up, https://crbug.com/579259
  return true;
}

}  // namespace mac
}  // namespace chrome
