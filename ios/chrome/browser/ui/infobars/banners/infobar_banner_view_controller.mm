// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/infobars/banners/infobar_banner_view_controller.h"

#include "base/strings/sys_string_conversions.h"
#include "components/infobars/core/confirm_infobar_delegate.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface InfobarBannerViewController ()

@property(nonatomic, readonly) ConfirmInfoBarDelegate* infoBarDelegate;
@property(nonatomic, assign) CGPoint originalCenter;

@end

// TODO(crbug.com/1372916): PLACEHOLDER Work in Progress class for the new
// InfobarUI.
@implementation InfobarBannerViewController
@synthesize delegate = _delegate;

- (instancetype)initWithInfoBarDelegate:
    (ConfirmInfoBarDelegate*)infoBarDelegate {
  self = [super initWithNibName:nil bundle:nil];
  if (self) {
    _infoBarDelegate = infoBarDelegate;
  }
  return self;
}

#pragma mark - View Lifecycle

- (void)viewDidLoad {
  [super viewDidLoad];

  NSString* messageText =
      base::SysUTF16ToNSString(self.infoBarDelegate->GetMessageText());
  UILabel* messageLabel = [[UILabel alloc] init];
  messageLabel.text = messageText;
  messageLabel.font = [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
  messageLabel.adjustsFontForContentSizeCategory = YES;
  messageLabel.textColor = [UIColor blackColor];
  messageLabel.translatesAutoresizingMaskIntoConstraints = NO;
  messageLabel.numberOfLines = 0;

  NSString* buttonText = base::SysUTF16ToNSString(
      self.infoBarDelegate->GetButtonLabel(ConfirmInfoBarDelegate::BUTTON_OK));
  UIButton* infobarButton = [UIButton buttonWithType:UIButtonTypeSystem];
  [infobarButton setTitle:buttonText forState:UIControlStateNormal];
  [infobarButton addTarget:self
                    action:@selector(buttonTapped:)
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

#pragma mark - InfobarUIDelegate

- (void)removeView {
  [self dismissViewControllerAnimated:YES completion:nil];
}

- (void)detachView {
  [self dismissViewControllerAnimated:YES completion:nil];
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
      [self dismissViewControllerAnimated:YES completion:nil];
    }
  }

  [gesture setTranslation:CGPointZero inView:self.view];
}

@end
