// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/today_extension/physical_web_optin_footer.h"

#import "base/mac/scoped_block.h"
#import "base/mac/scoped_nsobject.h"
#include "ios/chrome/today_extension/interactive_label.h"
#include "ios/chrome/today_extension/lock_screen_state.h"
#import "ios/chrome/today_extension/transparent_button.h"
#import "ios/chrome/today_extension/ui_util.h"
#include "ios/today_extension/grit/ios_today_extension_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

// Vertical space between the title and the description.
const CGFloat kTitleVerticalMargin = 3;
// Font size of the button.
const CGFloat kButtonFontSize = 13;
// Left and right padding inside the button.
const CGFloat kButtonPadding = 8;
// Space between buttons.
const CGFloat kButtonSpacing = 4;
// Height of the buttons.
const CGFloat kButtonHeight = 30;
// Vertical space between the description and the buttons.
const CGFloat kButtonVerticalMargin = 16;
// Font size of the explanation.
const CGFloat kDescriptionFontSize = 14;

}  // namespace

@interface PhysicalWebOptInFooter ()

- (void)optInButtonPressed:(id)sender;
- (void)dismissButtonPressed:(id)sender;

@end

@implementation PhysicalWebOptInFooter {
  // Container of the views of the opt-in dialog.
  base::scoped_nsobject<UIView> _mainView;
  // Title.
  base::scoped_nsobject<UILabel> _titleView;
  // Explanation.
  base::scoped_nsobject<InteractiveLabel> _descriptionLabel;
  // Confirm opt-in.
  base::scoped_nsobject<TransparentButton> _optInButton;
  // Refuse opt-in.
  base::scoped_nsobject<TransparentButton> _dismissButton;
  // Physical Web logo.
  base::scoped_nsobject<UIImageView> _imageView;
  // Outer horizontal padding.
  CGFloat _horizontalPadding;
  // Physical Web logo size.
  CGSize _imageSize;
  // Distance between the icon and the title.
  CGFloat _imageTextSeparator;
  // Opt-in action block.
  base::mac::ScopedBlock<EnableDisableBlock> _optinAction;
  // Dismiss action block.
  base::mac::ScopedBlock<EnableDisableBlock> _dismissAction;
  // Whether the screen is locked.
  BOOL _locked;
}

- (id)initWithLeftInset:(CGFloat)leftInset
         learnMoreBlock:(LearnMoreBlock)learnMoreBlock
            optinAction:(EnableDisableBlock)optinAction
          dismissAction:(EnableDisableBlock)dismissAction {
  self = [super init];
  if (self) {
    _mainView.reset([[UIView alloc] initWithFrame:CGRectZero]);
    _locked = [[LockScreenState sharedInstance] isScreenLocked];
    [_mainView setTranslatesAutoresizingMaskIntoConstraints:NO];

    [_mainView setUserInteractionEnabled:YES];

    // Creates the layout of the opt-in UI.
    UIImage* iconImage = [UIImage imageNamed:@"todayview_physical_web"];
    _imageSize = [iconImage size];
    _imageView.reset([[UIImageView alloc] init]);
    [_imageView setImage:iconImage];
    [_imageView setTranslatesAutoresizingMaskIntoConstraints:NO];

    _titleView.reset([[UILabel alloc] initWithFrame:CGRectZero]);
    [_titleView setTranslatesAutoresizingMaskIntoConstraints:NO];
    [_titleView setText:l10n_util::GetNSString(
                            IDS_IOS_PYSICAL_WEB_TODAY_EXTENSION_OPTIN_TITLE)];
    UIFont* titleFont =
        [UIFont fontWithName:@"Helvetica" size:ui_util::kTitleFontSize];
    [_titleView setTextColor:ui_util::TitleColor()];
    [_titleView setFont:titleFont];
    [_titleView setTextAlignment:ui_util::IsRTL() ? NSTextAlignmentRight
                                                  : NSTextAlignmentLeft];
    [_titleView sizeToFit];

    NSString* description;
    if (_locked) {
      description = l10n_util::GetNSString(
          IDS_IOS_PYSICAL_WEB_TODAY_EXTENSION_OPTIN_UNLOCK);
    } else {
      description = l10n_util::GetNSString(
          IDS_IOS_PYSICAL_WEB_TODAY_EXTENSION_OPTIN_DESCRIPTION);
    }
    _descriptionLabel.reset([[InteractiveLabel alloc]
         initWithFrame:CGRectZero
           labelString:description
              fontSize:kDescriptionFontSize
        labelAlignment:ui_util::IsRTL() ? NSTextAlignmentRight
                                        : NSTextAlignmentLeft
                insets:UIEdgeInsetsZero
          buttonString:nil
             linkBlock:learnMoreBlock
           buttonBlock:NULL]);
    [_descriptionLabel setTranslatesAutoresizingMaskIntoConstraints:NO];
    UIStackView* textStack = [[[UIStackView alloc]
        initWithArrangedSubviews:@[ _titleView, _descriptionLabel ]]
        autorelease];

    [textStack setAxis:UILayoutConstraintAxisVertical];
    [textStack setDistribution:UIStackViewDistributionEqualSpacing];
    [textStack setSpacing:kTitleVerticalMargin];
    [textStack setTranslatesAutoresizingMaskIntoConstraints:NO];

    [_mainView addSubview:textStack];
    [_mainView addSubview:_imageView];

    _horizontalPadding = leftInset + ui_util::ChromeIconOffset();
    _imageTextSeparator =
        ui_util::ChromeTextOffset() - ui_util::ChromeIconOffset();

    [NSLayoutConstraint activateConstraints:@[
      [[textStack topAnchor] constraintEqualToAnchor:[_imageView topAnchor]],
      [[textStack topAnchor]
          constraintEqualToAnchor:[_mainView topAnchor]
                         constant:ui_util::kSecondLineVerticalPadding],
      [[textStack leadingAnchor]
          constraintEqualToAnchor:[_imageView leadingAnchor]
                         constant:_imageTextSeparator],
      [[textStack trailingAnchor]
          constraintEqualToAnchor:[_mainView trailingAnchor]
                         constant:-_horizontalPadding],
      [[_imageView widthAnchor] constraintEqualToConstant:_imageSize.width],
      [[_imageView heightAnchor] constraintEqualToConstant:_imageSize.height],
      [[_imageView leadingAnchor]
          constraintEqualToAnchor:[_mainView leadingAnchor]
                         constant:_horizontalPadding]
    ]];

    if (!_locked) {
      UIView* buttons = [self createButtonsOptinAction:optinAction
                                         dismissAction:dismissAction];
      [_mainView addSubview:buttons];
      [[buttons topAnchor] constraintEqualToAnchor:[textStack bottomAnchor]
                                          constant:kButtonVerticalMargin]
          .active = YES;
      [[buttons centerXAnchor]
          constraintEqualToAnchor:[_mainView centerXAnchor]]
          .active = YES;
    }
  }
  return self;
}

