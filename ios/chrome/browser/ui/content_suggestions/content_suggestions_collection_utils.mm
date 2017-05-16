// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_collection_utils.h"

#include "base/i18n/rtl.h"
#include "base/logging.h"
#include "components/strings/grit/components_strings.h"
#include "ios/chrome/browser/ui/commands/ios_command_ids.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_most_visited_cell.h"
#import "ios/chrome/browser/ui/toolbar/web_toolbar_controller.h"
#include "ios/chrome/browser/ui/ui_util.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/third_party/material_components_ios/src/components/Typography/src/MaterialTypography.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
const CGFloat kSpacingIPhone = 16;
const CGFloat kSpacingIPad = 24;

const CGFloat kMaxSearchFieldFrameMargin = 200;
const CGFloat kDoodleTopMarginIPadPortrait = 82;
const CGFloat kDoodleTopMarginIPadLandscape = 82;
const CGFloat kNTPSearchFieldBottomPadding = 16;

const CGFloat kVoiceSearchButtonWidth = 48;

// Height for the doodle frame when Google is not the default search engine.
const CGFloat kNonGoogleSearchDoodleHeight = 60;
// Height for the header view on tablet when Google is not the default search
// engine.
const CGFloat kNonGoogleSearchHeaderHeightIPad = 10;

enum InterfaceOrientation {
  ALL = 0,
  IPHONE_LANDSCAPE = 1,
};

// Returns the width necessary to fit |numberOfItem| items, with no padding on
// the side.
CGFloat widthForNumberOfItem(NSUInteger numberOfItem) {
  return (numberOfItem - 1) * content_suggestions::spacingBetweenTiles() +
         numberOfItem * [ContentSuggestionsMostVisitedCell defaultSize].width;
}
}

