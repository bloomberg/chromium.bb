// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_OVERLAY_PANEL_CONTROLLER_H_
#define CHROME_BROWSER_UI_COCOA_OVERLAY_PANEL_CONTROLLER_H_
#pragma once

#import <Cocoa/Cocoa.h>

#include "base/memory/scoped_nsobject.h"

@class HistoryOverlayView;

enum HistoryOverlayMode {
  kHistoryOverlayModeBack,
  kHistoryOverlayModeForward
};

// The HistoryOverlayController manages a view that is inserted atop the web
// contents to provide visual feedback when the user is performing history
// navigation gestures.
@interface HistoryOverlayController : NSViewController {
 @private
  HistoryOverlayMode mode_;
  // Strongly typed reference of self.view.
  scoped_nsobject<HistoryOverlayView> contentView_;
  // The view above which self.view is inserted as a subview.
  scoped_nsobject<NSView> parent_;
}

// Designated initializer.
- (id)initForMode:(HistoryOverlayMode)mode;

// Shows the shield above |view|.
- (void)showPanelForView:(NSView*)view;

// Updates the appearance of the overlay based on track gesture progress.
- (void)setProgress:(CGFloat)gestureAmount;

// Fades the shield out and removes it from the view hierarchy.
- (void)dismiss;

@end

#endif  // CHROME_BROWSER_UI_COCOA_OVERLAY_PANEL_CONTROLLER_H_