- (UIView*)createButtonsOptinAction:(EnableDisableBlock)optinAction
                      dismissAction:(EnableDisableBlock)dismissAction {
  UIFont* buttonFont = [UIFont fontWithName:@"Helvetica" size:kButtonFontSize];

  _dismissButton.reset([[TransparentButton alloc] initWithFrame:CGRectZero]);
  [_dismissButton setCornerRadius:ui_util::kUIButtonCornerRadius];
  [[_dismissButton titleLabel] setFont:buttonFont];
  [_dismissButton setInkColor:ui_util::InkColor()];
  [_dismissButton setTitleColor:ui_util::TitleColor()
                       forState:UIControlStateNormal];
  [_dismissButton
      setTitle:l10n_util::GetNSString(
                   IDS_IOS_PYSICAL_WEB_TODAY_EXTENSION_OPTIN_DISMISS)
      forState:UIControlStateNormal];
  [_dismissButton setBorderWidth:1.0];
  [_dismissButton setBorderColor:ui_util::TitleColor()];
  [_dismissButton addTarget:self
                     action:@selector(dismissButtonPressed:)
           forControlEvents:UIControlEventTouchUpInside];
  _dismissAction.reset(dismissAction, base::scoped_policy::RETAIN);
  [_dismissButton setTranslatesAutoresizingMaskIntoConstraints:NO];

  _optInButton.reset([[TransparentButton alloc] initWithFrame:CGRectZero]);
  [_optInButton setCornerRadius:ui_util::kUIButtonCornerRadius];
  [[_optInButton titleLabel] setFont:buttonFont];
  [_optInButton setInkColor:ui_util::InkColor()];
  [_optInButton setBackgroundColor:ui_util::BackgroundColor()];
  [_optInButton setTitleColor:[UIColor blackColor]
                     forState:UIControlStateNormal];
  [_optInButton setTitle:l10n_util::GetNSString(
                             IDS_IOS_PYSICAL_WEB_TODAY_EXTENSION_OPTIN_ACCEPT)
                forState:UIControlStateNormal];
  [_optInButton addTarget:self
                   action:@selector(optInButtonPressed:)
         forControlEvents:UIControlEventTouchUpInside];
  [_optInButton setTranslatesAutoresizingMaskIntoConstraints:NO];

  [[_optInButton heightAnchor] constraintEqualToConstant:kButtonHeight];
  [[_dismissButton heightAnchor] constraintEqualToConstant:kButtonHeight];
  [[_optInButton widthAnchor]
      constraintEqualToAnchor:[_dismissButton widthAnchor]];

  _optinAction.reset(optinAction, base::scoped_policy::RETAIN);
  UIStackView* buttonStack = [[UIStackView alloc]
      initWithArrangedSubviews:@[ _dismissButton, _optInButton ]];
  [buttonStack setUserInteractionEnabled:YES];
  [buttonStack setAxis:UILayoutConstraintAxisHorizontal];
  [buttonStack setSpacing:kButtonSpacing];
  [buttonStack setTranslatesAutoresizingMaskIntoConstraints:NO];

  [[_optInButton heightAnchor] constraintEqualToConstant:kButtonHeight].active =
      YES;
  [[_dismissButton heightAnchor] constraintEqualToConstant:kButtonHeight]
      .active = YES;

  [_optInButton setContentEdgeInsets:UIEdgeInsetsMake(0, kButtonPadding, 0,
                                                      kButtonPadding)];
  [_dismissButton setContentEdgeInsets:UIEdgeInsetsMake(0, kButtonPadding, 0,
                                                        kButtonPadding)];

  [[_optInButton widthAnchor]
      constraintEqualToAnchor:[_dismissButton widthAnchor]]
      .active = YES;
  return [buttonStack autorelease];
}

- (CGFloat)heightForWidth:(CGFloat)width {
  CGFloat height = ui_util::kSecondLineVerticalPadding +
                   [_titleView frame].size.height + kTitleVerticalMargin +
                   [_descriptionLabel
                       sizeThatFits:CGSizeMake(width - (_horizontalPadding * 2 +
                                                        _imageTextSeparator),
                                               CGFLOAT_MAX)]
                       .height +
                   ui_util::kSecondLineVerticalPadding;

  if (!_locked) {
    height += kButtonVerticalMargin + kButtonHeight;
  }
  return height;
}

- (UIView*)view {
  return _mainView;
}

- (void)optInButtonPressed:(id)sender {
  _optinAction.get()();
}

- (void)dismissButtonPressed:(id)sender {
  _dismissAction.get()();
}

@end
