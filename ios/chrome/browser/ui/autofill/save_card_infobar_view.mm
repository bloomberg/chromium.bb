// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/autofill/save_card_infobar_view.h"

#include "base/logging.h"
#include "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/procedural_block_types.h"
#import "ios/chrome/browser/ui/autofill/save_card_infobar_view_delegate.h"
#import "ios/chrome/browser/ui/colors/MDCPalette+CrAdditions.h"
#import "ios/chrome/browser/ui/infobars/infobar_view_sizing_delegate.h"
#include "ios/chrome/browser/ui/ui_util.h"
#import "ios/chrome/browser/ui/uikit_ui_util.h"
#import "ios/chrome/browser/ui/util/constraints_ui_util.h"
#import "ios/chrome/browser/ui/util/label_link_controller.h"
#import "ios/third_party/material_components_ios/src/components/Buttons/src/MaterialButtons.h"
#import "ios/third_party/material_components_ios/src/components/Typography/src/MaterialTypography.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Padding used on the top and bottom edges of the infobar. Every view with the
// exception of the close button is inside this padding.
const CGFloat kVerticalPadding = 10;

// Padding used on the leading and trailing edges of the infobar.
const CGFloat kHorizontalPadding = 10;

// Padding used on the top and leading edges of the icon. This is to align its
// top edge with the top edge of the message and its leading edge with the
// leading edge of the content.
const CGFloat kIconPadding = 3;

// Padding used on the top edge of the GPay icon. This is to align its top with
// the top edge of the message. Similar to the infobar content,
// kHorizontalPadding is used on the leading edge of the GPay icon.
const CGFloat kGPayIconVerticalPadding = 12;

// Padding used in the close button. This is to make up for the padding in the
// close button icon itself in order to align the icon with kVerticalPadding and
// kHorizontalPadding.
const CGFloat kCloseButtonInnerPadding = 8;

// Vertical spacing between the views.
const CGFloat kVerticalSpacing = 10;

// Horizontal spacing between the views.
const CGFloat kHorizontalSpacing = 10;

// Color in RGB to be used as tint color on the icon.
const CGFloat kIconTintColor = 0x1A73E8;

// Color in RGB used as background of the action buttons.
const int kButtonTitleColorBlue = 0x4285f4;

// Corner radius for action buttons.
const CGFloat kButtonCornerRadius = 11.0;

// Returns the font for the infobar message.
UIFont* InfoBarMessageFont() {
  return IsRefreshInfobarEnabled()
             ? [UIFont preferredFontForTextStyle:UIFontTextStyleBody]
             : [MDCTypography subheadFont];
}

}  // namespace

@implementation MessageWithLinks

@synthesize messageText = _messageText;
@synthesize linkRanges = _linkRanges;
@synthesize linkURLs = _linkURLs;

@end

@interface SaveCardInfoBarView ()

// Allows styled and clickable links in the message label. May be nil.
@property(nonatomic, strong) LabelLinkController* messageLinkController;

// Allows styled and clickable links in the legal message label. May be nil.
@property(nonatomic, strong) LabelLinkController* legalMessageLinkController;

@end

@implementation SaveCardInfoBarView

@synthesize visibleHeight = _visibleHeight;
@synthesize sizingDelegate = _sizingDelegate;
@synthesize delegate = _delegate;
@synthesize googlePayBrandingEnabled = _googlePayBrandingEnabled;
@synthesize icon = _icon;
@synthesize message = _message;
@synthesize closeButtonImage = _closeButtonImage;
@synthesize description = _description;
@synthesize cardIssuerIcon = _cardIssuerIcon;
@synthesize cardLabel = _cardLabel;
@synthesize cardSublabel = _cardSublabel;
@synthesize legalMessages = _legalMessages;
@synthesize cancelButtonTitle = _cancelButtonTitle;
@synthesize confirmButtonTitle = _confirmButtonTitle;
@synthesize messageLinkController = _messageLinkController;
@synthesize legalMessageLinkController = _legalMessageLinkController;

#pragma mark - UIView

- (void)willMoveToSuperview:(UIView*)newSuperview {
  // Create and add subviews the first time this moves to a superview.
  if (newSuperview && self.subviews.count == 0) {
    [self setupSubviews];
  }
}

- (void)layoutSubviews {
  [super layoutSubviews];

  [self.sizingDelegate didSetInfoBarTargetHeight:CGRectGetHeight(self.frame)];
}

- (void)setFrame:(CGRect)frame {
  [super setFrame:frame];

  [self layoutIfNeeded];
}

