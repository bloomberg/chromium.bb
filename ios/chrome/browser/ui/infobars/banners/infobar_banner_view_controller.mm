// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/infobars/banners/infobar_banner_view_controller.h"

#import "ios/chrome/browser/ui/infobars/banners/infobar_banner_delegate.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Banner View constants.
const CGFloat kBannerViewCornerRadius = 13.0;
const CGFloat kBannerViewYShadowOffset = 5.0;
const CGFloat kBannerViewShadowRadius = 3.0;
const CGFloat kBannerViewShadowOpactiy = 0.08;

// Bottom Grip constants.
const CGFloat kBottomGripCornerRadius = 0.2;
const CGFloat kBottomGripWidth = 44.0;
const CGFloat kBottomGripHeight = 3.0;
const CGFloat kBottomGripBottomPadding = 4.0;
const int kBottomGripBackgroundColor = 0xD8D8D8;

// Labels constants.
const int kTitleLabelColor = 0x202124;
const int kSubTitleLabelColor = 0x7F868C;

// Button constants.
const CGFloat kButtonWidth = 100.0;
const CGFloat kButtonSeparatorWidth = 1.0;
const int kButtonSeparatorColor = 0xF1F3F4;

// Container Stack constants.
const CGFloat kContainerStackSpacing = 18.0;

// Icon constants.
const CGFloat kIconWidth = 25.0;

// PanGesture constants.
const CGFloat kChangeInPositionForTransition = 100.0;
}  // namespace

@interface InfobarBannerViewController ()

// The original position of this InfobarVC view in the parent's view coordinate
// system.
@property(nonatomic, assign) CGPoint originalCenter;
// Delegate to handle this InfobarVC actions.
@property(nonatomic, weak) id<InfobarBannerDelegate> delegate;

@end

@implementation InfobarBannerViewController

- (instancetype)initWithDelegate:(id<InfobarBannerDelegate>)delegate {
  self = [super initWithNibName:nil bundle:nil];
  if (self) {
    _delegate = delegate;
  }
  return self;
}

#pragma mark - View Lifecycle

