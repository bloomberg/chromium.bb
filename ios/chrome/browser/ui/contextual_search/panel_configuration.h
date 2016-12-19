// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTEXTUAL_SEARCH_PANEL_CONFIGURATION_H_
#define IOS_CHROME_BROWSER_UI_CONTEXTUAL_SEARCH_PANEL_CONFIGURATION_H_

#import <UIKit/UIKit.h>

namespace ContextualSearch {

// Possible states (static positions) of the panel.
typedef NS_ENUM(NSInteger, PanelState) {
  // TODO(crbug.com/546210): Rename to match Android implementation.
  // Ordering matters for these values.
  UNDEFINED = -1,
  DISMISSED,   // (CLOSED) Offscreen
  PEEKING,     // (PEEKED) Showing a small amount at the bottom of the screen
  PREVIEWING,  // (EXPANDED) Panel covers 2/3 of the tab.
  COVERING,    // (MAXIMIZED) Panel covers entire tab.
};

}  // namespace ContextualSearch

// A PanelConfiguration object manages the panel's size, and the mapping between
// states and y-coordinates for the panel. Nothing in the configuration object
// changes the position or state of the panel; it just provides information for
// other classes that make such changes.
// PanelConfiguration is an abstract superclass; one of the device-specific
// classes should be instantiated instead.
@interface PanelConfiguration : NSObject

// Size of the view that contains the panel.
@property(nonatomic, assign) CGSize containerSize;
// Current horizontal size class of the view that contains the panel.
@property(nonatomic, assign) UIUserInterfaceSizeClass horizontalSizeClass;
// Height that the panel peeks into the containing view when peeking.
@property(nonatomic, readonly) CGFloat peekingHeight;

// Creates and returns a configuration object for a container of the given size.
+ (instancetype)configurationForContainerSize:(CGSize)containerSize
                          horizontalSizeClass:
                              (UIUserInterfaceSizeClass)horizontalSizeClass;

// Given the current configuration, returns the y-coordinate for the top of the
// panel in |state|. This can be called "the position for |state|".
- (CGFloat)positionForPanelState:(ContextualSearch::PanelState)state;

// Given the current configuration, returns the state for a panel whose top
// edge is at |positions|. The range of positions for a state are the half-
// closed interval (J .. I], where I is position for |state| and J is the
// position for |state| + 1. (Note that positions decrease, moving towards
// the y-origin, as states increase).
- (ContextualSearch::PanelState)panelStateForPosition:(CGFloat)position;

// Returns the gradation (a value in [0.0 .. 1.0]) of |position| in terms of the
// distance from |fromState| to |toState|. A gradation of 0.0 means |position|
// is equal to the position for |fromState|; a gradation of 1.0 means it is
// equal to the position for |toState|, and intermediate values correspond
// linearly to intermediate values.
- (CGFloat)gradationToState:(ContextualSearch::PanelState)toState
                  fromState:(ContextualSearch::PanelState)fromState
                 atPosition:(CGFloat)position;

// Given |panel|, a view being used as the panel that the receiver configures,
// return constraints to size the panel as defined by the receiver.
- (NSArray*)constraintsForSizingPanel:(UIView*)panel;

// Given a |guide|, which must have a non-nil owning view, constructs and
// returns an autoreleased layout constraint that correctly sets the height of
// |guide| for |guide| to act as a positioning guide for a panel in |state|.
- (NSLayoutConstraint*)
constraintForPositioningGuide:(UILayoutGuide*)view
                      atState:(ContextualSearch::PanelState)state;

@end

// Panel configuration for phone formats.
@interface PhonePanelConfiguration : PanelConfiguration
@end

// Panel configuration for iPad interface idioms.
@interface PadPanelConfiguration : PanelConfiguration
@end

#endif  // IOS_CHROME_BROWSER_UI_CONTEXTUAL_SEARCH_PANEL_CONFIGURATION_H_
