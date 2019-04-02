// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/omnibox/popup/omnibox_popup_row_cell.h"

#include "base/feature_list.h"
#include "base/logging.h"
#include "components/omnibox/common/omnibox_features.h"
#import "ios/chrome/browser/ui/omnibox/popup/autocomplete_suggestion.h"
#import "ios/chrome/browser/ui/omnibox/popup/omnibox_popup_truncating_label.h"
#import "ios/chrome/browser/ui/toolbar/public/toolbar_constants.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ios/chrome/grit/ios_theme_resources.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
const CGFloat kLeadingMargin = 12;
const CGFloat kLeadingMarginIpad = 183;
const CGFloat kTextTopMargin = 6;
const CGFloat kTextLeadingMargin = 10;
const CGFloat kTextTrailingMargin = -12;
const CGFloat kImageViewSize = 28;
const CGFloat kImageViewCornerRadius = 7;
const CGFloat kTrailingButtonSize = 48;
const CGFloat kTrailingButtonTrailingMargin = 4;

NSString* const kOmniboxPopupRowSwitchTabAccessibilityIdentifier =
    @"OmniboxPopupRowSwitchTabAccessibilityIdentifier";
}  // namespace

@interface OmniboxPopupRowCell ()

// The suggestion that this cell is currently displaying.
@property(nonatomic, strong) id<AutocompleteSuggestion> suggestion;
// Whether the cell is currently dispalying in incognito mode or not.
@property(nonatomic, assign) BOOL incognito;

// Stack view containing all text labels.
@property(nonatomic, strong) UIStackView* textStackView;
// Truncating label for the main text.
@property(nonatomic, strong) OmniboxPopupTruncatingLabel* textTruncatingLabel;
// Truncating label for the detail text.
@property(nonatomic, strong) OmniboxPopupTruncatingLabel* detailTruncatingLabel;
// Regular UILabel for the detail text when the suggestion is an answer.
// Answers have slightly different display requirements, like possibility of
// multiple lines and truncating with ellipses instead of a fade gradient.
@property(nonatomic, strong) UILabel* detailAnswerLabel;
// Image view for the leading image (only appears on iPad).
@property(nonatomic, strong) UIImageView* leadingImageView;
// Trailing button for appending suggestion into omnibox.
@property(nonatomic, strong) UIButton* trailingButton;

@end

@implementation OmniboxPopupRowCell

- (instancetype)initWithStyle:(UITableViewCellStyle)style
              reuseIdentifier:(NSString*)reuseIdentifier {
  self = [super initWithStyle:style reuseIdentifier:reuseIdentifier];
  if (self) {
    _textTruncatingLabel =
        [[OmniboxPopupTruncatingLabel alloc] initWithFrame:CGRectZero];
    _textTruncatingLabel.translatesAutoresizingMaskIntoConstraints = NO;

    _textStackView = [[UIStackView alloc]
        initWithArrangedSubviews:@[ _textTruncatingLabel ]];
    _textStackView.translatesAutoresizingMaskIntoConstraints = NO;
    _textStackView.axis = UILayoutConstraintAxisVertical;

    _detailTruncatingLabel =
        [[OmniboxPopupTruncatingLabel alloc] initWithFrame:CGRectZero];
    _detailTruncatingLabel.translatesAutoresizingMaskIntoConstraints = NO;

    // Answers use a UILabel with NSLineBreakByTruncatingTail to produce a
    // truncation with an ellipse instead of fading on multi-line text.
    _detailAnswerLabel = [[UILabel alloc] initWithFrame:CGRectZero];
    _detailAnswerLabel.translatesAutoresizingMaskIntoConstraints = NO;
    _detailAnswerLabel.lineBreakMode = NSLineBreakByTruncatingTail;

    _leadingImageView = [[UIImageView alloc] initWithImage:nil];
    _leadingImageView.translatesAutoresizingMaskIntoConstraints = NO;
    _leadingImageView.contentMode = UIViewContentModeCenter;
    _leadingImageView.layer.cornerRadius = kImageViewCornerRadius;

    _trailingButton = [UIButton buttonWithType:UIButtonTypeCustom];
    _trailingButton.translatesAutoresizingMaskIntoConstraints = NO;
    [_trailingButton addTarget:self
                        action:@selector(trailingButtonTapped)
              forControlEvents:UIControlEventTouchUpInside];
    [_trailingButton setContentMode:UIViewContentModeRight];
    // The trailing button also shows answer images. In that case, the
    // button will be disabled, but the image shouldn't be dimmed.
    _trailingButton.adjustsImageWhenDisabled = NO;

    _incognito = NO;

    self.backgroundColor = [UIColor clearColor];
  }
  return self;
}

