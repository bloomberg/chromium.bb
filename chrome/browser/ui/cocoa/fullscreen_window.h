// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <Cocoa/Cocoa.h>
#import "chrome/browser/ui/cocoa/chrome_browser_window.h"

// A FullscreenWindow is a borderless window suitable for going fullscreen.  The
// returned window is NOT release when closed and is not initially visible.
// FullscreenWindow derives from ChromeBrowserWindow to inherit hole punching,
// theming methods, and special event handling
// (e.g. handleExtraKeyboardShortcut).
@interface FullscreenWindow : ChromeBrowserWindow

// Initialize a FullscreenWindow for the given screen.
// Designated initializer.
- (id)initForScreen:(NSScreen*)screen;

@end
