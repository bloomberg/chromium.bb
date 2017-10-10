// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/toolbar/web_toolbar_controller_constants.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

const CGFloat kiPhoneOmniboxPlaceholderColorBrightness = 150 / 255.0;

const char* const kOmniboxQueryLocationAuthorizationStatusHistogram =
    "Omnibox.QueryIosLocationAuthorizationStatus";

const CGFloat kNTPBackgroundColorBrightness = 1.0;
const CGFloat kNTPBackgroundColorBrightnessIncognito = 34.0 / 255.0;

const CGFloat kiPadOmniboxPopupVerticalOffset = 3;

const CGFloat kExpandedOmniboxPadding = 6;

const CGFloat kBackButtonTrailingPadding = 7.0;
const CGFloat kForwardButtonTrailingPadding = -1.0;
const CGFloat kReloadButtonTrailingPadding = 4.0;

const CGFloat kCancelButtonBottomMargin = 4.0;
const CGFloat kCancelButtonTopMargin = 4.0;
const CGFloat kCancelButtonLeadingMargin = 7.0;
const CGFloat kCancelButtonWidth = 40.0;

const CGFloat kDeterminateProgressBarYOffset = 1.0;
const CGFloat kMaterialProgressBarHeight = 2.0;
const CGFloat kLoadCompleteHideProgressBarDelay = 0.5;
const LayoutOffset kPositionAnimationLeadingOffset = -10.0;

const CGFloat kIPadToolbarY = 53;
const CGFloat kScrollFadeDistance = 30;
const CGFloat kBackgroundImageVisibleRectOffset = 6;

const CGFloat kWebToolbarWidths[INTERFACE_IDIOM_COUNT] = {224, 720};
// clang-format off
const LayoutRect kWebToolbarFrame[INTERFACE_IDIOM_COUNT] = {
  {kPortraitWidth[IPHONE_IDIOM], LayoutRectPositionZero,
    {kWebToolbarWidths[IPHONE_IDIOM], 56}},
  {kPortraitWidth[IPAD_IDIOM],   LayoutRectPositionZero,
    {kWebToolbarWidths[IPAD_IDIOM],   56}},
};

#define IPHONE_LAYOUT(L, Y, H, W) \
{ kWebToolbarWidths[IPHONE_IDIOM], {L, Y}, {H, W} }
#define IPAD_LAYOUT(L, Y, H, W) \
{ kWebToolbarWidths[IPAD_IDIOM], {L, Y}, {H, W} }

// Layouts inside the WebToolbar frame
const LayoutRect kOmniboxFrame[INTERFACE_IDIOM_COUNT] = {
  IPHONE_LAYOUT(55, 7, 169, 43), IPAD_LAYOUT(152, 7, 568, 43),
};
const LayoutRect kBackButtonFrame[INTERFACE_IDIOM_COUNT] = {
  IPHONE_LAYOUT(0, 4, 48, 48), IPAD_LAYOUT(4, 4, 48, 48),
};
const LayoutRect kForwardButtonFrame[INTERFACE_IDIOM_COUNT] = {
  IPHONE_LAYOUT(48, 4, 48, 48), IPAD_LAYOUT(52, 4, 48, 48),
};
// clang-format on

const LayoutRect kStopReloadButtonFrame = IPAD_LAYOUT(100, 4, 48, 48);
const LayoutRect kStarButtonFrame = IPAD_LAYOUT(644, 4, 36, 48);
const LayoutRect kVoiceSearchButtonFrame = IPAD_LAYOUT(680, 4, 36, 48);

const CGFloat kOmniboxCenterOffsetY = 0.5;

NSString* const kDotComTLD = @".com";

void RunBlockAfterDelay(NSTimeInterval delay, void (^block)(void)) {
  DCHECK([NSThread isMainThread]);
  dispatch_after(dispatch_time(DISPATCH_TIME_NOW, NSEC_PER_SEC * delay),
                 dispatch_get_main_queue(), block);
}

CGRect RectShiftedDownForStatusBar(CGRect rect) {
  if (IsIPadIdiom())
    return rect;
  rect.origin.y += StatusBarHeight();
  return rect;
}

CGRect RectShiftedUpForStatusBar(CGRect rect) {
  if (IsIPadIdiom())
    return rect;
  rect.origin.y -= StatusBarHeight();
  return rect;
}

CGRect RectShiftedUpAndResizedForStatusBar(CGRect rect) {
  if (IsIPadIdiom())
    return rect;
  rect.size.height += StatusBarHeight();
  return RectShiftedUpForStatusBar(rect);
}

CGRect RectShiftedDownAndResizedForStatusBar(CGRect rect) {
  if (IsIPadIdiom())
    return rect;
  rect.size.height -= StatusBarHeight();
  return RectShiftedDownForStatusBar(rect);
}
