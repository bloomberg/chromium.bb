// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_OVERLAY_PANEL_CONTROLLER_H_
#define CHROME_BROWSER_UI_COCOA_OVERLAY_PANEL_CONTROLLER_H_
#pragma once

#import <Cocoa/Cocoa.h>

#import "base/mac/cocoa_protocols.h"
#include "base/memory/scoped_nsobject.h"

@class OverlayFrameView;

enum HistoryOverlayMode {
  kHistoryOverlayModeBack,
  kHistoryOverlayModeForward
};

// The HistoryOverlayController manages the overlay HUD panel which displays
// navigation gesture icons within a browser window.
@interface HistoryOverlayController : NSWindowController<NSWindowDelegate> {
 @private
  HistoryOverlayMode mode_;

  // The content view of the window that this controller manages.
  OverlayFrameView* contentView_;  // Weak, owned by the window.

  scoped_nsobject<NSWindow> parent_;
}

// Designated initializer.
- (id)initForMode:(HistoryOverlayMode)mode;

// Shows the panel.
- (void)showPanelForWindow:(NSWindow*)window;

// Updates the appearance of the overlay based on track gesture progress.
- (void)setProgress:(CGFloat)gestureAmount;

// Schedules a fade-out animation and then closes the window,
// which will release the controller.
- (void)dismiss;
@end

#endif  // CHROME_BROWSER_UI_COCOA_OVERLAY_PANEL_CONTROLLER_H_
