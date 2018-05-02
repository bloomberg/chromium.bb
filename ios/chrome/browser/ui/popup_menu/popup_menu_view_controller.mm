// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/popup_menu/popup_menu_view_controller.h"

#import "ios/chrome/browser/ui/commands/browser_commands.h"
#import "ios/chrome/browser/ui/image_util/image_util.h"
#import "ios/chrome/browser/ui/util/constraints_ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
const CGFloat kImageMargin = 196;
const CGFloat kCornerRadius = 13;
const CGFloat kBackgroundGreyScale = 0.98;
const CGFloat kBackgroundAlpha = 0.65;
}  // namespace

@interface PopupMenuViewController ()<UIGestureRecognizerDelegate>
// Redefined as readwrite.
@property(nonatomic, strong, readwrite) UIView* contentContainer;
@end

@implementation PopupMenuViewController

@synthesize contentContainer = _contentContainer;
@synthesize commandHandler = _commandHandler;

#pragma mark - Public

- (instancetype)init {
  self = [super initWithNibName:nil bundle:nil];
  if (self) {
    [self setUpContentContainer];
    UITapGestureRecognizer* gestureRecognizer = [[UITapGestureRecognizer alloc]
        initWithTarget:self
                action:@selector(touchOnScrim:)];
    gestureRecognizer.delegate = self;
    [self.view addGestureRecognizer:gestureRecognizer];
  }
  return self;
}

- (void)addContent:(UIViewController*)content {
  [self addChildViewController:content];
  content.view.translatesAutoresizingMaskIntoConstraints = NO;
  [self.contentContainer addSubview:content.view];
  AddSameConstraints(self.contentContainer, content.view);
  [content didMoveToParentViewController:self];
}

#pragma mark - Private

// Sets the content container view up.
- (void)setUpContentContainer {
  _contentContainer = [[UIView alloc] init];

  if (UIAccessibilityIsReduceTransparencyEnabled()) {
    _contentContainer.backgroundColor =
        [UIColor colorWithWhite:kBackgroundGreyScale alpha:1];
  } else {
    UIBlurEffect* blurEffect =
        [UIBlurEffect effectWithStyle:UIBlurEffectStyleExtraLight];
    UIVisualEffectView* blur =
        [[UIVisualEffectView alloc] initWithEffect:blurEffect];
    blur.translatesAutoresizingMaskIntoConstraints = NO;
    blur.layer.cornerRadius = kCornerRadius;
    blur.clipsToBounds = YES;
    blur.backgroundColor =
        [UIColor colorWithWhite:kBackgroundGreyScale alpha:kBackgroundAlpha];
    [_contentContainer addSubview:blur];
    AddSameConstraints(_contentContainer, blur);
  }

  UIImageView* shadow =
      [[UIImageView alloc] initWithImage:StretchableImageNamed(@"menu_shadow")];
  [_contentContainer addSubview:shadow];
  shadow.translatesAutoresizingMaskIntoConstraints = NO;
  AddSameConstraintsToSidesWithInsets(
      _contentContainer, shadow,
      LayoutSides::kTop | LayoutSides::kBottom | LayoutSides::kLeading |
          LayoutSides::kTrailing,
      ChromeDirectionalEdgeInsetsMake(kImageMargin, kImageMargin, kImageMargin,
                                      kImageMargin));
  _contentContainer.layer.cornerRadius = kCornerRadius;
  _contentContainer.translatesAutoresizingMaskIntoConstraints = NO;
  [self.view addSubview:_contentContainer];
}

// Handler receiving the touch event on the background scrim.
- (void)touchOnScrim:(UITapGestureRecognizer*)recognizer {
  if (recognizer.state == UIGestureRecognizerStateEnded) {
    [self.commandHandler dismissPopupMenuAnimated:YES];
  }
}

#pragma mark - UIGestureRecognizerDelegate

- (BOOL)gestureRecognizer:(UIGestureRecognizer*)gestureRecognizer
       shouldReceiveTouch:(UITouch*)touch {
  // Only get the touch on the scrim.
  return touch.view == self.view;
}

@end
