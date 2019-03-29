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
const CGFloat kLeadingMargin = 24;
const CGFloat kLeadingMarginIpad = 183;
const CGFloat kTextTopMargin = 6;
const CGFloat kTextLeadingMargin = 10;
const CGFloat kTextTrailingMargin = -12;
const CGFloat kImageViewSize = 28;
const CGFloat kImageViewCornerRadius = 7;
const CGFloat kTrailingButtonSize = 48;
const CGFloat kTrailingButtonTrailingMargin = 4;
const CGFloat kAnswerImageSize = 30;
const CGFloat kAnswerImageLeadingMargin = -1;
const CGFloat kAnswerImageViewTopMargin = 2;

NSString* const kOmniboxPopupRowSwitchTabAccessibilityIdentifier =
    @"OmniboxPopupRowSwitchTabAccessibilityIdentifier";
}  // namespace

@interface OmniboxPopupRowCell ()

// The suggestion that this cell is currently displaying.
@property(nonatomic, strong) id<AutocompleteSuggestion> suggestion;
// Whether the cell is currently dispalying in incognito mode or not.
@property(nonatomic, assign) BOOL incognito;

// Truncating label for the main text.
@property(nonatomic, strong) OmniboxPopupTruncatingLabel* textTruncatingLabel;
// Truncating label for the detail text.
@property(nonatomic, strong) OmniboxPopupTruncatingLabel* detailTruncatingLabel;
// Regular UILabel for the detail text when the suggestion is an answer.
// Answers have slightly different display requirements, like possibility of
// multiple lines and truncating with ellipses instead of a fade gradient.
@property(nonatomic, strong) UILabel* detailAnswerLabel;
// Image view for the image in an answer (e.g. weather image).
@property(nonatomic, strong) UIImageView* detailAnswerImageView;
// Image view for the leading image (only appears on iPad).
@property(nonatomic, strong) UIImageView* leadingImageView;
// Trailing button for appending suggestion into omnibox.
@property(nonatomic, strong) UIButton* trailingButton;
// Layout guide encapsulating everything related to the detail section.
@property(nonatomic, strong) UILayoutGuide* detailLayoutGuide;

@end

@implementation OmniboxPopupRowCell

- (instancetype)initWithStyle:(UITableViewCellStyle)style
              reuseIdentifier:(NSString*)reuseIdentifier {
  self = [super initWithStyle:style reuseIdentifier:reuseIdentifier];
  if (self) {
    _textTruncatingLabel =
        [[OmniboxPopupTruncatingLabel alloc] initWithFrame:CGRectZero];
    _textTruncatingLabel.translatesAutoresizingMaskIntoConstraints = NO;
    _textTruncatingLabel.userInteractionEnabled = NO;

    _detailTruncatingLabel =
        [[OmniboxPopupTruncatingLabel alloc] initWithFrame:CGRectZero];
    _detailTruncatingLabel.translatesAutoresizingMaskIntoConstraints = NO;
    _detailTruncatingLabel.userInteractionEnabled = NO;

    // Answers use a UILabel with NSLineBreakByTruncatingTail to produce a
    // truncation with an ellipse instead of fading on multi-line text.
    _detailAnswerLabel = [[UILabel alloc] initWithFrame:CGRectZero];
    _detailAnswerLabel.translatesAutoresizingMaskIntoConstraints = NO;
    _detailAnswerLabel.userInteractionEnabled = NO;
    _detailAnswerLabel.lineBreakMode = NSLineBreakByTruncatingTail;

    _detailAnswerImageView = [[UIImageView alloc] initWithImage:nil];
    _detailAnswerImageView.translatesAutoresizingMaskIntoConstraints = NO;
    _detailAnswerImageView.userInteractionEnabled = NO;
    _detailAnswerImageView.contentMode = UIViewContentModeScaleAspectFit;

    _leadingImageView = [[UIImageView alloc] initWithImage:nil];
    _leadingImageView.translatesAutoresizingMaskIntoConstraints = NO;
    _leadingImageView.userInteractionEnabled = NO;
    _leadingImageView.contentMode = UIViewContentModeCenter;
    _leadingImageView.layer.cornerRadius = kImageViewCornerRadius;

    _trailingButton = [UIButton buttonWithType:UIButtonTypeCustom];
    _trailingButton.translatesAutoresizingMaskIntoConstraints = NO;
    [_trailingButton addTarget:self
                        action:@selector(trailingButtonTapped)
              forControlEvents:UIControlEventTouchUpInside];
    [_trailingButton setContentMode:UIViewContentModeRight];

    _detailLayoutGuide = [[UILayoutGuide alloc] init];
    _incognito = NO;

    self.backgroundColor = [UIColor clearColor];
  }
  return self;
}

