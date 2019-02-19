// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/infobars/banners/infobar_banner_view_controller.h"

#import "ios/chrome/browser/ui/infobars/banners/infobar_banner_delegate.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface InfobarBannerViewController ()

// The original position of this InfobarVC view in the parent's view coordinate
// system.
@property(nonatomic, assign) CGPoint originalCenter;
// Delegate to handle this InfobarVC actions.
@property(nonatomic, weak) id<InfobarBannerDelegate> delegate;

@end

// TODO(crbug.com/1372916): PLACEHOLDER Work in Progress class for the new
// InfobarUI.
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

  UILabel* messageLabel = [[UILabel alloc] init];
  messageLabel.text = self.messageText;
  messageLabel.font = [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
  messageLabel.adjustsFontForContentSizeCategory = YES;
  messageLabel.textColor = [UIColor blackColor];
  messageLabel.translatesAutoresizingMaskIntoConstraints = NO;
  messageLabel.numberOfLines = 0;

  UIButton* infobarButton = [UIButton buttonWithType:UIButtonTypeSystem];
  [infobarButton setTitle:self.buttonText forState:UIControlStateNormal];
  [infobarButton addTarget:self.delegate
                    action:@selector(bannerInfobarButtonWasPressed:)
          forControlEvents:UIControlEventTouchUpInside];
  infobarButton.translatesAutoresizingMaskIntoConstraints = NO;

  UIPanGestureRecognizer* panGestureRecognizer =
      [[UIPanGestureRecognizer alloc] init];
  [panGestureRecognizer addTarget:self action:@selector(handlePanGesture:)];
  [panGestureRecognizer setMaximumNumberOfTouches:1];
  [self.view addGestureRecognizer:panGestureRecognizer];

  [self.view addSubview:messageLabel];
  [self.view addSubview:infobarButton];
  self.view.backgroundColor = [UIColor lightGrayColor];

  [NSLayoutConstraint activateConstraints:@[
    [messageLabel.leadingAnchor constraintEqualToAnchor:self.view.leadingAnchor
                                               constant:30],
    [messageLabel.trailingAnchor
        constraintEqualToAnchor:infobarButton.leadingAnchor],
    [messageLabel.centerYAnchor
        constraintEqualToAnchor:self.view.centerYAnchor],
    [infobarButton.centerYAnchor
        constraintEqualToAnchor:self.view.centerYAnchor],
    [infobarButton.trailingAnchor
        constraintEqualToAnchor:self.view.trailingAnchor],
    [infobarButton.widthAnchor constraintEqualToConstant:100]
  ]];
}

#pragma mark - Private Methods

- (void)buttonTapped:(id)sender {
  [self dismissViewControllerAnimated:YES completion:nil];
}

- (void)handlePanGesture:(UIPanGestureRecognizer*)gesture {
  CGPoint translation = [gesture translationInView:self.view];

  if (gesture.state == UIGestureRecognizerStateBegan) {
    self.originalCenter = self.view.center;

  } else if (gesture.state == UIGestureRecognizerStateChanged) {
    self.view.center =
        CGPointMake(self.view.center.x, self.view.center.y + translation.y);
  }

  if (gesture.state == UIGestureRecognizerStateEnded ||
      gesture.state == UIGestureRecognizerStateCancelled) {
    if (self.view.center.y > self.originalCenter.y) {
      self.view.center = self.originalCenter;
    } else {
      [self.delegate dismissInfobarBanner:self];
    }
  }

  [gesture setTranslation:CGPointZero inView:self.view];
}

@end
