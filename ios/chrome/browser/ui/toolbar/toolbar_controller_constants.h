// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TOOLBAR_TOOLBAR_CONTROLLER_CONSTANTS_H_
#define IOS_CHROME_BROWSER_UI_TOOLBAR_TOOLBAR_CONTROLLER_CONSTANTS_H_

#import <Foundation/Foundation.h>

#import <CoreGraphics/CoreGraphics.h>
#include "ios/chrome/browser/ui/rtl_geometry.h"
#include "ios/chrome/browser/ui/ui_util.h"

// The time delay before non-initial button images are loaded.
extern const int64_t kNonInitialImageAdditionDelayNanosec;

// Notification that the tools menu has been requested to be shown.
extern NSString* const kMenuWillShowNotification;
// Notification that the tools menu is closed.
extern NSString* const kMenuWillHideNotification;

// Accessibility identifier of the toolbar view.
extern NSString* const kToolbarIdentifier;
// Accessibility identifier of the incognito toolbar view.
extern NSString* const kIncognitoToolbarIdentifier;
// Accessibility identifier of the tools menu button.
extern NSString* const kToolbarToolsMenuButtonIdentifier;
// Accessibility identifier of the stack button.
extern NSString* const kToolbarStackButtonIdentifier;
// Accessibility identifier of the share button.
extern NSString* const kToolbarShareButtonIdentifier;

// The maximum number to display in the tab switcher button.
extern NSInteger const kStackButtonMaxTabCount;

// Animation key used for stack view transition animations
extern NSString* const kToolbarTransitionAnimationKey;

// Font sizes for the button containing the tab count
extern const NSInteger kFontSizeFewerThanTenTabs;
extern const NSInteger kFontSizeTenTabsOrMore;

// The initial capacity used to construct |self.transitionLayers|.  The value
// is chosen because WebToolbarController animates 11 separate layers during
// transitions; this value should be updated if new subviews are animated in
// the future.
extern const NSUInteger kTransitionLayerCapacity;

// Toolbar frames shared with subclasses.
extern const CGRect kToolbarFrame[INTERFACE_IDIOM_COUNT];
// UI frames.  iPhone values followed by iPad values.
// Full-width frames that don't change for RTL languages.
extern const CGRect kBackgroundViewFrame[INTERFACE_IDIOM_COUNT];
extern const CGRect kShadowViewFrame[INTERFACE_IDIOM_COUNT];
// Full bleed shadow frame is iPhone-only
extern const CGRect kFullBleedShadowViewFrame;

// Color constants for the stack button text, normal and pressed states.  These
// arrays are indexed by ToolbarControllerStyle enum values.
extern const CGFloat kStackButtonNormalColors[];
extern const int kStackButtonHighlightedColors[];

// Frames that change for RTL.
extern const LayoutRect kStackButtonFrame;
extern const LayoutRect kShareMenuButtonFrame;
extern const LayoutRect kToolsMenuButtonFrame[INTERFACE_IDIOM_COUNT];

// Distance to shift buttons when fading out.
extern const LayoutOffset kButtonFadeOutXOffset;

// The amount of horizontal padding removed from a view's frame when presenting
// a popover anchored to it.
extern const CGFloat kPopoverAnchorHorizontalPadding;

// Toolbar style.  Determines which button images are used.
enum ToolbarControllerStyle {
  ToolbarControllerStyleLightMode = 0,
  ToolbarControllerStyleDarkMode,
  ToolbarControllerStyleIncognitoMode,
  ToolbarControllerStyleMaxStyles
};
enum ToolbarButtonMode {
  ToolbarButtonModeNormal,
  ToolbarButtonModeReversed,
};
enum ToolbarButtonUIState {
  ToolbarButtonUIStateNormal = 0,
  ToolbarButtonUIStatePressed,
  ToolbarButtonUIStateDisabled,
  NumberOfToolbarButtonUIStates,
};

// This enumerates the different buttons used by the toolbar and is used to map
// the resource IDs for the button's icons.  Subclasses with additional buttons
// should extend these values.  The first new enum should be set to
// |NumberOfToolbarButtonNames|.  Note that functions that use these values use
// an int rather than the |ToolbarButtonName| to accommodate additional values.
// Also, if this enum is extended by a subclass, the subclass must also override
// -imageIdForImageEnum:style:forState: to provide mapping from enum to resource
// ID for the various states.
enum ToolbarButtonName {
  ToolbarButtonNameStack = 0,
  ToolbarButtonNameShare,
  NumberOfToolbarButtonNames,
};

// Style used to specify the direction of the toolbar transition animations.
typedef enum {
  TOOLBAR_TRANSITION_STYLE_TO_STACK_VIEW,
  TOOLBAR_TRANSITION_STYLE_TO_BVC
} ToolbarTransitionStyle;

#endif  // IOS_CHROME_BROWSER_UI_TOOLBAR_TOOLBAR_CONTROLLER_CONSTANTS_H_