#pragma mark - Layout

// Setup the layout of the cell initially. This only adds the elements that are
// always in the cell.
- (void)setupLayout {
  [self.contentView addSubview:self.textTruncatingLabel];
  [self.contentView addSubview:self.detailTruncatingLabel];
  [self.contentView addSubview:self.detailAnswerLabel];
  [self.contentView addLayoutGuide:self.detailLayoutGuide];

  [NSLayoutConstraint activateConstraints:@[
    // Use greater/less than or equal constraints to allow for when any optional
    // elements are added (leadingImageView, trailingButton, etc.)

    // Position textTruncatingLabel
    [self.textTruncatingLabel.leadingAnchor
        constraintGreaterThanOrEqualToAnchor:self.contentView.leadingAnchor
                                    constant:kLeadingMargin],
    [self.contentView.trailingAnchor
        constraintGreaterThanOrEqualToAnchor:self.textTruncatingLabel
                                                 .trailingAnchor],
    [self.textTruncatingLabel.topAnchor
        constraintEqualToAnchor:self.contentView.topAnchor
                       constant:kTextTopMargin],

    // Position detailLayoutGuide
    [self.detailLayoutGuide.leadingAnchor
        constraintGreaterThanOrEqualToAnchor:self.contentView.leadingAnchor
                                    constant:kLeadingMargin],
    [self.contentView.trailingAnchor
        constraintGreaterThanOrEqualToAnchor:self.detailLayoutGuide
                                                 .trailingAnchor],
    [self.detailLayoutGuide.topAnchor
        constraintEqualToAnchor:self.textTruncatingLabel.bottomAnchor],
    [self.detailLayoutGuide.bottomAnchor
        constraintEqualToAnchor:self.contentView.bottomAnchor],

    // Position detailTruncatingLabel in detailLayoutGuide.
    [self.detailTruncatingLabel.leadingAnchor
        constraintGreaterThanOrEqualToAnchor:self.detailLayoutGuide
                                                 .leadingAnchor],
    [self.detailTruncatingLabel.trailingAnchor
        constraintEqualToAnchor:self.detailLayoutGuide.trailingAnchor],
    [self.detailTruncatingLabel.topAnchor
        constraintEqualToAnchor:self.detailLayoutGuide.topAnchor],
    [self.detailTruncatingLabel.bottomAnchor
        constraintEqualToAnchor:self.detailLayoutGuide.bottomAnchor],

    // Position detailAnswerLabel exactly equal to detailTruncatingLabel.
    [self.detailAnswerLabel.topAnchor
        constraintEqualToAnchor:self.detailTruncatingLabel.topAnchor],
    [self.detailAnswerLabel.bottomAnchor
        constraintEqualToAnchor:self.detailTruncatingLabel.bottomAnchor],
    [self.detailAnswerLabel.leadingAnchor
        constraintEqualToAnchor:self.detailTruncatingLabel.leadingAnchor],
    [self.detailAnswerLabel.trailingAnchor
        constraintEqualToAnchor:self.detailTruncatingLabel.trailingAnchor],
  ]];

  // To prevent ambiguity when there is no leading image view, add a
  // non-required leading space 0 constraint to force the "Greater than or
  // equal" to become as small as possible.
  NSLayoutConstraint* textConstraint = [self.textTruncatingLabel.leadingAnchor
      constraintEqualToAnchor:self.contentView.leadingAnchor];
  textConstraint.priority = UILayoutPriorityDefaultHigh;

  NSLayoutConstraint* detailConstraint = [self.detailLayoutGuide.leadingAnchor
      constraintEqualToAnchor:self.contentView.leadingAnchor];
  detailConstraint.priority = UILayoutPriorityDefaultHigh;

  NSLayoutConstraint* detailTextConstraint =
      [self.detailTruncatingLabel.leadingAnchor
          constraintEqualToAnchor:self.self.detailLayoutGuide.leadingAnchor];
  detailTextConstraint.priority = UILayoutPriorityDefaultHigh;
  [NSLayoutConstraint activateConstraints:@[
    textConstraint, detailConstraint, detailTextConstraint
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
        constraintEqualToAnchor:self.textTruncatingLabel.trailingAnchor
                       constant:kTextTrailingMargin],
    [self.trailingButton.leadingAnchor
        constraintEqualToAnchor:self.detailLayoutGuide.trailingAnchor
                       constant:kTextTrailingMargin],
  ]];
}

