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

// Padding used on the edges of the infobar.
const CGFloat kPadding = 16;

// Padding used on the close button to increase the touchable area. This added
// to the close button icon's intrinsic padding equals kPadding.
const CGFloat kCloseButtonPadding = 12;

// Intrinsic padding of the icon. Used to set a padding on the icon that equals
// kPadding.
const CGFloat kIconIntrinsicPadding = 6;

// Vertical spacing between the views.
const CGFloat kVerticalSpacing = 16;

// Horizontal spacing between the views.
const CGFloat kHorizontalSpacing = 8;

// Color in RGB to be used as tint color on the icon.
const CGFloat kIconTintColor = 0x1A73E8;

// Padding used on the top edge of the action buttons' container.
const CGFloat kButtonsTopPadding = 24;

// Color in RGB used as background of the action buttons.
const int kButtonTitleColorBlue = 0x4285f4;

// Corner radius for action buttons.
const CGFloat kButtonCornerRadius = 8.0;

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
@synthesize icon = _icon;
@synthesize googlePayIcon = _googlePayIcon;
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

  // The top container view that lays out the icon, the content, and the close
  // button hotizontally.
  UIStackView* horizontalLayoutView =
      [[UIStackView alloc] initWithArrangedSubviews:@[]];
  horizontalLayoutView.translatesAutoresizingMaskIntoConstraints = NO;
  horizontalLayoutView.clipsToBounds = YES;
  horizontalLayoutView.axis = UILayoutConstraintAxisHorizontal;
  horizontalLayoutView.alignment = UIStackViewAlignmentTop;
  [self addSubview:horizontalLayoutView];
  id<LayoutGuideProvider> layoutGuide = SafeAreaLayoutGuideForView(self);
  [NSLayoutConstraint activateConstraints:@[
    [layoutGuide.leadingAnchor
        constraintEqualToAnchor:horizontalLayoutView.leadingAnchor],
    [layoutGuide.trailingAnchor
        constraintEqualToAnchor:horizontalLayoutView.trailingAnchor],
    [layoutGuide.topAnchor
        constraintEqualToAnchor:horizontalLayoutView.topAnchor],
  ]];

  // The icon is fixed to the top leading edge.
  if (self.icon) {
    UIImageView* iconImageView = [[UIImageView alloc] initWithImage:self.icon];
    if (IsRefreshInfobarEnabled()) {
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
    iconContainerView.layoutMargins = UIEdgeInsetsMake(
        kPadding - kIconIntrinsicPadding, kPadding - kIconIntrinsicPadding, 0,
        kHorizontalSpacing - kIconIntrinsicPadding);
    [iconContainerView addArrangedSubview:iconImageView];
    [horizontalLayoutView addArrangedSubview:iconContainerView];
  }

  // The middle container view that lays out the header and the content
  // vertically.
  UIStackView* verticalLayoutView =
      [[UIStackView alloc] initWithArrangedSubviews:@[]];
  verticalLayoutView.translatesAutoresizingMaskIntoConstraints = NO;
  verticalLayoutView.clipsToBounds = YES;
  verticalLayoutView.axis = UILayoutConstraintAxisVertical;
  verticalLayoutView.spacing = kVerticalSpacing;
  verticalLayoutView.layoutMarginsRelativeArrangement = YES;
  verticalLayoutView.layoutMargins =
      UIEdgeInsetsMake(kPadding, self.icon ? 0 : kPadding, 0, 0);
  [horizontalLayoutView addArrangedSubview:verticalLayoutView];

  // Add the header view.
  [verticalLayoutView addArrangedSubview:[self headerView]];

  // Add the content view.
  [verticalLayoutView addArrangedSubview:[self contentView]];

  // The close button is fixed to the top trailing edge.
  if (self.closeButtonImage) {
    UIButton* closeButton = [UIButton buttonWithType:UIButtonTypeCustom];
    closeButton.translatesAutoresizingMaskIntoConstraints = NO;
    [closeButton setImage:self.closeButtonImage forState:UIControlStateNormal];
    closeButton.contentEdgeInsets =
        UIEdgeInsetsMake(kCloseButtonPadding, kCloseButtonPadding,
                         kCloseButtonPadding, kCloseButtonPadding);
    [closeButton addTarget:self
                    action:@selector(didTapClose)
          forControlEvents:UIControlEventTouchUpInside];
    [closeButton setAccessibilityLabel:l10n_util::GetNSString(IDS_CLOSE)];
    if (IsRefreshInfobarEnabled()) {
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

    [horizontalLayoutView addArrangedSubview:closeButton];
  }

  // The bottom trailing edge aligned view containing the action buttons.
  UIStackView* footerView = [[UIStackView alloc] initWithArrangedSubviews:@[]];
  footerView.translatesAutoresizingMaskIntoConstraints = NO;
  footerView.clipsToBounds = YES;
  footerView.axis = UILayoutConstraintAxisHorizontal;
  footerView.spacing = kHorizontalSpacing;
  footerView.layoutMarginsRelativeArrangement = YES;
  footerView.layoutMargins =
      UIEdgeInsetsMake(kButtonsTopPadding, kPadding, kPadding, kPadding);
  [self addSubview:footerView];
  [NSLayoutConstraint activateConstraints:@[
    [layoutGuide.leadingAnchor
        constraintLessThanOrEqualToAnchor:footerView.leadingAnchor],
    [layoutGuide.trailingAnchor
        constraintEqualToAnchor:footerView.trailingAnchor],
    [horizontalLayoutView.bottomAnchor
        constraintEqualToAnchor:footerView.topAnchor],
    [layoutGuide.bottomAnchor constraintEqualToAnchor:footerView.bottomAnchor],
  ]];

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

// Returns the view containing the infobar header.
- (UIView*)headerView {
  UIStackView* headerView = [[UIStackView alloc] initWithArrangedSubviews:@[]];
  headerView.translatesAutoresizingMaskIntoConstraints = NO;
  headerView.clipsToBounds = YES;
  headerView.axis = UILayoutConstraintAxisHorizontal;
  headerView.alignment = UIStackViewAlignmentCenter;
  headerView.spacing = kHorizontalSpacing;

  if (self.googlePayIcon) {
    UIImageView* iconImageView =
        [[UIImageView alloc] initWithImage:self.googlePayIcon];
    // Prevent the icon from shrinking or expanding horizontally.
    [iconImageView
        setContentCompressionResistancePriority:UILayoutPriorityRequired
                                        forAxis:
                                            UILayoutConstraintAxisHorizontal];
    [iconImageView setContentHuggingPriority:UILayoutPriorityRequired
                                     forAxis:UILayoutConstraintAxisHorizontal];

    [headerView addArrangedSubview:iconImageView];
  }

  if (self.message) {
    DCHECK_GT(self.message.messageText.length, 0UL);
    DCHECK_EQ(self.message.linkURLs.size(), self.message.linkRanges.count);

    UILabel* label = [[UILabel alloc] initWithFrame:CGRectZero];
    label.translatesAutoresizingMaskIntoConstraints = NO;
    label.clipsToBounds = YES;
    label.textColor = [[MDCPalette greyPalette] tint900];
    label.numberOfLines = 0;
    label.backgroundColor = [UIColor clearColor];
    NSMutableParagraphStyle* paragraphStyle =
        [[NSMutableParagraphStyle alloc] init];
    paragraphStyle.lineBreakMode = NSLineBreakByWordWrapping;
    paragraphStyle.lineSpacing = 1.5;
    NSDictionary* attributes = @{
      NSParagraphStyleAttributeName : paragraphStyle,
      NSFontAttributeName : InfoBarMessageFont(),
    };
    [label setAttributedText:[[NSAttributedString alloc]
                                 initWithString:self.message.messageText
                                     attributes:attributes]];

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
    [messageContainerView addArrangedSubview:label];
    [headerView addArrangedSubview:messageContainerView];
  }

  return headerView;
}

// Returns the view containing the infobar content.
- (UIView*)contentView {
  UIStackView* contentView = [[UIStackView alloc] initWithArrangedSubviews:@[]];
  contentView.translatesAutoresizingMaskIntoConstraints = NO;
  contentView.clipsToBounds = YES;
  contentView.axis = UILayoutConstraintAxisVertical;
  contentView.spacing = kVerticalSpacing;

  // Description.
  if (self.description.length > 0UL) {
    UILabel* label = [[UILabel alloc] initWithFrame:CGRectZero];
    label.translatesAutoresizingMaskIntoConstraints = NO;
    label.textColor = [[MDCPalette greyPalette] tint700];
    label.numberOfLines = 0;
    label.backgroundColor = [UIColor clearColor];
    NSMutableParagraphStyle* paragraphStyle =
        [[NSMutableParagraphStyle alloc] init];
    paragraphStyle.lineBreakMode = NSLineBreakByWordWrapping;
    paragraphStyle.lineSpacing = 1.4;
    NSDictionary* attributes = @{
      NSParagraphStyleAttributeName : paragraphStyle,
      NSFontAttributeName : [MDCTypography body1Font],
    };
    [label setAttributedText:[[NSAttributedString alloc]
                                 initWithString:self.description
                                     attributes:attributes]];

    [contentView addArrangedSubview:label];
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
  cardLabel.font = [MDCTypography body1Font];
  cardLabel.textColor = [[MDCPalette greyPalette] tint900];
  cardLabel.backgroundColor = [UIColor clearColor];
  [cardDetailsContainerView addArrangedSubview:cardLabel];

  UILabel* cardSublabel = [[UILabel alloc] initWithFrame:CGRectZero];
  cardSublabel.translatesAutoresizingMaskIntoConstraints = NO;
  cardSublabel.text = self.cardSublabel;
  cardSublabel.font = [MDCTypography body1Font];
  cardSublabel.textColor = [[MDCPalette greyPalette] tint700];
  cardSublabel.backgroundColor = [UIColor clearColor];
  [cardDetailsContainerView addArrangedSubview:cardSublabel];

  // Dummy view that expands so that the card details are aligned to the leading
  // Edge of the |contentView|.
  UIView* dummyView = [[UIView alloc] initWithFrame:CGRectZero];
  [dummyView
      setContentCompressionResistancePriority:UILayoutPriorityFittingSizeLevel
                                      forAxis:UILayoutConstraintAxisHorizontal];
  [cardDetailsContainerView addArrangedSubview:dummyView];

  // Legal messages.
  auto block = ^(MessageWithLinks* legalMessage, NSUInteger idx, BOOL* stop) {
    DCHECK_GT(legalMessage.messageText.length, 0UL);
    DCHECK_EQ(legalMessage.linkURLs.size(), legalMessage.linkRanges.count);

    UILabel* label = [[UILabel alloc] initWithFrame:CGRectZero];
    label.translatesAutoresizingMaskIntoConstraints = NO;
    label.textColor = [[MDCPalette greyPalette] tint700];
    label.numberOfLines = 0;
    label.backgroundColor = [UIColor clearColor];
    NSMutableParagraphStyle* paragraphStyle =
        [[NSMutableParagraphStyle alloc] init];
    paragraphStyle.lineBreakMode = NSLineBreakByWordWrapping;
    paragraphStyle.lineSpacing = 1.4;
    NSDictionary* attributes = @{
      NSParagraphStyleAttributeName : paragraphStyle,
      NSFontAttributeName : [MDCTypography body1Font],
    };
    [label setAttributedText:[[NSAttributedString alloc]
                                 initWithString:legalMessage.messageText
                                     attributes:attributes]];

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
    [contentView addArrangedSubview:label];
  };
  [self.legalMessages enumerateObjectsUsingBlock:block];

  return contentView;
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
