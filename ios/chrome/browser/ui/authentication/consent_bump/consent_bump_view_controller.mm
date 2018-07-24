// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/consent_bump/consent_bump_view_controller.h"

#import "ios/chrome/browser/ui/authentication/consent_bump/consent_bump_view_controller_delegate.h"
#import "ios/chrome/common/ui_util/constraints_ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
const CGFloat kMargin = 16;
const CGFloat kGradientHeight = 40;
}  // namespace

@interface ConsentBumpViewController ()

@property(nonatomic, strong) UIView* buttonContainer;
@property(nonatomic, strong) UIButton* primaryButton;
@property(nonatomic, strong) UIButton* secondaryButton;
@property(nonatomic, strong) UIView* gradientView;
@property(nonatomic, strong) CAGradientLayer* gradientLayer;

@end

@implementation ConsentBumpViewController

@synthesize delegate = _delegate;
@synthesize contentViewController = _contentViewController;
@synthesize buttonContainer = _buttonContainer;
@synthesize primaryButton = _primaryButton;
@synthesize secondaryButton = _secondaryButton;
@synthesize gradientView = _gradientView;
@synthesize gradientLayer = _gradientLayer;

#pragma mark - Public

- (void)setContentViewController:(UIViewController*)contentViewController {
  if (_contentViewController == contentViewController)
    return;

  // Remove previous VC.
  [_contentViewController willMoveToParentViewController:nil];
  [_contentViewController.view removeFromSuperview];
  [_contentViewController removeFromParentViewController];

  _contentViewController = contentViewController;

  if (!contentViewController)
    return;

  [self addChildViewController:contentViewController];

  contentViewController.view.translatesAutoresizingMaskIntoConstraints = NO;
  [self.view insertSubview:contentViewController.view
              belowSubview:self.gradientView];
  AddSameConstraintsToSides(
      self.view, contentViewController.view,
      LayoutSides::kTop | LayoutSides::kLeading | LayoutSides::kTrailing);
  [contentViewController.view.bottomAnchor
      constraintEqualToAnchor:self.buttonContainer.topAnchor]
      .active = YES;

  [contentViewController didMoveToParentViewController:self];
}

#pragma mark - Property

- (UIButton*)primaryButton {
  if (!_primaryButton) {
    _primaryButton = [UIButton buttonWithType:UIButtonTypeSystem];
    _primaryButton.translatesAutoresizingMaskIntoConstraints = NO;
    [_primaryButton setTitle:@"Primary Button" forState:UIControlStateNormal];
    _primaryButton.backgroundColor = [UIColor blueColor];
    [_primaryButton setContentHuggingPriority:UILayoutPriorityDefaultHigh
                                      forAxis:UILayoutConstraintAxisVertical];
    [_primaryButton addTarget:self
                       action:@selector(primaryButtonCallback)
             forControlEvents:UIControlEventTouchUpInside];
  }
  return _primaryButton;
}

- (UIButton*)secondaryButton {
  if (!_secondaryButton) {
    _secondaryButton = [UIButton buttonWithType:UIButtonTypeSystem];
    _secondaryButton.translatesAutoresizingMaskIntoConstraints = NO;
    [_secondaryButton setTitle:@"Secondary Button"
                      forState:UIControlStateNormal];
    [_secondaryButton setContentHuggingPriority:UILayoutPriorityDefaultHigh
                                        forAxis:UILayoutConstraintAxisVertical];
    [_secondaryButton addTarget:self
                         action:@selector(secondaryButtonCallback)
               forControlEvents:UIControlEventTouchUpInside];
  }
  return _secondaryButton;
}

- (UIView*)buttonContainer {
  if (!_buttonContainer) {
    _buttonContainer = [[UIView alloc] init];
    _buttonContainer.translatesAutoresizingMaskIntoConstraints = NO;
  }
  return _buttonContainer;
}

- (UIView*)gradientView {
  if (!_gradientView) {
    _gradientView = [[UIView alloc] initWithFrame:CGRectZero];
    _gradientView.userInteractionEnabled = NO;
    _gradientView.translatesAutoresizingMaskIntoConstraints = NO;
    [_gradientView.layer insertSublayer:self.gradientLayer atIndex:0];
  }
  return _gradientView;
}

- (CAGradientLayer*)gradientLayer {
  if (!_gradientLayer) {
    _gradientLayer = [CAGradientLayer layer];
    _gradientLayer.colors = @[
      (id)[[UIColor colorWithWhite:1 alpha:0] CGColor],
      (id)[self.view.backgroundColor CGColor]
    ];
  }
  return _gradientLayer;
}

#pragma mark - UIViewController

- (void)viewDidLayoutSubviews {
  [super viewDidLayoutSubviews];
  self.gradientLayer.frame = self.gradientView.bounds;
}

- (void)viewDidLoad {
  self.view.backgroundColor = [UIColor whiteColor];

  // Add subviews.
  [self.buttonContainer addSubview:self.primaryButton];
  [self.buttonContainer addSubview:self.secondaryButton];
  [self.view addSubview:self.buttonContainer];
  [self.view addSubview:self.gradientView];

  // Constraints.
  id<LayoutGuideProvider> safeArea = SafeAreaLayoutGuideForView(self.view);
  AddSameConstraintsToSides(self.view, self.gradientView,
                            LayoutSides::kLeading | LayoutSides::kTrailing);
  AddSameConstraintsToSides(
      safeArea, self.buttonContainer,
      LayoutSides::kBottom | LayoutSides::kLeading | LayoutSides::kTrailing);
  AddSameConstraintsToSidesWithInsets(
      self.secondaryButton, self.buttonContainer,
      LayoutSides::kLeading | LayoutSides::kTop | LayoutSides::kBottom,
      ChromeDirectionalEdgeInsetsMake(kMargin, kMargin, kMargin, 0));
  AddSameConstraintsToSidesWithInsets(
      self.primaryButton, self.buttonContainer,
      LayoutSides::kTrailing | LayoutSides::kTop | LayoutSides::kBottom,
      ChromeDirectionalEdgeInsetsMake(kMargin, 0, kMargin, kMargin));
  [NSLayoutConstraint activateConstraints:@[
    [self.gradientView.heightAnchor constraintEqualToConstant:kGradientHeight],
    [self.gradientView.bottomAnchor
        constraintEqualToAnchor:self.buttonContainer.topAnchor],
    [self.primaryButton.leadingAnchor
        constraintGreaterThanOrEqualToAnchor:self.secondaryButton
                                                 .trailingAnchor],
  ]];
}

#pragma mark - Private

- (void)primaryButtonCallback {
  [self.delegate consentBumpViewControllerDidTapPrimaryButton:self];
}

- (void)secondaryButtonCallback {
  [self.delegate consentBumpViewControllerDidTapSecondaryButton:self];
}

@end
