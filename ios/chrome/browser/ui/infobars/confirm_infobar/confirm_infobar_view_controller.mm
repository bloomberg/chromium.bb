// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/infobars/confirm_infobar/confirm_infobar_view_controller.h"

#include "base/strings/sys_string_conversions.h"
#include "components/infobars/core/confirm_infobar_delegate.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface ConfirmInfobarViewController ()

@property(nonatomic, readonly) ConfirmInfoBarDelegate* infoBarDelegate;

@end

// TODO(crbug.com/1372916): PLACEHOLDER Work in Progress class for the new
// InfobarUI.
@implementation ConfirmInfobarViewController
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
  base::string16 messageText = self.infoBarDelegate->GetMessageText();
  NSString* message = base::SysUTF16ToNSString(messageText);

  UILabel* messageLabel = [[UILabel alloc] init];
  messageLabel.text = message;
  messageLabel.font = [UIFont preferredFontForTextStyle:UIFontTextStyleTitle1];
  messageLabel.adjustsFontForContentSizeCategory = YES;
  messageLabel.textColor = [UIColor blackColor];
  messageLabel.translatesAutoresizingMaskIntoConstraints = NO;

  [self.view addSubview:messageLabel];
  self.view.backgroundColor = [UIColor grayColor];

  [NSLayoutConstraint activateConstraints:@[
    [messageLabel.leadingAnchor
        constraintEqualToAnchor:self.view.leadingAnchor],
    [messageLabel.topAnchor constraintEqualToAnchor:self.view.topAnchor],
    [messageLabel.heightAnchor constraintEqualToConstant:30],
    [messageLabel.widthAnchor constraintEqualToConstant:200],
  ]];
}

#pragma mark - InfobarUIDelegate

- (void)removeView {
  // TO-DO
}

- (void)detachView {
  // TO-DO
}

@end