namespace content_suggestions {

NSUInteger numberOfTilesForWidth(CGFloat availableWidth) {
  if (availableWidth > widthForNumberOfItem(4))
    return 4;
  if (availableWidth > widthForNumberOfItem(3))
    return 3;
  if (availableWidth > widthForNumberOfItem(2))
    return 2;

  return 1;
}

CGFloat spacingBetweenTiles() {
  return IsIPadIdiom() ? kSpacingIPad : kSpacingIPhone;
}

CGRect getOrientationFrame(const CGRect frames[], CGFloat width) {
  BOOL is_portrait = UIInterfaceOrientationIsPortrait(
      [[UIApplication sharedApplication] statusBarOrientation]);
  InterfaceOrientation orientation =
      (IsIPadIdiom() || is_portrait) ? ALL : IPHONE_LANDSCAPE;

  // Calculate width based on screen width and origin x.
  CGRect frame = frames[orientation];
  frame.size.width = fmax(width - 2 * frame.origin.x, 50);
  return frame;
}

CGFloat centeredTilesMarginForWidth(CGFloat width) {
  NSUInteger columns = numberOfTilesForWidth(width - 2 * spacingBetweenTiles());
  CGFloat whitespace =
      width - columns * [ContentSuggestionsMostVisitedCell defaultSize].width -
      (columns - 1) * spacingBetweenTiles();
  CGFloat margin = AlignValueToPixel(whitespace / 2);
  DCHECK(margin >= spacingBetweenTiles());
  return margin;
}

CGRect doodleFrame(CGFloat width, BOOL logoIsShowing) {
  const CGRect kDoodleFrame[2] = {
      CGRectMake(0, 66, 0, 120), CGRectMake(0, 56, 0, 120),
  };
  CGRect doodleFrame = getOrientationFrame(kDoodleFrame, width);
  if (!IsIPadIdiom() && !logoIsShowing)
    doodleFrame.size.height = kNonGoogleSearchDoodleHeight;
  if (IsIPadIdiom()) {
    doodleFrame.origin.y = IsPortrait() ? kDoodleTopMarginIPadPortrait
                                        : kDoodleTopMarginIPadLandscape;
  }
  return doodleFrame;
}

CGRect searchFieldFrame(CGFloat width, BOOL logoIsShowing) {
  CGFloat y = CGRectGetMaxY(doodleFrame(width, logoIsShowing));
  CGFloat margin = centeredTilesMarginForWidth(width);
  if (margin > kMaxSearchFieldFrameMargin)
    margin = kMaxSearchFieldFrameMargin;
  const CGRect kSearchFieldFrame[2] = {
      CGRectMake(margin, y + 32, 0, 50), CGRectMake(margin, y + 16, 0, 50),
  };
  CGRect searchFieldFrame = getOrientationFrame(kSearchFieldFrame, width);
  if (IsIPadIdiom()) {
    CGFloat iPadTopMargin = IsPortrait() ? kDoodleTopMarginIPadPortrait
                                         : kDoodleTopMarginIPadLandscape;
    searchFieldFrame.origin.y += iPadTopMargin - 32;
  }
  return searchFieldFrame;
}

CGFloat heightForLogoHeader(CGFloat width,
                            BOOL logoIsShowing,
                            BOOL promoCanShow) {
  CGFloat headerHeight = CGRectGetMaxY(searchFieldFrame(width, logoIsShowing)) +
                         kNTPSearchFieldBottomPadding;
  if (!IsIPadIdiom()) {
    return headerHeight;
  }
  if (!logoIsShowing) {
    return kNonGoogleSearchHeaderHeightIPad;
  }
  if (!promoCanShow) {
    UIInterfaceOrientation orient =
        [[UIApplication sharedApplication] statusBarOrientation];
    const CGFloat kTopSpacingMaterialPortrait = 56;
    const CGFloat kTopSpacingMaterialLandscape = 32;
    headerHeight += UIInterfaceOrientationIsPortrait(orient)
                        ? kTopSpacingMaterialPortrait
                        : kTopSpacingMaterialLandscape;
  }

  return headerHeight;
}

void configureSearchHintLabel(UILabel* searchHintLabel,
                              UIButton* searchTapTarget,
                              CGFloat searchFieldWidth) {
  CGRect hintFrame = CGRectInset(searchTapTarget.bounds, 12, 3);
  const CGFloat kVoiceSearchOffset = 48;
  hintFrame.size.width = searchFieldWidth - kVoiceSearchOffset;
  [searchHintLabel setTranslatesAutoresizingMaskIntoConstraints:NO];

  [searchTapTarget addSubview:searchHintLabel];

  [NSLayoutConstraint activateConstraints:@[
    [searchHintLabel.heightAnchor
        constraintEqualToConstant:hintFrame.size.height],
    [searchHintLabel.centerYAnchor
        constraintEqualToAnchor:searchTapTarget.centerYAnchor]
  ]];

  [searchHintLabel setText:l10n_util::GetNSString(IDS_OMNIBOX_EMPTY_HINT)];
  if (base::i18n::IsRTL()) {
    [searchHintLabel setTextAlignment:NSTextAlignmentRight];
  }
  [searchHintLabel
      setTextColor:[UIColor
                       colorWithWhite:kiPhoneOmniboxPlaceholderColorBrightness
                                alpha:1.0]];
  [searchHintLabel setFont:[MDCTypography subheadFont]];
}

void configureVoiceSearchButton(UIButton* voiceSearchButton,
                                UIButton* searchTapTarget) {
  UIImage* micImage = [UIImage imageNamed:@"voice_icon"];
  [voiceSearchButton setTranslatesAutoresizingMaskIntoConstraints:NO];
  [searchTapTarget addSubview:voiceSearchButton];

  [NSLayoutConstraint activateConstraints:@[
    [voiceSearchButton.centerYAnchor
        constraintEqualToAnchor:searchTapTarget.centerYAnchor],
    [voiceSearchButton.widthAnchor
        constraintEqualToConstant:kVoiceSearchButtonWidth],
    [voiceSearchButton.heightAnchor
        constraintEqualToAnchor:voiceSearchButton.widthAnchor],
  ]];

  [voiceSearchButton setAdjustsImageWhenHighlighted:NO];
  [voiceSearchButton setImage:micImage forState:UIControlStateNormal];
  [voiceSearchButton setTag:IDC_VOICE_SEARCH];
  [voiceSearchButton setAccessibilityLabel:l10n_util::GetNSString(
                                               IDS_IOS_ACCNAME_VOICE_SEARCH)];
  [voiceSearchButton setAccessibilityIdentifier:@"Voice Search"];
}

}  // namespace content_suggestions