// Add the leading image view as a subview and setup its constraints.
- (void)setupLeadingImageViewLayout {
  [self.contentView addSubview:self.leadingImageView];
  [NSLayoutConstraint activateConstraints:@[
    [self.leadingImageView.heightAnchor
        constraintEqualToConstant:kImageViewSize],
    [self.leadingImageView.widthAnchor
        constraintEqualToConstant:kImageViewSize],
    [self.leadingImageView.centerYAnchor
        constraintEqualToAnchor:self.contentView.centerYAnchor],
    [self.leadingImageView.leadingAnchor
        constraintEqualToAnchor:self.contentView.leadingAnchor
                       constant:kLeadingMarginIpad],
    [self.textTruncatingLabel.leadingAnchor
        constraintEqualToAnchor:self.leadingImageView.trailingAnchor
                       constant:kTextLeadingMargin],
    [self.detailLayoutGuide.leadingAnchor
        constraintEqualToAnchor:self.leadingImageView.trailingAnchor
                       constant:kTextLeadingMargin],
  ]];
}

// Add the detail answer image view as a subview and setup its constraints.
- (void)setupDetailAnswerImageViewLayout {
  [self.contentView addSubview:self.detailAnswerImageView];
  [NSLayoutConstraint activateConstraints:@[
    [self.detailAnswerImageView.heightAnchor
        constraintEqualToConstant:kAnswerImageSize],
    [self.detailAnswerImageView.widthAnchor
        constraintEqualToConstant:kAnswerImageSize],
    [self.detailAnswerImageView.leadingAnchor
        constraintEqualToAnchor:self.detailLayoutGuide.leadingAnchor
                       constant:kAnswerImageLeadingMargin],
    [self.detailAnswerImageView.topAnchor
        constraintEqualToAnchor:self.detailLayoutGuide.topAnchor
                       constant:kAnswerImageViewTopMargin],
    [self.detailAnswerImageView.trailingAnchor
        constraintEqualToAnchor:self.detailTruncatingLabel.leadingAnchor],
  ]];
}

- (void)prepareForReuse {
  [super prepareForReuse];

  self.suggestion = nil;
  self.incognito = NO;

  // Remove optional views
  [self.trailingButton removeFromSuperview];
  [self.leadingImageView removeFromSuperview];
  [self.detailAnswerImageView removeFromSuperview];

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

  self.detailAnswerLabel.hidden = !self.suggestion.hasAnswer;
  self.detailTruncatingLabel.hidden = self.suggestion.hasAnswer;
  // URLs have have special layout requirements.
  self.detailTruncatingLabel.displayAsURL = suggestion.isURL;
  UILabel* detailLabel = self.suggestion.hasAnswer ? self.detailAnswerLabel
                                                   : self.detailTruncatingLabel;
  detailLabel.attributedText = self.suggestion.detailText;
  if (self.suggestion.hasAnswer) {
    detailLabel.numberOfLines = self.suggestion.numberOfLines;
  }

  // Show append button for search history/search suggestions as the right
  // control element (aka an accessory element of a table view cell).
  if (self.suggestion.isAppendable || self.suggestion.isTabMatch) {
    [self setupTrailingButtonLayout];

    NSString* trailingButtonActionName =
        suggestion.isTabMatch
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
      trailingButtonImage =
          NativeReversableImage(trailingButtonResourceID, YES);
    }
    trailingButtonImage = [trailingButtonImage
        imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];

    [self.trailingButton setImage:trailingButtonImage
                         forState:UIControlStateNormal];
    if (base::FeatureList::IsEnabled(omnibox::kOmniboxTabSwitchSuggestions)) {
      self.trailingButton.tintColor =
          self.incognito ? [UIColor whiteColor]
                         : UIColorFromRGB(kLocationBarTintBlue);
    } else {
      self.trailingButton.tintColor =
          self.incognito ? [UIColor colorWithWhite:1 alpha:0.5]
                         : [UIColor colorWithWhite:0 alpha:0.3];
    }
  }

  if ([self showsLeadingIcons]) {
    self.leadingImageView.image = self.suggestion.suggestionTypeIcon;
    self.leadingImageView.backgroundColor =
        self.incognito ? [UIColor colorWithWhite:1 alpha:0.05]
                       : [UIColor colorWithWhite:0 alpha:0.03];
    self.leadingImageView.tintColor =
        self.incognito ? [UIColor colorWithWhite:1 alpha:0.4]
                       : [UIColor colorWithWhite:0 alpha:0.33];
    [self setupLeadingImageViewLayout];
  }

  if (suggestion.hasImage) {
    [self setupDetailAnswerImageViewLayout];
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

- (BOOL)showsLeadingIcons {
  return IsRegularXRegularSizeClass();
}

- (void)trailingButtonTapped {
  [self.delegate trailingButtonTappedForCell:self];
}

@end