#pragma mark - Layout

// Setup the layout of the cell initially. This only adds the elements that are
// always in the cell.
- (void)setupLayout {
  [self.contentView addSubview:self.leadingImageView];
  [self.contentView addSubview:self.textStackView];

  [NSLayoutConstraint activateConstraints:@[
    // Row has a minimum height.
    [self.contentView.heightAnchor constraintGreaterThanOrEqualToConstant:48],

    // Position leadingImageView at the leading edge of the view
    [self.leadingImageView.heightAnchor
        constraintEqualToConstant:kImageViewSize],
    [self.leadingImageView.widthAnchor
        constraintEqualToConstant:kImageViewSize],
    [self.leadingImageView.leadingAnchor
        constraintEqualToAnchor:self.contentView.leadingAnchor
                       constant:IsRegularXRegularSizeClass()
                                    ? kLeadingMarginIpad
                                    : kLeadingMargin],
    [self.leadingImageView.centerYAnchor
        constraintEqualToAnchor:self.contentView.centerYAnchor],

    // Position textStackView after leadingImageView.
    [self.textStackView.leadingAnchor
        constraintEqualToAnchor:self.leadingImageView.trailingAnchor
                       constant:kTextLeadingMargin],
    // Use greater than or equal constraints because there may be a trailing
    // button here.
    [self.contentView.trailingAnchor
        constraintGreaterThanOrEqualToAnchor:self.textStackView.trailingAnchor],
    // Top space should be at least the given top margin, but can be more if
    // the row is short enough to use the minimum height constraint above.
    [self.textStackView.topAnchor
        constraintGreaterThanOrEqualToAnchor:self.contentView.topAnchor
                                    constant:kTextTopMargin],
    [self.textStackView.centerYAnchor
        constraintEqualToAnchor:self.contentView.centerYAnchor],
  ]];
}

// Add the trailing button as a subview and setup its constraints.
- (void)setupTrailingButtonLayout {
  [self.contentView addSubview:self.trailingButton];
  [NSLayoutConstraint activateConstraints:@[
    [self.trailingButton.heightAnchor
        constraintEqualToConstant:kTrailingButtonSize],
    [self.trailingButton.widthAnchor
        constraintEqualToConstant:kTrailingButtonSize],
    [self.trailingButton.centerYAnchor
        constraintEqualToAnchor:self.contentView.centerYAnchor],
    [self.contentView.trailingAnchor
        constraintEqualToAnchor:self.trailingButton.trailingAnchor
                       constant:kTrailingButtonTrailingMargin],
    [self.trailingButton.leadingAnchor
        constraintEqualToAnchor:self.textStackView.trailingAnchor
                       constant:kTextTrailingMargin],
  ]];
}

- (void)prepareForReuse {
  [super prepareForReuse];

  self.suggestion = nil;
  self.incognito = NO;

  // Clear text.
  self.textTruncatingLabel.attributedText = nil;
  self.detailTruncatingLabel.attributedText = nil;
  self.detailAnswerLabel.attributedText = nil;

  // Remove optional views.
  [self.trailingButton setImage:nil forState:UIControlStateNormal];
  [self.trailingButton removeFromSuperview];
  [self.detailTruncatingLabel removeFromSuperview];
  [self.detailAnswerLabel removeFromSuperview];

  self.trailingButton.accessibilityIdentifier = nil;

  self.accessibilityCustomActions = nil;
}

#pragma mark - Cell setup with data

