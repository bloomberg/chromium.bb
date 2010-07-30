// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COCOA_TABPOSE_WINDOW_H_
#define CHROME_BROWSER_COCOA_TABPOSE_WINDOW_H_
#pragma once

#import <Cocoa/Cocoa.h>

#include "base/scoped_cftyperef.h"

// A TabposeWindow shows an overview of open tabs and lets the user select a new
// active tab. The window blocks clicks on the tab strip and the download
// shelf. Every open browser window has its own overlay, and they are
// independent of each other.
@interface TabposeWindow : NSWindow {
 @private
  // The root layer added to the content view. Covers the whole window.
  CALayer* rootLayer_;  // weak

  // The layer showing the background layer. Covers the whole visible area.
  CALayer* bgLayer_;  // weak

  scoped_cftyperef<CGColorRef> gray_;
}

// Shows a TabposeWindow on top of |parent|, with |rect| being the active area.
// If |slomo| is YES, then the appearance animation is shown in slow motion.
// The window blocks all keyboard and mouse events and releases itself when
// closed.
+ (id)openTabposeFor:(NSWindow*)parent rect:(NSRect)rect slomo:(BOOL)slomo;
@end

#endif  // CHROME_BROWSER_COCOA_TABPOSE_WINDOW_H_

