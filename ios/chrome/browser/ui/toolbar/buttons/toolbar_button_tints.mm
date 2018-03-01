// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/ui/toolbar/buttons/toolbar_button_tints.h"

#include "base/logging.h"
#import "ios/chrome/browser/ui/ui_util.h"
#import "ios/chrome/browser/ui/uikit_ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace toolbar {

// The colors used to tint the buttons in the toolbar.
const int kLightModeNormalColor = 0x5A5A5A;
const int kIncognitoModeNormalColor = 0xFFFFFF;
const int kDarkModeNormalColor = 0xFFFFFF;
const int kPressedColor = 0x4285F4;
const CGFloat kPressedAlphaLight = 0.10;
const CGFloat kPressedAlphaDark = 0.21;

UIColor* NormalButtonTint(ToolbarControllerStyle style) {
  switch (style) {
    case ToolbarControllerStyleLightMode:
      if (IsUIRefreshPhase1Enabled())
        return [UIColor colorWithWhite:0 alpha:kToolbarButtonTintColorAlpha];
      else
        return UIColorFromRGB(kLightModeNormalColor);
    case ToolbarControllerStyleIncognitoMode:
      if (IsUIRefreshPhase1Enabled())
        return [UIColor whiteColor];
      else
        return UIColorFromRGB(kIncognitoModeNormalColor);
    case ToolbarControllerStyleDarkMode:
      return UIColorFromRGB(kDarkModeNormalColor);
    case ToolbarControllerStyleMaxStyles:
      NOTREACHED();
      return nil;
  }
}

UIColor* HighlighButtonTint(ToolbarControllerStyle style) {
  if (!IsUIRefreshPhase1Enabled())
    return UIColorFromRGB(kPressedColor);

  if (style == ToolbarControllerStyleLightMode) {
    return [UIColor colorWithWhite:0 alpha:kPressedAlphaLight];
  } else if (style == ToolbarControllerStyleIncognitoMode ||
             style == ToolbarControllerStyleDarkMode) {
    return [UIColor colorWithWhite:1 alpha:kPressedAlphaDark];
  }
  NOTREACHED();
  return nil;
}

}  // namespace toolbar
