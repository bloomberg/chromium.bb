// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_CONSTRAINED_WINDOW_CONSTRAINED_WINDOW_SHEET_INFO_H_
#define CHROME_BROWSER_UI_COCOA_CONSTRAINED_WINDOW_CONSTRAINED_WINDOW_SHEET_INFO_H_

#import <Cocoa/Cocoa.h>

#include "base/memory/scoped_nsobject.h"

// Information about a single sheet managed by
// ConstrainedWindowSheetController.Note, this is a private class not meant for
// public use.
@interface ConstrainedWindowSheetInfo : NSObject {
 @private
  scoped_nsobject<NSWindow> sheet_;
  scoped_nsobject<NSView> parentView_;
  scoped_nsobject<NSWindow> overlayWindow_;
  NSRect oldSheetFrame_;
  BOOL oldSheetAutoresizesSubviews_;
}

@property(nonatomic, readonly) NSWindow* sheet;
@property(nonatomic, readonly) NSWindow* parentView;
@property(nonatomic, readonly) NSWindow* overlayWindow;

// Initializes a info object with for the given |sheet| and associated
// |parentView| and |overlayWindow|.
- (id)initWithSheet:(NSWindow*)sheet
         parentView:(NSView*)parentView
      overlayWindow:(NSWindow*)overlayWindow;

// Hides the sheet and the associated overlay window. Hiding is done in such
// a way as to not disturb the window cycle order.
- (void)hideSheet;

// Shows the sheet and the associated overlay window.
- (void)showSheet;

@end

#endif  // CHROME_BROWSER_UI_COCOA_CONSTRAINED_WINDOW_CONSTRAINED_WINDOW_SHEET_INFO_H_
