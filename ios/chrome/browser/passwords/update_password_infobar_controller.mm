// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/passwords/update_password_infobar_controller.h"

#import "base/mac/objc_property_releaser.h"
#include "base/strings/string_util.h"
#include "base/strings/sys_string_conversions.h"
#include "ios/chrome/browser/infobars/confirm_infobar_controller+protected.h"
#include "ios/chrome/browser/passwords/ios_chrome_update_password_infobar_delegate.h"
#import "ios/chrome/browser/ui/elements/selector_coordinator.h"
#import "ios/chrome/browser/ui/infobars/infobar_view.h"

namespace {
// Tag for the account link in the info bar message. Set to 10 to avoid conflict
// with tags from superclass ConfirmInfoBarController, which uses tags 1-4.
NSUInteger kAccountTag = 10;
}

@interface UpdatePasswordInfoBarController ()<SelectorCoordinatorDelegate> {
  base::mac::ObjCPropertyReleaser
      _propertyReleaser_UpdatePasswordInfoBarController;
  IOSChromeUpdatePasswordInfoBarDelegate* _delegate;
}
@property(nonatomic, retain) SelectorCoordinator* selectorCoordinator;
@end

@implementation UpdatePasswordInfoBarController

@synthesize selectorCoordinator = _selectorCoordinator;

- (instancetype)initWithDelegate:(InfoBarViewDelegate*)delegate {
  self = [super initWithDelegate:delegate];
  if (self) {
    _propertyReleaser_UpdatePasswordInfoBarController.Init(
        self, [UpdatePasswordInfoBarController class]);
  }
  return self;
}

- (InfoBarView*)viewForDelegate:
                    (IOSChromeUpdatePasswordInfoBarDelegate*)delegate
                          frame:(CGRect)frame {
  _delegate = delegate;
  return [super viewForDelegate:delegate frame:frame];
}

- (void)updateInfobarLabel:(InfoBarView*)view {
  [super updateInfobarLabel:view];

  // Get the message text with current links marked.
  base::string16 messageText = base::SysNSStringToUTF16(view.markedLabel);
  // If there are multiple possible credentials, turn the account string into a
  // link.
  if (_delegate->ShowMultipleAccounts()) {
    base::string16 usernameLink = base::SysNSStringToUTF16([[view class]
        stringAsLink:base::SysUTF16ToNSString(_delegate->selected_account())
                 tag:kAccountTag]);
    base::ReplaceFirstSubstringAfterOffset(
        &messageText, 0, _delegate->selected_account(), usernameLink);
  }

  [view addLabel:base::SysUTF16ToNSString(messageText)
          target:self
          action:@selector(infobarLinkDidPress:)];
}

- (void)infobarLinkDidPress:(NSNumber*)tag {
  [super infobarLinkDidPress:tag];
  if ([tag unsignedIntegerValue] != kAccountTag)
    return;

  UIViewController* baseViewController =
      [[UIApplication sharedApplication] keyWindow].rootViewController;
  self.selectorCoordinator = [[[SelectorCoordinator alloc]
      initWithBaseViewController:baseViewController] autorelease];
  self.selectorCoordinator.delegate = self;
  self.selectorCoordinator.options =
      [NSOrderedSet orderedSetWithArray:_delegate->GetAccounts()];
  self.selectorCoordinator.defaultOption =
      base::SysUTF16ToNSString(_delegate->selected_account());
  [self.selectorCoordinator start];
}

#pragma mark SelectorCoordinatorDelegate

- (void)selectorCoordinator:(SelectorCoordinator*)coordinator
    didCompleteWithSelection:(NSString*)selection {
  _delegate->set_selected_account(base::SysNSStringToUTF16(selection));
  [self updateInfobarLabel:self.view];
}

@end
