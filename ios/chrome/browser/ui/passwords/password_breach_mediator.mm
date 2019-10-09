// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/passwords/password_breach_mediator.h"

#include "base/strings/sys_string_conversions.h"
#include "components/password_manager/core/browser/leak_detection_dialog_utils.h"
#import "ios/chrome/browser/ui/commands/application_commands.h"
#import "ios/chrome/browser/ui/commands/open_new_tab_command.h"
#import "ios/chrome/browser/ui/passwords/password_breach_consumer.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using base::SysUTF16ToNSString;
using password_manager::CredentialLeakType;
using password_manager::GetAcceptButtonLabel;
using password_manager::GetCancelButtonLabel;
using password_manager::GetDescription;
using password_manager::GetTitle;
using password_manager::ShouldCheckPasswords;
using password_manager::GetPasswordCheckupURL;

@interface PasswordBreachMediator ()

// The presenter of the feature.
@property(nonatomic, weak) id<PasswordBreachPresenter> presenter;

// Dispatcher.
@property(nonatomic, weak) id<ApplicationCommands> dispatcher;

@end

@implementation PasswordBreachMediator

- (instancetype)initWithConsumer:(id<PasswordBreachConsumer>)consumer
                       presenter:(id<PasswordBreachPresenter>)presenter
                      dispatcher:(id<ApplicationCommands>)dispatcher
                             URL:(const GURL&)URL
                        leakType:(CredentialLeakType)leakType {
  self = [super init];
  if (self) {
    _presenter = presenter;
    _dispatcher = dispatcher;
    NSString* subtitle = SysUTF16ToNSString(GetDescription(leakType, URL));
    NSString* primaryActionString =
        SysUTF16ToNSString(GetAcceptButtonLabel(leakType));
    [consumer setTitleString:SysUTF16ToNSString(GetTitle(leakType))
                subtitleString:subtitle
           primaryActionString:primaryActionString
        primaryActionAvailable:ShouldCheckPasswords(leakType)];
  }
  return self;
}

#pragma mark - PasswordBreachConsumerDelegate

- (void)passwordBreachDone {
  [self.presenter stop];
}

- (void)passwordBreachPrimaryAction {
  OpenNewTabCommand* newTabCommand =
      [OpenNewTabCommand commandWithURLFromChrome:GetPasswordCheckupURL()];
  [self.dispatcher openURLInNewTab:newTabCommand];
}

@end
