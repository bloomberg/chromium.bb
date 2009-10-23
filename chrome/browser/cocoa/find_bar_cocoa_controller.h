// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#import "chrome/browser/cocoa/find_bar_cocoa_controller.h"

#include "base/scoped_nsobject.h"
#include "base/string16.h"

class BrowserWindowCocoa;
class FindBarBridge;
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
  IBOutlet NSTextField* findText_;
  IBOutlet NSTextField* resultsLabel_;
  IBOutlet NSButton* nextButton_;
  IBOutlet NSButton* previousButton_;

  // Needed to call methods on FindBarController.
  FindBarBridge* findBarBridge_;  // weak

  scoped_nsobject<FocusTracker> focusTracker_;

  // The currently-running animation.  This is defined to be non-nil if an
  // animation is running, and is always nil otherwise.  The
  // FindBarCocoaController should not be deallocated while an animation is
  // running (stopAnimation is currently called before the last tab in a
  // window is removed).
  scoped_nsobject<NSViewAnimation> currentAnimation_;

  // If YES, do nothing as a result of find pasteboard update notifications.
  BOOL suppressPboardUpdateActions_;
};

// Initializes a new FindBarCocoaController.
- (id)init;

- (void)setFindBarBridge:(FindBarBridge*)findBar;

- (IBAction)close:(id)sender;

- (IBAction)nextResult:(id)sender;

- (IBAction)previousResult:(id)sender;

// Positions the find bar based on the location of the infobar container.
// TODO(rohitrao): Move this logic into BrowserWindowController.
- (void)positionFindBarView:(NSView*)infoBarContainerView;

// Methods called from FindBarBridge.
- (void)showFindBar:(BOOL)animate;
- (void)hideFindBar:(BOOL)animate;
- (void)stopAnimation;
- (void)setFocusAndSelection;
- (void)restoreSavedFocus;
- (void)setFindText:(NSString*)findText;

- (void)clearResults:(const FindNotificationDetails&)results;
- (void)updateUIForFindResult:(const FindNotificationDetails&)results
                     withText:(const string16&)findText;
- (BOOL)isFindBarVisible;

@end