- (void)viewDidLoad {
  [super viewDidLoad];

  // BannerView setup.
  self.view.backgroundColor = [UIColor whiteColor];
  self.view.layer.cornerRadius = kBannerViewCornerRadius;
  [self.view.layer setShadowColor:[UIColor blackColor].CGColor];
  [self.view.layer setShadowOffset:CGSizeMake(0.0, kBannerViewYShadowOffset)];
  [self.view.layer setShadowRadius:kBannerViewShadowRadius];
  [self.view.layer setShadowOpacity:kBannerViewShadowOpactiy];

  // Bottom Grip setup.
  UIView* bottomGrip = [[UIView alloc] init];
  bottomGrip.backgroundColor = UIColorFromRGB(kBottomGripBackgroundColor);
  bottomGrip.layer.cornerRadius = kBottomGripCornerRadius;
  bottomGrip.translatesAutoresizingMaskIntoConstraints = NO;
  [self.view addSubview:bottomGrip];

  // Icon setup.
  self.iconImage = [self.iconImage
      imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
  UIImageView* iconImageView =
      [[UIImageView alloc] initWithImage:self.iconImage];
  iconImageView.contentMode = UIViewContentModeScaleAspectFit;

  // Labels setup.
  UILabel* titleLabel = [[UILabel alloc] init];
  titleLabel.text = self.titleText;
  titleLabel.font = [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
  titleLabel.adjustsFontForContentSizeCategory = YES;
  titleLabel.textColor = UIColorFromRGB(kTitleLabelColor);
  titleLabel.numberOfLines = 0;
  titleLabel.baselineAdjustment = UIBaselineAdjustmentAlignCenters;

  UILabel* subTitleLabel = [[UILabel alloc] init];
  subTitleLabel.text = self.subTitleText;
  subTitleLabel.font =
      [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];
  subTitleLabel.adjustsFontForContentSizeCategory = YES;
  subTitleLabel.textColor = UIColorFromRGB(kSubTitleLabelColor);
  subTitleLabel.numberOfLines = 0;
  // If |self.subTitleText| hasn't been set or is empty, hide the label to keep
  // the title label centered in the Y axis.
  subTitleLabel.hidden = ![self.subTitleText length];

  UIStackView* labelsStackView = [[UIStackView alloc]
      initWithArrangedSubviews:@[ titleLabel, subTitleLabel ]];
  labelsStackView.axis = UILayoutConstraintAxisVertical;
  labelsStackView.alignment = UIStackViewAlignmentLeading;
  labelsStackView.distribution = UIStackViewDistributionEqualCentering;
  [labelsStackView setContentHuggingPriority:UILayoutPriorityRequired
                                     forAxis:UILayoutConstraintAxisVertical];

  // Button setup.
  UIButton* infobarButton = [UIButton buttonWithType:UIButtonTypeSystem];
  [infobarButton setTitle:self.buttonText forState:UIControlStateNormal];
  infobarButton.titleLabel.font =
      [UIFont preferredFontForTextStyle:UIFontTextStyleHeadline];
  [infobarButton addTarget:self.delegate
                    action:@selector(bannerInfobarButtonWasPressed:)
          forControlEvents:UIControlEventTouchUpInside];

  UIView* buttonSeparator = [[UIView alloc] init];
  buttonSeparator.translatesAutoresizingMaskIntoConstraints = NO;
  buttonSeparator.backgroundColor = UIColorFromRGB(kButtonSeparatorColor);
  [infobarButton addSubview:buttonSeparator];

  // Container Stack setup.
  UIStackView* containerStack = [[UIStackView alloc] initWithArrangedSubviews:@[
    iconImageView, labelsStackView, infobarButton
  ]];
  containerStack.axis = UILayoutConstraintAxisHorizontal;
  containerStack.spacing = kContainerStackSpacing;
  containerStack.distribution = UIStackViewDistributionFill;
  containerStack.alignment = UIStackViewAlignmentFill;
  containerStack.translatesAutoresizingMaskIntoConstraints = NO;
  [self.view addSubview:containerStack];

  // Constraints setup.
  [NSLayoutConstraint activateConstraints:@[
    // Container Stack.
    [containerStack.leadingAnchor
        constraintEqualToAnchor:self.view.leadingAnchor
                       constant:kContainerStackSpacing],
    [containerStack.trailingAnchor
        constraintEqualToAnchor:self.view.trailingAnchor],
    [containerStack.centerYAnchor
        constraintEqualToAnchor:self.view.centerYAnchor],
    // Icon.
    [iconImageView.widthAnchor constraintEqualToConstant:kIconWidth],
    // Button.
    [infobarButton.widthAnchor constraintEqualToConstant:kButtonWidth],
    [buttonSeparator.widthAnchor
        constraintEqualToConstant:kButtonSeparatorWidth],
    [buttonSeparator.leadingAnchor
        constraintEqualToAnchor:infobarButton.leadingAnchor],
    [buttonSeparator.topAnchor constraintEqualToAnchor:self.view.topAnchor],
    [buttonSeparator.bottomAnchor
        constraintEqualToAnchor:self.view.bottomAnchor],
    // Bottom Grip.
    [bottomGrip.widthAnchor constraintEqualToConstant:kBottomGripWidth],
    [bottomGrip.heightAnchor constraintEqualToConstant:kBottomGripHeight],
    [bottomGrip.centerXAnchor constraintEqualToAnchor:self.view.centerXAnchor],
    [bottomGrip.bottomAnchor constraintEqualToAnchor:self.view.bottomAnchor
                                            constant:-kBottomGripBottomPadding],
  ]];

  // Gestures setup.
  UIPanGestureRecognizer* panGestureRecognizer =
      [[UIPanGestureRecognizer alloc] init];
  [panGestureRecognizer addTarget:self action:@selector(handlePanGesture:)];
  [panGestureRecognizer setMaximumNumberOfTouches:1];
  [self.view addGestureRecognizer:panGestureRecognizer];
}

#pragma mark - Private Methods

- (void)buttonTapped:(id)sender {
  [self dismissViewControllerAnimated:YES completion:nil];
}

// TODO(crbug.com/911864): PLACEHOLDER Gesture handling for the new InfobarUI.
- (void)handlePanGesture:(UIPanGestureRecognizer*)gesture {
  CGPoint translation = [gesture translationInView:self.view];

  if (gesture.state == UIGestureRecognizerStateBegan) {
    self.originalCenter = self.view.center;
  } else if (gesture.state == UIGestureRecognizerStateChanged) {
    self.view.center =
        CGPointMake(self.view.center.x, self.view.center.y + translation.y);
    // If the translation in the positive Y axis is larger than
    // kChangeInPositionForTransition then present the InfobarModal.
    if (self.view.center.y - self.originalCenter.y >
        kChangeInPositionForTransition) {
      [self.delegate presentInfobarModalFromBanner];
      return;
    }
  }

  if (gesture.state == UIGestureRecognizerStateEnded ||
      gesture.state == UIGestureRecognizerStateCancelled) {
    // If there's more than a 1px translation in the negative Y axis when the
    // gesture ended dismiss the banner.
    if (self.view.center.y - self.originalCenter.y < 0) {
      [self.delegate dismissInfobarBanner:self];
      return;
    }
    self.view.center = self.originalCenter;
  }

  [gesture setTranslation:CGPointZero inView:self.view];
}

@end
