// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COCOA_TAB_STRIP_VIEW_H_
#define CHROME_BROWSER_COCOA_TAB_STRIP_VIEW_H_

#import <Cocoa/Cocoa.h>

#include "base/scoped_nsobject.h"
#import "chrome/browser/cocoa/url_drop_target.h"

// A view class that handles rendering the tab strip

@interface TabStripView : NSView {
 @private
  NSTimeInterval lastMouseUp_;

  // Handles being a drag-and-drop target.
  scoped_nsobject<URLDropTargetHandler> dropHandler_;

  // Weak; the following come from the nib.
  NSButton* newTabButton_;
}

@property(assign, nonatomic) IBOutlet NSButton* newTabButton;

@end

#endif  // CHROME_BROWSER_COCOA_TAB_STRIP_VIEW_H_
