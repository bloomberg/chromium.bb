// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <UIKit/UIKit.h>

#include "base/ios/ios_util.h"
#include "ios/chrome/today_extension/ui_util.h"

namespace {
const CGFloat kVeryLowOpacity = 0.25;
const CGFloat kLowOpacity = 0.3;
const CGFloat kMediumOpacity = 0.54;
const CGFloat kHighOpacity = 0.8;
const CGFloat kVeryHighOpacity = 0.87;
const CGFloat kFooterFontOpacity = 0.4;
const CGFloat kBackgroundOpacity = 0.75;
}

namespace ui_util {

const CGFloat kFirstLineHeight = 60;
const CGFloat kFirstLineButtonMargin = 8;
const CGFloat kSecondLineHeight = 56;
const CGFloat kSecondLineVerticalPadding = 10;
const CGFloat kUIButtonSeparator = 28;
const CGFloat kUIButtonFrontShift = 8;
const CGFloat kUIButtonCornerRadius = 4;

const CGFloat kFooterVerticalMargin = 18;
const CGFloat kFooterHorizontalMargin = 16;
const CGFloat kTitleFontSize = 16;

const CGFloat kEmptyLabelYOffset = -16;

// These five constants have been empirically determined to align paste URL
// button with chrome header icon and text.
const CGFloat kiPadTextOffset = 10;
const CGFloat kiPhoneTextOffset = 6;
const CGFloat kiPadIconOffset = -31;
const CGFloat kiPhoneIconOffset = -26;
const CGFloat kDefaultLeadingMarginInset = 48;

UIColor* InkColor() {
  return [UIColor colorWithWhite:0.5 alpha:0.4];
}

UIColor* BaseColorWithAlpha(CGFloat alpha) {
  if (base::ios::IsRunningOnIOS10OrLater()) {
    return [UIColor colorWithWhite:0.3 alpha:alpha];
  } else {
    return [UIColor colorWithWhite:1 alpha:alpha];
  }
}

UIColor* TitleColor() {
  return BaseColorWithAlpha(kVeryHighOpacity);
}

UIColor* BorderColor() {
  return BaseColorWithAlpha(kLowOpacity);
}

UIColor* TextColor() {
  return BaseColorWithAlpha(kMediumOpacity);
}

UIColor* BackgroundColor() {
  return BaseColorWithAlpha(kBackgroundOpacity);
}

UIColor* FooterTextColor() {
  return BaseColorWithAlpha(kFooterFontOpacity);
}

UIColor* NormalTintColor() {
  if (base::ios::IsRunningOnIOS10OrLater()) {
    return [UIColor colorWithWhite:0 alpha:kHighOpacity];
  } else {
    return BaseColorWithAlpha(kVeryLowOpacity);
  }
}

UIColor* ActiveTintColor() {
  if (base::ios::IsRunningOnIOS10OrLater()) {
    return [UIColor colorWithWhite:0 alpha:kHighOpacity];
  } else {
    return BaseColorWithAlpha(kVeryHighOpacity);
  }
}

UIColor* emptyLabelColor() {
  return [[UIColor blackColor] colorWithAlphaComponent:kFooterFontOpacity];
}

bool IsIPadIdiom() {
  UIUserInterfaceIdiom idiom = [[UIDevice currentDevice] userInterfaceIdiom];
  return idiom == UIUserInterfaceIdiomPad;
}

bool IsRTL() {
  NSString* locale = [NSLocale currentLocale].localeIdentifier;
  return [NSLocale characterDirectionForLanguage:locale] ==
         NSLocaleLanguageDirectionRightToLeft;
}

CGFloat ChromeIconOffset() {
  return ui_util::IsIPadIdiom() ? kiPadIconOffset : kiPhoneIconOffset;
}

CGFloat ChromeTextOffset() {
  return ui_util::IsIPadIdiom() ? kiPadTextOffset : kiPhoneTextOffset;
}

void ConstrainAllSidesOfViewToView(UIView* container, UIView* filler) {
  [NSLayoutConstraint activateConstraints:@[
    [filler.leadingAnchor constraintEqualToAnchor:container.leadingAnchor],
    [filler.trailingAnchor constraintEqualToAnchor:container.trailingAnchor],
    [filler.topAnchor constraintEqualToAnchor:container.topAnchor],
    [filler.bottomAnchor constraintEqualToAnchor:container.bottomAnchor],
  ]];
}

}  // namespace ui_util
