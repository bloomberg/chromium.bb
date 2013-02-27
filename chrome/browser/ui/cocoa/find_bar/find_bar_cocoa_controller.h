// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "base/memory/scoped_nsobject.h"
#include "base/string16.h"
#include "ui/gfx/point.h"

@class BrowserWindowController;
class FindBarBridge;
@class FindBarTextField;
class FindNotificationDetails;
@class FocusTracker;

// A controller for the find bar in the browser window.  Manages
// updating the state of the find bar and provides a target for the
// next/previous/close buttons.  Certain operations require a pointer
// to the cross-platform FindBarController, so be sure to call
// setFindBarBridge: after creating this controller.

@interface FindBarCocoaController : NSViewController {
 @private
  IBOutlet NSView* findBarView_;
  IBOutlet FindBarTextField* findText_;
  IBOutlet NSButton* nextButton_;
  IBOutlet NSButton* previousButton_;
  IBOutlet NSButton* closeButton_;

  // Needed to call methods on FindBarController.
  FindBarBridge* findBarBridge_;  // weak

  // Needed to request a layout of the FindBar view.
  BrowserWindowController* browserWindowController_;  // weak

  scoped_nsobject<FocusTracker> focusTracker_;

  // The show/hide animation. This is defined to be non-nil if the
  // animation is running, and is always nil otherwise.  The
  // FindBarCocoaController should not be deallocated while an animation is
  // running (stopAnimation is currently called before the last tab in a
  // window is removed).
  scoped_nsobject<NSViewAnimation> showHideAnimation_;

  // The horizontal-moving animation, to avoid occluding find results. This
  // is nil when the animation is not running, and is also stopped by
  // stopAnimation.
  scoped_nsobject<NSViewAnimation> moveAnimation_;

  // If YES, do nothing as a result of find pasteboard update notifications.
  BOOL suppressPboardUpdateActions_;

  // Vertical point of attachment of the FindBar.
  CGFloat maxY_;

  // Default width of FindBar.
  CGFloat defaultWidth_;
};

// Initializes a new FindBarCocoaController.
- (id)init;

- (void)setFindBarBridge:(FindBarBridge*)findBar;
- (void)setBrowserWindowController:(BrowserWindowController*)controller;

- (IBAction)close:(id)sender;

- (IBAction)nextResult:(id)sender;

- (IBAction)previousResult:(id)sender;

// Position the find bar at the given maximum y-coordinate (the min-y of the
// bar -- toolbar + possibly bookmark bar, but not including the infobars) with
// the given maximum width (i.e., the find bar should fit between 0 and
// |maxWidth|).
- (void)positionFindBarViewAtMaxY:(CGFloat)maxY maxWidth:(CGFloat)maxWidth;

// Methods called from FindBarBridge.
- (void)showFindBar:(BOOL)animate;
- (void)hideFindBar:(BOOL)animate;
- (void)stopAnimation;
- (void)setFocusAndSelection;
- (void)restoreSavedFocus;
- (NSString*)findText;
- (void)setFindText:(NSString*)findText;
- (NSString*)matchCountText;
- (void)updateFindBarForChangedWebContents;

- (void)clearResults:(const FindNotificationDetails&)results;
- (void)updateUIForFindResult:(const FindNotificationDetails&)results
                     withText:(const string16&)findText;
- (BOOL)isFindBarVisible;
- (BOOL)isFindBarAnimating;

// Returns the FindBar's position in the superview's coordinates, but with
// the Y coordinate growing down.
- (gfx::Point)findBarWindowPosition;

// Returns the width of the FindBar.
- (int)findBarWidth;

@end
