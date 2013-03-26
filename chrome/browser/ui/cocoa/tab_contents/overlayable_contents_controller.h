// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_TAB_CONTENTS_OVERLAYABLE_CONTENTS_CONTROLLER_H_
#define CHROME_BROWSER_UI_COCOA_TAB_CONTENTS_OVERLAYABLE_CONTENTS_CONTROLLER_H_

#import <Cocoa/Cocoa.h>

#include "base/memory/scoped_nsobject.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/common/instant_types.h"

class Browser;
@class BrowserWindowController;
class InstantOverlayControllerMac;

namespace content {
class WebContents;
}

// OverlayableContentsController manages the display of up to two tab contents
// views. It is primarily for use with Instant results. This class supports the
// notion of an "active" view vs. an "overlay" tab contents view.
//
// The "active" view is a container view that can be retrieved using
// |-activeContainer|. Its contents are meant to be managed by an external
// class.
//
// The "overlay" can be set using |-showOverlay:| and |-hideOverlay|. When an
// overlay is set, the active view is hidden (but stays in the view hierarchy).
// When the overlay is removed, the active view is reshown.
@interface OverlayableContentsController : NSViewController {
 @private
  // Container view for the "active" contents.
  scoped_nsobject<NSView> activeContainer_;

  // The overlay WebContents. Will be NULL if no overlay is currently showing.
  content::WebContents* overlayContents_;  // weak

  // C++ bridge to the Instant model change interface.
  scoped_ptr<InstantOverlayControllerMac> instantOverlayController_;

  // The desired height of the overlay and units.
  CGFloat overlayHeight_;
  InstantSizeUnits overlayHeightUnits_;

  // If true then a shadow is drawn below the overlay. This is used to make the
  // overlay "float" over the tab's web contents.
  BOOL drawDropShadow_;

  // View responsible for drawing a drop shadow.
  scoped_nsobject<NSView> dropShadowView_;

  BrowserWindowController* windowController_;

  // The vertical offset between the top of the view and the active container.
  // This is used to push the active container below the bookmark bar. Normally
  // this is set to the height of the bookmark bar so that the bookmark bar is
  // not obscured.
  CGFloat activeContainerOffset_;
}

@property(readonly, nonatomic) NSView* activeContainer;
@property(readonly, nonatomic) NSView* dropShadowView;
@property(readonly, nonatomic) BOOL drawDropShadow;
@property(assign, nonatomic) CGFloat activeContainerOffset;

// Initialization.
- (id)initWithBrowser:(Browser*)browser
     windowController:(BrowserWindowController*)windowController;

// Sets the current overlay and installs its WebContentsView into the view
// hierarchy. Hides the active view. If |overlay| is NULL then closes the
// current overlay and shows the active view.
- (void)setOverlay:(content::WebContents*)overlay
            height:(CGFloat)height
       heightUnits:(InstantSizeUnits)heightUnits
    drawDropShadow:(BOOL)drawDropShadow;

// Called when a tab with |contents| is activated, so that we can check to see
// if it's the overlay being activated (and adjust internal state accordingly).
- (void)onActivateTabWithContents:(content::WebContents*)contents;

// Returns YES if the overlay contents is currently showing.
- (BOOL)isShowingOverlay;

- (InstantOverlayControllerMac*)instantOverlayController;

@end

#endif  // CHROME_BROWSER_UI_COCOA_TAB_CONTENTS_OVERLAYABLE_CONTENTS_CONTROLLER_H_
