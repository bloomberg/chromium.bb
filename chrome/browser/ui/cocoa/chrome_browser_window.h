// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_CHROME_BROWSER_WINDOW_H_
#define CHROME_BROWSER_UI_COCOA_CHROME_BROWSER_WINDOW_H_
#pragma once

#import <Cocoa/Cocoa.h>

#include "chrome/browser/ui/cocoa/chrome_event_processing_window.h"

// Common base class for chrome browser windows.  Contains methods relating to
// theming and hole punching that are shared between framed and fullscreen
// windows.
@interface ChromeBrowserWindow : ChromeEventProcessingWindow {
 @private
  int underlaySurfaceCount_;
}

// Informs the window that an underlay surface has been added/removed. The
// window is non-opaque while underlay surfaces are present.
- (void)underlaySurfaceAdded;
- (void)underlaySurfaceRemoved;

@end

#endif  // CHROME_BROWSER_UI_COCOA_CHROME_BROWSER_WINDOW_H_
