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

const CGFloat kMinSearchFieldWidth = 50;

const CGFloat kSearchHintMargin = 3;

const CGFloat kMaxSearchFieldFrameMargin = 200;
const CGFloat kDoodleTopMarginIPad = 82;
const CGFloat kDoodleTopMarginIPhonePortrait = 66;
const CGFloat kDoodleTopMarginIPhoneLandscape = 56;
const CGFloat kSearchFieldTopMarginIPhonePortrait = 32;
const CGFloat kSearchFieldTopMarginIPhoneLandscape = 16;
const CGFloat kNTPSearchFieldBottomPadding = 16;

const CGFloat kTopSpacingMaterialPortrait = 56;
const CGFloat kTopSpacingMaterialLandscape = 32;

const CGFloat kVoiceSearchButtonWidth = 48;

const CGFloat kGoogleSearchDoodleHeight = 120;
// Height for the doodle frame when Google is not the default search engine.
const CGFloat kNonGoogleSearchDoodleHeight = 60;
// Height for the header view on tablet when Google is not the default search
// engine.
const CGFloat kNonGoogleSearchHeaderHeightIPad = 10;

// Returns the width necessary to fit |numberOfItem| items, with no padding on
// the side.
CGFloat widthForNumberOfItem(NSUInteger numberOfItem) {
  return (numberOfItem - 1) * content_suggestions::spacingBetweenTiles() +
         numberOfItem * [ContentSuggestionsMostVisitedCell defaultSize].width;
}
}

namespace content_suggestions {

const CGFloat kSearchFieldHeight = 50;

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

CGFloat centeredTilesMarginForWidth(CGFloat width) {
  NSUInteger columns = numberOfTilesForWidth(width - 2 * spacingBetweenTiles());
  CGFloat whitespace =
      width - columns * [ContentSuggestionsMostVisitedCell defaultSize].width -
      (columns - 1) * spacingBetweenTiles();
  CGFloat margin = AlignValueToPixel(whitespace / 2);
  DCHECK(margin >= spacingBetweenTiles());
  return margin;
}

CGFloat doodleHeight(BOOL logoIsShowing) {
  if (!IsIPadIdiom() && !logoIsShowing)
    return kNonGoogleSearchDoodleHeight;
  return kGoogleSearchDoodleHeight;
}

CGFloat doodleTopMargin() {
  if (IsIPadIdiom())
    return kDoodleTopMarginIPad;
  if (IsPortrait())
    return kDoodleTopMarginIPhonePortrait;
  return kDoodleTopMarginIPhoneLandscape;
}

CGFloat searchFieldTopMargin() {
  if (IsIPadIdiom()) {
    return kDoodleTopMarginIPad;
  } else if (IsPortrait()) {
    return kSearchFieldTopMarginIPhonePortrait;
  } else {
    return kSearchFieldTopMarginIPhoneLandscape;
  }
}

CGFloat searchFieldWidth(CGFloat superviewWidth) {
  CGFloat margin = centeredTilesMarginForWidth(superviewWidth);
  if (margin > kMaxSearchFieldFrameMargin)
    margin = kMaxSearchFieldFrameMargin;
  return fmax(superviewWidth - 2 * margin, kMinSearchFieldWidth);
}

CGFloat heightForLogoHeader(BOOL logoIsShowing, BOOL promoCanShow) {
  CGFloat headerHeight = doodleTopMargin() + doodleHeight(logoIsShowing) +
                         searchFieldTopMargin() + kSearchFieldHeight +
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
    headerHeight += UIInterfaceOrientationIsPortrait(orient)
                        ? kTopSpacingMaterialPortrait
                        : kTopSpacingMaterialLandscape;
  }

  return headerHeight;
}

void configureSearchHintLabel(UILabel* searchHintLabel,
                              UIButton* searchTapTarget) {
  [searchHintLabel setTranslatesAutoresizingMaskIntoConstraints:NO];

  [searchTapTarget addSubview:searchHintLabel];

  [NSLayoutConstraint activateConstraints:@[
    [searchHintLabel.heightAnchor
        constraintEqualToConstant:kSearchFieldHeight - 2 * kSearchHintMargin],
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
