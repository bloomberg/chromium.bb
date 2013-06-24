// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_CONSTRAINED_WINDOW_CONSTRAINED_WINDOW_SHEET_CONTROLLER_H_
#define CHROME_BROWSER_UI_COCOA_CONSTRAINED_WINDOW_CONSTRAINED_WINDOW_SHEET_CONTROLLER_H_

#import <Cocoa/Cocoa.h>
#include <vector>

#include "base/mac/scoped_nsobject.h"
#include "base/memory/scoped_vector.h"

@protocol ConstrainedWindowSheet;

// This class manages multiple tab modal sheets for a single parent window. Each
// tab can have a single sheet and only the active tab's sheet will be visible.
// A tab in this case is the |parentView| passed to |-showSheet:forParentView:|.
@interface ConstrainedWindowSheetController : NSObject {
 @private
  base::scoped_nsobject<NSMutableArray> sheets_;
  base::scoped_nsobject<NSWindow> parentWindow_;
  base::scoped_nsobject<NSView> activeView_;
}

// Returns a sheet controller for |parentWindow|. If a sheet controller does not
// exist yet then one will be created.
+ (ConstrainedWindowSheetController*)
    controllerForParentWindow:(NSWindow*)parentWindow;

// Find a controller that's managing the given sheet. If no such controller
// exists then nil is returned.
+ (ConstrainedWindowSheetController*)
    controllerForSheet:(id<ConstrainedWindowSheet>)sheet;

// Find the sheet attached to the given overlay window.
+ (id<ConstrainedWindowSheet>)sheetForOverlayWindow:(NSWindow*)overlayWindow;

// Shows the given sheet over |parentView|. If |parentView| is not the active
// view then the sheet is not shown until the |parentView| becomes active.
- (void)showSheet:(id<ConstrainedWindowSheet>)sheet
    forParentView:(NSView*)parentView;

// Calculates the position of the sheet for the given window size.
- (NSPoint)originForSheet:(id<ConstrainedWindowSheet>)sheet
           withWindowSize:(NSSize)size;

// Closes the given sheet.
- (void)closeSheet:(id<ConstrainedWindowSheet>)sheet;

// Make |parentView| the current active view. If |parentView| has an attached
// sheet then the sheet is made visible.
- (void)parentViewDidBecomeActive:(NSView*)parentView;

// Run a pulse animation for the given sheet. This does nothing if the sheet
// is not visible.
- (void)pulseSheet:(id<ConstrainedWindowSheet>)sheet;

// Gets the number of sheets attached to the controller's window.
- (int)sheetCount;

@end

#endif  // CHROME_BROWSER_UI_COCOA_CONSTRAINED_WINDOW_CONSTRAINED_WINDOW_SHEET_CONTROLLER_H_
