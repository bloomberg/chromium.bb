// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_TABS_TAB_VIEW_H_
#define CHROME_BROWSER_UI_COCOA_TABS_TAB_VIEW_H_

#include <ApplicationServices/ApplicationServices.h>
#import <Cocoa/Cocoa.h>

#include "base/mac/scoped_cftyperef.h"
#include "base/mac/scoped_nsobject.h"
#import "chrome/browser/ui/cocoa/hover_close_button.h"
#import "chrome/browser/ui/cocoa/themed_window.h"

namespace tabs {

// Nomenclature:
// Tabs _glow_ under two different circumstances, when they are _hovered_ (by
// the mouse) and when they are _alerted_ (to show that the tab's title has
// changed).

// The state of alerting (to show a title change on an unselected, pinned tab).
// This is more complicated than a simple on/off since we want to allow the
// alert glow to go through a full rise-hold-fall cycle to avoid flickering (or
// always holding).
enum AlertState {
  kAlertNone = 0,  // Obj-C initializes to this.
  kAlertRising,
  kAlertHolding,
  kAlertFalling,
  kAlertOff
};

// When the window doesn't have focus then we want to draw the button with a
// slightly lighter color. We do this by just reducing the alpha.
const CGFloat kImageNoFocusAlpha = 0.65;

// The default COLOR_TAB_TEXT color.
const SkColor kDefaultTabTextColor = SkColorSetARGB(0xA0, 0x00, 0x00, 0x00);

}  // namespace tabs

@class TabController, TabWindowController, GTMFadeTruncatingTextFieldCell;

// A view that handles the event tracking (clicking and dragging) for a tab
// on the tab strip. Relies on an associated TabController to provide a
// target/action for selecting the tab.

@interface TabView : NSControl<ThemedWindowDrawing> {
 @private
  TabController* controller_;
  base::scoped_nsobject<NSTextField> titleView_;
  GTMFadeTruncatingTextFieldCell* titleViewCell_;  // weak

  // TODO(rohitrao): Add this button to a CoreAnimation layer so we can fade it
  // in and out on mouseovers.
  HoverCloseButton* closeButton_;  // Weak.

  BOOL closing_;

  BOOL isMouseInside_;  // Is the mouse hovering over?
  BOOL isInfiniteAlert_;  // Valid only when alertState_ != kAlertNone.
  tabs::AlertState alertState_;

  CGFloat hoverAlpha_;  // How strong the hover glow is.
  NSTimeInterval hoverHoldEndTime_;  // When the hover glow will begin dimming.

  CGFloat alertAlpha_;  // How strong the alert glow is.
  NSTimeInterval alertHoldEndTime_;  // When the hover glow will begin dimming.

  NSTimeInterval lastGlowUpdate_;  // Time either glow was last updated.

  NSPoint hoverPoint_;  // Current location of hover in view coords.

  // The location of the current mouseDown event in window coordinates.
  NSPoint mouseDownPoint_;

  NSCellStateValue state_;

  // The tool tip text for this tab view.
  base::scoped_nsobject<NSString> toolTipText_;
}

@property(readonly, nonatomic) BOOL isActiveTab;
@property(retain, nonatomic) NSString* title;
@property(assign, nonatomic) NSRect titleFrame;
@property(retain, nonatomic) NSColor* titleColor;
@property(assign, nonatomic) BOOL titleHidden;

// The state affects how the tab will be drawn.
// NSOnState    -> active
// NSMixedState -> selected
// NSOffState   -> none
@property(assign, nonatomic) NSCellStateValue state;

@property(assign, nonatomic) CGFloat hoverAlpha;
@property(assign, nonatomic) CGFloat alertAlpha;

// Determines if the tab is in the process of animating closed. It may still
// be visible on-screen, but should not respond to/initiate any events. Upon
// setting to NO, clears the target/action of the close button to prevent
// clicks inside it from sending messages.
@property(assign, nonatomic, getter=isClosing) BOOL closing;

// The tool tip text for this tab view.
@property(copy, nonatomic) NSString* toolTipText;

// Designated initializer.
- (id)initWithFrame:(NSRect)frame
         controller:(TabController*)controller
        closeButton:(HoverCloseButton*)closeButton;

// Enables/Disables tracking regions for the tab.
- (void)setTrackingEnabled:(BOOL)enabled;

// Begin showing an "alert" glow (shown to call attention to an unselected
// pinned tab whose title changed). This glow cycles once and automatically
// stops.
- (void)startOnceAlert;

// Begin showing an "alert" glow (shown to call attention to an alert dialog).
// This glow cycles until stopped by -cancelAlert.
- (void)startInfiniteAlert;

// Stop showing the "alert" glow; this won't immediately wipe out any glow, but
// will make it fade away.
- (void)cancelAlert;

// Returns the width of the largest part of the tab that is available for the
// user to click to select/activate the tab.
- (int)widthOfLargestSelectableRegion;

// Returns the Material Design color of the icons. Used by the alert indicator,
// the "x", and the default favicon.
- (SkColor)iconColor;

// Called when systemwide accessibility options change.
- (void)accessibilityOptionsDidChange:(id)ignored;

@end

// The TabController |controller_| is not the only owner of this view. If the
// controller is released before this view, then we could be hanging onto a
// garbage pointer. To prevent this, the TabController uses this interface to
// clear the |controller_| pointer when it is dying.
@interface TabView (TabControllerInterface)
- (void)setController:(TabController*)controller;
@end

#endif  // CHROME_BROWSER_UI_COCOA_TABS_TAB_VIEW_H_