// Use the given autocomplete suggestion and whether incognito is enabled to
// layout the cell correctly for that data.
- (void)setupWithAutocompleteSuggestion:(id<AutocompleteSuggestion>)suggestion
                              incognito:(BOOL)incognito {
  // Setup the view layout the first time the cell is setup.
  if (self.contentView.subviews.count == 0) {
    [self setupLayout];
  }
  self.suggestion = suggestion;
  self.incognito = incognito;

  self.textTruncatingLabel.attributedText = self.suggestion.text;

  // URLs have have special layout requirements.
  self.detailTruncatingLabel.displayAsURL = suggestion.isURL;
  UILabel* detailLabel = self.suggestion.hasAnswer ? self.detailAnswerLabel
                                                   : self.detailTruncatingLabel;
  if ([self.suggestion.detailText length] > 0) {
    [self.textStackView addArrangedSubview:detailLabel];
    detailLabel.attributedText = self.suggestion.detailText;
    if (self.suggestion.hasAnswer) {
      detailLabel.numberOfLines = self.suggestion.numberOfLines;
    }
  }

  if (self.suggestion.hasImage || self.suggestion.isAppendable ||
      self.suggestion.isTabMatch) {
    [self setupTrailingButton];
  }

  self.leadingImageView.image = self.suggestion.suggestionTypeIcon;
  self.leadingImageView.backgroundColor =
      self.incognito ? [UIColor colorWithWhite:1 alpha:0.05]
                     : [UIColor colorWithWhite:0 alpha:0.03];
  self.leadingImageView.tintColor = self.incognito
                                        ? [UIColor colorWithWhite:1 alpha:0.4]
                                        : [UIColor colorWithWhite:0 alpha:0.33];
}

- (void)setupTrailingButton {
  [self setupTrailingButtonLayout];

  // If there's an image, put it in the button, but disable interaction.
  if (self.suggestion.hasImage) {
    self.trailingButton.enabled = NO;
    [self.trailingButton setImage:nil forState:UIControlStateNormal];
    return;
  }

  // Show append button for search history/search suggestions or
  // switch-to-open-tab as the right control element (aka an accessory element
  // of a table view cell).
  NSString* trailingButtonActionName =
      self.suggestion.isTabMatch
          ? l10n_util::GetNSString(IDS_IOS_OMNIBOX_POPUP_SWITCH_TO_OPEN_TAB)
          : l10n_util::GetNSString(IDS_IOS_OMNIBOX_POPUP_APPEND);
  UIAccessibilityCustomAction* trailingButtonAction =
      [[UIAccessibilityCustomAction alloc]
          initWithName:trailingButtonActionName
                target:self
              selector:@selector(trailingButtonTapped)];

  self.accessibilityCustomActions = @[ trailingButtonAction ];

  UIImage* trailingButtonImage = nil;
  if (self.suggestion.isTabMatch) {
    trailingButtonImage = [UIImage imageNamed:@"omnibox_popup_tab_match"];
    trailingButtonImage =
        trailingButtonImage.imageFlippedForRightToLeftLayoutDirection;
    self.trailingButton.accessibilityIdentifier =
        kOmniboxPopupRowSwitchTabAccessibilityIdentifier;
  } else {
    int trailingButtonResourceID = 0;
    if (base::FeatureList::IsEnabled(omnibox::kOmniboxTabSwitchSuggestions)) {
      trailingButtonResourceID = IDR_IOS_OMNIBOX_KEYBOARD_VIEW_APPEND;
    } else {
      trailingButtonResourceID =
          self.incognito ? IDR_IOS_OMNIBOX_KEYBOARD_VIEW_APPEND_INCOGNITO
                         : IDR_IOS_OMNIBOX_KEYBOARD_VIEW_APPEND;
    }
    trailingButtonImage = NativeReversableImage(trailingButtonResourceID, YES);
  }
  trailingButtonImage = [trailingButtonImage
      imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];

  self.trailingButton.enabled = YES;
  [self.trailingButton setImage:trailingButtonImage
                       forState:UIControlStateNormal];
  if (base::FeatureList::IsEnabled(omnibox::kOmniboxTabSwitchSuggestions)) {
    self.trailingButton.tintColor = self.incognito
                                        ? [UIColor whiteColor]
                                        : UIColorFromRGB(kLocationBarTintBlue);
  } else {
    self.trailingButton.tintColor = self.incognito
                                        ? [UIColor colorWithWhite:1 alpha:0.5]
                                        : [UIColor colorWithWhite:0 alpha:0.3];
  }
}

- (NSString*)accessibilityLabel {
  return self.textTruncatingLabel.attributedText.string;
}

- (NSString*)accessibilityValue {
  return self.detailTruncatingLabel.hidden
             ? self.detailAnswerLabel.attributedText.string
             : self.detailTruncatingLabel.attributedText.string;
}

- (void)trailingButtonTapped {
  [self.delegate trailingButtonTappedForCell:self];
}

@end