- (CGSize)sizeThatFits:(CGSize)size {
  CGSize computedSize = [self systemLayoutSizeFittingSize:size];
  return CGSizeMake(size.width, computedSize.height);
}

#pragma mark - Helper methods

// Creates and adds subviews.
- (void)setupSubviews {
  [self setAccessibilityViewIsModal:YES];
  [self setBackgroundColor:[UIColor whiteColor]];

  // The drop shadow is at the top of the view, placed outside of its bounds.
  if (!IsRefreshInfobarEnabled()) {
    UIImage* shadowImage = [UIImage imageNamed:@"infobar_shadow"];
    UIImageView* shadowView = [[UIImageView alloc] initWithImage:shadowImage];
    shadowView.translatesAutoresizingMaskIntoConstraints = NO;

    [self addSubview:shadowView];
    [NSLayoutConstraint activateConstraints:@[
      [self.leadingAnchor constraintEqualToAnchor:shadowView.leadingAnchor],
      [self.trailingAnchor constraintEqualToAnchor:shadowView.trailingAnchor],
      [self.topAnchor constraintEqualToAnchor:shadowView.topAnchor
                                     constant:shadowView.image.size.height],
    ]];
  }

  // The topmost view containing the icon, the message, and the close button,
  // arranged from the leading edge to the trailing edge respectively. The icon
  // and the close button are fixed to the edges while the message expands to
  // fill the remaining space.
  UIStackView* headerView = [[UIStackView alloc] initWithArrangedSubviews:@[]];
  headerView.translatesAutoresizingMaskIntoConstraints = NO;
  headerView.clipsToBounds = YES;
  headerView.axis = UILayoutConstraintAxisHorizontal;
  headerView.alignment = UIStackViewAlignmentTop;

  [self addSubview:headerView];
  id<LayoutGuideProvider> layoutGuide = SafeAreaLayoutGuideForView(self);
  [NSLayoutConstraint activateConstraints:@[
    [layoutGuide.leadingAnchor
        constraintEqualToAnchor:headerView.leadingAnchor],
    [layoutGuide.trailingAnchor
        constraintEqualToAnchor:headerView.trailingAnchor],
    [layoutGuide.topAnchor constraintEqualToAnchor:headerView.topAnchor],
  ]];

  // The middle view containing all of the content.
  UIStackView* contentView = [[UIStackView alloc] initWithArrangedSubviews:@[]];
  contentView.translatesAutoresizingMaskIntoConstraints = NO;
  contentView.clipsToBounds = YES;
  contentView.axis = UILayoutConstraintAxisVertical;
  contentView.spacing = kVerticalSpacing;
  contentView.layoutMarginsRelativeArrangement = YES;
  contentView.layoutMargins =
      UIEdgeInsetsMake(kVerticalPadding, kHorizontalPadding, kVerticalPadding,
                       kHorizontalPadding);

  [self addSubview:contentView];
  [NSLayoutConstraint activateConstraints:@[
    [layoutGuide.leadingAnchor
        constraintEqualToAnchor:contentView.leadingAnchor],
    [layoutGuide.trailingAnchor
        constraintEqualToAnchor:contentView.trailingAnchor],
    [headerView.bottomAnchor constraintEqualToAnchor:contentView.topAnchor],
  ]];

  // The bottommost trailing edge aligned view containing the action buttons.
  UIStackView* footerView = [[UIStackView alloc] initWithArrangedSubviews:@[]];
  footerView.translatesAutoresizingMaskIntoConstraints = NO;
  footerView.clipsToBounds = YES;
  footerView.axis = UILayoutConstraintAxisHorizontal;
  footerView.spacing = kHorizontalSpacing;
  footerView.layoutMarginsRelativeArrangement = YES;
  footerView.layoutMargins = UIEdgeInsetsMake(
      0, kHorizontalPadding, kVerticalPadding, kHorizontalPadding);

  [self addSubview:footerView];
  [NSLayoutConstraint activateConstraints:@[
    [layoutGuide.leadingAnchor
        constraintLessThanOrEqualToAnchor:footerView.leadingAnchor],
    [layoutGuide.trailingAnchor
        constraintEqualToAnchor:footerView.trailingAnchor],
    [contentView.bottomAnchor constraintEqualToAnchor:footerView.topAnchor],
    [layoutGuide.bottomAnchor constraintEqualToAnchor:footerView.bottomAnchor],
  ]];

  if (self.icon) {
    UIImageView* iconImageView = [[UIImageView alloc] initWithImage:self.icon];
    if (IsRefreshInfobarEnabled() && !self.googlePayBrandingEnabled) {
      iconImageView.tintColor = UIColorFromRGB(kIconTintColor);
    }
    // Prevent the icon from shrinking or expanding horizontally.
    [iconImageView
        setContentCompressionResistancePriority:UILayoutPriorityRequired
                                        forAxis:
                                            UILayoutConstraintAxisHorizontal];
    [iconImageView setContentHuggingPriority:UILayoutPriorityRequired
                                     forAxis:UILayoutConstraintAxisHorizontal];

    UIStackView* iconContainerView =
        [[UIStackView alloc] initWithFrame:CGRectZero];
    iconContainerView.translatesAutoresizingMaskIntoConstraints = NO;
    iconContainerView.clipsToBounds = YES;
    iconContainerView.layoutMarginsRelativeArrangement = YES;
    iconContainerView.layoutMargins = iconContainerView.layoutMargins =
        UIEdgeInsetsMake(
            self.googlePayBrandingEnabled ? kGPayIconVerticalPadding
                                          : kIconPadding,
            self.googlePayBrandingEnabled ? kHorizontalPadding : kIconPadding,
            0, 0);
    [iconContainerView addArrangedSubview:iconImageView];
    [headerView addArrangedSubview:iconContainerView];
  }

  if (self.message) {
    DCHECK_GT(self.message.messageText.length, 0UL);
    DCHECK_EQ(self.message.linkURLs.size(), self.message.linkRanges.count);

    UILabel* label = [[UILabel alloc] initWithFrame:CGRectZero];
    label.translatesAutoresizingMaskIntoConstraints = NO;
    label.clipsToBounds = YES;
    label.text = self.message.messageText;
    label.font = InfoBarMessageFont();
    label.textColor = [[MDCPalette greyPalette] tint900];
    label.numberOfLines = 0;
    label.lineBreakMode = NSLineBreakByWordWrapping;
    label.backgroundColor = [UIColor clearColor];

    if (self.message.linkRanges.count > 0UL) {
      __weak SaveCardInfoBarView* weakSelf = self;
      self.messageLinkController = [self
          labelLinkControllerWithLabel:label
                                action:^(const GURL& URL) {
                                  [weakSelf.delegate
                                      saveCardInfoBarViewDidTapLink:weakSelf];
                                }];

      auto block = ^(NSValue* rangeValue, NSUInteger idx, BOOL* stop) {
        [self.messageLinkController
            addLinkWithRange:rangeValue.rangeValue
                         url:self.message.linkURLs[idx]];
      };
      [self.message.linkRanges enumerateObjectsUsingBlock:block];
    }

    UIStackView* messageContainerView =
        [[UIStackView alloc] initWithArrangedSubviews:@[]];
    messageContainerView.translatesAutoresizingMaskIntoConstraints = NO;
    messageContainerView.clipsToBounds = YES;
    messageContainerView.axis = UILayoutConstraintAxisHorizontal;
    messageContainerView.alignment = UIStackViewAlignmentTop;
    messageContainerView.layoutMarginsRelativeArrangement = YES;
    messageContainerView.layoutMargins = UIEdgeInsetsMake(
        kVerticalPadding, kHorizontalPadding, 0, kHorizontalPadding);
    [messageContainerView addArrangedSubview:label];
    [headerView addArrangedSubview:messageContainerView];
  }

  if (self.closeButtonImage) {
    UIButton* closeButton = [UIButton buttonWithType:UIButtonTypeCustom];
    closeButton.translatesAutoresizingMaskIntoConstraints = NO;
    [closeButton setImage:self.closeButtonImage forState:UIControlStateNormal];
    closeButton.contentEdgeInsets =
        UIEdgeInsetsMake(kCloseButtonInnerPadding, kCloseButtonInnerPadding,
                         kCloseButtonInnerPadding, kCloseButtonInnerPadding);
    [closeButton addTarget:self
                    action:@selector(didTapClose)
          forControlEvents:UIControlEventTouchUpInside];
    [closeButton setAccessibilityLabel:l10n_util::GetNSString(IDS_CLOSE)];
    if (IsUIRefreshPhase1Enabled()) {
      closeButton.tintColor = [UIColor blackColor];
      closeButton.alpha = 0.20;
    }
    // Prevent the button from shrinking or expanding horizontally.
    [closeButton
        setContentCompressionResistancePriority:UILayoutPriorityRequired
                                        forAxis:
                                            UILayoutConstraintAxisHorizontal];
    [closeButton setContentHuggingPriority:UILayoutPriorityRequired
                                   forAxis:UILayoutConstraintAxisHorizontal];

    UIView* closeButtonContainerView =
        [[UIView alloc] initWithFrame:CGRectZero];
    closeButtonContainerView.translatesAutoresizingMaskIntoConstraints = NO;
    closeButtonContainerView.clipsToBounds = YES;
    [closeButtonContainerView addSubview:closeButton];
    AddSameConstraints(closeButtonContainerView, closeButton);
    [headerView addArrangedSubview:closeButtonContainerView];
  }

  if (self.description.length > 0UL) {
    UILabel* label = [[UILabel alloc] initWithFrame:CGRectZero];
    label.translatesAutoresizingMaskIntoConstraints = NO;
    label.text = self.description;
    label.font = [MDCTypography captionFont];
    label.textColor = [[MDCPalette greyPalette] tint600];
    label.numberOfLines = 0;
    label.lineBreakMode = NSLineBreakByWordWrapping;
    label.backgroundColor = [UIColor clearColor];

    UIStackView* descriptionContainerView =
        [[UIStackView alloc] initWithFrame:CGRectZero];
    descriptionContainerView.translatesAutoresizingMaskIntoConstraints = NO;
    descriptionContainerView.clipsToBounds = YES;
    descriptionContainerView.axis = UILayoutConstraintAxisVertical;
    descriptionContainerView.spacing = kVerticalSpacing;
    [descriptionContainerView addArrangedSubview:label];
    [contentView addArrangedSubview:descriptionContainerView];
  }

  DCHECK(self.cardIssuerIcon);
  DCHECK_GT(self.cardLabel.length, 0UL);
  DCHECK_GT(self.cardSublabel.length, 0UL);

  // The leading edge aligned card details container view. Contains the card
  // issuer network icon, the card label, and the card sublabel.
  UIStackView* cardDetailsContainerView =
      [[UIStackView alloc] initWithArrangedSubviews:@[]];
  cardDetailsContainerView.translatesAutoresizingMaskIntoConstraints = NO;
  cardDetailsContainerView.clipsToBounds = YES;
  cardDetailsContainerView.axis = UILayoutConstraintAxisHorizontal;
  cardDetailsContainerView.spacing = kHorizontalSpacing;
  [contentView addArrangedSubview:cardDetailsContainerView];

  UIImageView* cardIssuerIconImageView =
      [[UIImageView alloc] initWithImage:self.cardIssuerIcon];
  [cardDetailsContainerView addArrangedSubview:cardIssuerIconImageView];

  UILabel* cardLabel = [[UILabel alloc] initWithFrame:CGRectZero];
  cardLabel.translatesAutoresizingMaskIntoConstraints = NO;
  cardLabel.text = self.cardLabel;
  cardLabel.font = [MDCTypography captionFont];
  cardLabel.textColor = [[MDCPalette greyPalette] tint900];
  cardLabel.backgroundColor = [UIColor clearColor];
  [cardDetailsContainerView addArrangedSubview:cardLabel];

  UILabel* cardSublabel = [[UILabel alloc] initWithFrame:CGRectZero];
  cardSublabel.translatesAutoresizingMaskIntoConstraints = NO;
  cardSublabel.text = self.cardSublabel;
  cardSublabel.font = [MDCTypography captionFont];
  cardSublabel.textColor = [[MDCPalette greyPalette] tint600];
  cardSublabel.backgroundColor = [UIColor clearColor];
  [cardDetailsContainerView addArrangedSubview:cardSublabel];

  // Dummy view that expands so that the card details are aligned to the leading
  // Edge of the |contentView|.
  UIView* dummyView = [[UIView alloc] initWithFrame:CGRectZero];
  [dummyView
      setContentCompressionResistancePriority:UILayoutPriorityFittingSizeLevel
                                      forAxis:UILayoutConstraintAxisHorizontal];
  [cardDetailsContainerView addArrangedSubview:dummyView];

  if (self.legalMessages.count > 0UL) {
    UIStackView* legalMessagesView =
        [[UIStackView alloc] initWithArrangedSubviews:@[]];
    legalMessagesView.translatesAutoresizingMaskIntoConstraints = NO;
    legalMessagesView.clipsToBounds = YES;
    legalMessagesView.axis = UILayoutConstraintAxisVertical;
    legalMessagesView.spacing = kVerticalSpacing;
    [contentView addArrangedSubview:legalMessagesView];

    auto block = ^(MessageWithLinks* legalMessage, NSUInteger idx, BOOL* stop) {
      DCHECK_GT(legalMessage.messageText.length, 0UL);
      DCHECK_EQ(legalMessage.linkURLs.size(), legalMessage.linkRanges.count);

      UILabel* label = [[UILabel alloc] initWithFrame:CGRectZero];
      label.translatesAutoresizingMaskIntoConstraints = NO;
      label.text = legalMessage.messageText;
      label.font = [MDCTypography captionFont];
      label.textColor = [[MDCPalette greyPalette] tint600];
      label.numberOfLines = 0;
      label.lineBreakMode = NSLineBreakByWordWrapping;
      label.backgroundColor = [UIColor clearColor];

      if (legalMessage.linkRanges.count > 0UL) {
        __weak SaveCardInfoBarView* weakSelf = self;
        self.legalMessageLinkController =
            [self labelLinkControllerWithLabel:label
                                        action:^(const GURL& URL) {
                                          [weakSelf.delegate
                                              saveCardInfoBarView:weakSelf
                                               didTapLegalLinkURL:URL];
                                        }];
      }
      [legalMessage.linkRanges
          enumerateObjectsUsingBlock:^(NSValue* rangeValue, NSUInteger idx,
                                       BOOL* stop) {
            [self.legalMessageLinkController
                addLinkWithRange:rangeValue.rangeValue
                             url:legalMessage.linkURLs[idx]];
          }];
      [legalMessagesView addArrangedSubview:label];
    };
    [self.legalMessages enumerateObjectsUsingBlock:block];
  }

  if (self.cancelButtonTitle.length > 0UL) {
    UIButton* cancelButton =
        [self actionButtonWithTitle:self.cancelButtonTitle
                            palette:nil
                         titleColor:UIColorFromRGB(kButtonTitleColorBlue)
                             target:self
                             action:@selector(didTapCancel)];

    [footerView addArrangedSubview:cancelButton];
  }

  if (self.confirmButtonTitle.length > 0UL) {
    UIButton* confirmButton =
        [self actionButtonWithTitle:self.confirmButtonTitle
                            palette:[MDCPalette cr_bluePalette]
                         titleColor:[UIColor whiteColor]
                             target:self
                             action:@selector(didTapConfirm)];

    [footerView addArrangedSubview:confirmButton];
  }
}

