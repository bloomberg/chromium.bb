// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_TAB_CONTENTS_PREVIEWABLE_CONTENTS_CONTROLLER_H_
#define CHROME_BROWSER_UI_COCOA_TAB_CONTENTS_PREVIEWABLE_CONTENTS_CONTROLLER_H_

#import <Cocoa/Cocoa.h>

#include "base/memory/scoped_nsobject.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/common/instant_types.h"

class Browser;
@class BrowserWindowController;
class InstantPreviewControllerMac;

namespace content {
class WebContents;
}

// PreviewableContentsController manages the display of up to two tab contents
// views.  It is primarily for use with Instant results.  This class supports
// the notion of an "active" view vs. a "preview" tab contents view.
//
// The "active" view is a container view that can be retrieved using
// |-activeContainer|.  Its contents are meant to be managed by an external
// class.
//
// The "preview" can be set using |-showPreview:| and |-hidePreview|.  When a
// preview is set, the active view is hidden (but stays in the view hierarchy).
// When the preview is removed, the active view is reshown.
@interface PreviewableContentsController : NSViewController {
 @private
  // Container view for the "active" contents.
  scoped_nsobject<NSView> activeContainer_;

  // The preview WebContents.  Will be NULL if no preview is currently showing.
  content::WebContents* previewContents_;  // weak

  // C++ bridge to the Instant model change interface.
  scoped_ptr<InstantPreviewControllerMac> instantPreviewController_;

  // The desired height of the preview and units.
  CGFloat previewHeight_;
  InstantSizeUnits previewHeightUnits_;

  // If true then a shadow is drawn below the preview. This is used to make
  // instant omnibox "float" over the tab's web contents.
  BOOL drawDropShadow_;

  // View responsible for drawing a drop shadow.
  scoped_nsobject<NSView> dropShadowView_;
}

@property(readonly, nonatomic) NSView* activeContainer;
@property(readonly, nonatomic) NSView* dropShadowView;
@property(readonly, nonatomic) BOOL drawDropShadow;

// Initialization.
- (id)initWithBrowser:(Browser*)browser
     windowController:(BrowserWindowController*)windowController;

// Sets the current preview and installs its WebContentsView into the view
// hierarchy.  Hides the active view.  |preview| must not be NULL.
- (void)showPreview:(content::WebContents*)preview
             height:(CGFloat)height
        heightUnits:(InstantSizeUnits)heightUnits
     drawDropShadow:(BOOL)drawDropShadow;

// Closes the current preview and shows the active view.
- (void)hidePreview;

// Called when a tab with |contents| is activated, so that we can check to see
// if it's the preview being activated (and adjust internal state accordingly).
- (void)onActivateTabWithContents:(content::WebContents*)contents;

- (InstantPreviewControllerMac*)instantPreviewController;

@end

#endif  // CHROME_BROWSER_UI_COCOA_TAB_CONTENTS_PREVIEWABLE_CONTENTS_CONTROLLER_H_
