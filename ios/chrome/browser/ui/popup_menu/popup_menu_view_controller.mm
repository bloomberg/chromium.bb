// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/popup_menu/popup_menu_view_controller.h"

#import "ios/chrome/browser/ui/commands/browser_commands.h"
#import "ios/chrome/browser/ui/util/constraints_ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
const CGFloat kCornerRadius = 13;
const CGFloat kShadowRadius = 10;
const CGFloat kShadowOpacity = 0.3;
const CGFloat kBlurBackgroundGreyScale = 0.98;
const CGFloat kBlurBackgroundAlpha = 0.95;
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
        [UIColor colorWithWhite:kBlurBackgroundGreyScale alpha:1];
  } else {
    _contentContainer.backgroundColor = [UIColor clearColor];
    UIBlurEffect* blurEffect =
        [UIBlurEffect effectWithStyle:UIBlurEffectStyleLight];
    UIVisualEffectView* blur =
        [[UIVisualEffectView alloc] initWithEffect:blurEffect];
    blur.contentView.backgroundColor =
        [UIColor colorWithWhite:kBlurBackgroundGreyScale
                          alpha:kBlurBackgroundAlpha];
    [_contentContainer addSubview:blur];
    blur.translatesAutoresizingMaskIntoConstraints = NO;
    blur.layer.cornerRadius = kCornerRadius;
    blur.layer.masksToBounds = YES;
    AddSameConstraints(blur, _contentContainer);
  }

  _contentContainer.layer.cornerRadius = kCornerRadius;
  _contentContainer.layer.shadowRadius = kShadowRadius;
  _contentContainer.layer.shadowOpacity = kShadowOpacity;
  _contentContainer.translatesAutoresizingMaskIntoConstraints = NO;
  // TODO(crbug.com/821765): Add update the shadow.
  [self.view addSubview:_contentContainer];
}

// Handler receiving the touch event on the background scrim.
- (void)touchOnScrim:(UITapGestureRecognizer*)recognizer {
  if (recognizer.state == UIGestureRecognizerStateEnded) {
    [self.commandHandler dismissPopupMenu];
  }
}

#pragma mark - UIGestureRecognizerDelegate

- (BOOL)gestureRecognizer:(UIGestureRecognizer*)gestureRecognizer
       shouldReceiveTouch:(UITouch*)touch {
  // Only get the touch on the scrim.
  return touch.view == self.view;
}

@end