// Creates and returns an instance of LabelLinkController for |label| and
// |action| which is invoked when links managed by it are tapped.
- (LabelLinkController*)labelLinkControllerWithLabel:(UILabel*)label
                                              action:(ProceduralBlockWithURL)
                                                         action {
  LabelLinkController* labelLinkController =
      [[LabelLinkController alloc] initWithLabel:label action:action];
  [labelLinkController setLinkColor:[[MDCPalette cr_bluePalette] tint500]];
  return labelLinkController;
}

// Creates and returns an infobar action button initialized with the given
// title, colors, and action.
- (UIButton*)actionButtonWithTitle:(NSString*)title
                           palette:(MDCPalette*)palette
                        titleColor:(UIColor*)titleColor
                            target:(id)target
                            action:(SEL)action {
  MDCFlatButton* button = [[MDCFlatButton alloc] init];
  button.translatesAutoresizingMaskIntoConstraints = NO;
  button.titleLabel.adjustsFontSizeToFitWidth = YES;
  button.titleLabel.minimumScaleFactor = 0.6f;
  [button setTitle:title forState:UIControlStateNormal];
  [button addTarget:target
                action:action
      forControlEvents:UIControlEventTouchUpInside];
  [button setUnderlyingColorHint:[UIColor blackColor]];

  if (IsRefreshInfobarEnabled()) {
    button.uppercaseTitle = NO;
    button.layer.cornerRadius = kButtonCornerRadius;
    [button
        setTitleFont:[UIFont preferredFontForTextStyle:UIFontTextStyleHeadline]
            forState:UIControlStateNormal];
  }

  if (palette) {
    button.hasOpaqueBackground = YES;
    button.inkColor = [[palette tint300] colorWithAlphaComponent:0.5f];
    [button setBackgroundColor:[palette tint500] forState:UIControlStateNormal];
  }

  if (titleColor) {
    button.tintAdjustmentMode = UIViewTintAdjustmentModeNormal;
    button.tintColor = titleColor;
    [button setTitleColor:titleColor forState:UIControlStateNormal];
  }

  return button;
}

- (void)didTapClose {
  [self.delegate saveCardInfoBarViewDidTapClose:self];
}

- (void)didTapCancel {
  [self.delegate saveCardInfoBarViewDidTapCancel:self];
}

- (void)didTapConfirm {
  [self.delegate saveCardInfoBarViewDidTapConfirm:self];
}

@end
