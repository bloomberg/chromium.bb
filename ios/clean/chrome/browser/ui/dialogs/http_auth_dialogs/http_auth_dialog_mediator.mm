// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/clean/chrome/browser/ui/dialogs/http_auth_dialogs/http_auth_dialog_mediator.h"

#include "base/logging.h"
#include "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/ui/commands/command_dispatcher.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/clean/chrome/browser/ui/commands/http_auth_dialog_commands.h"
#import "ios/clean/chrome/browser/ui/dialogs/dialog_button_configuration.h"
#import "ios/clean/chrome/browser/ui/dialogs/dialog_button_style.h"
#import "ios/clean/chrome/browser/ui/dialogs/dialog_consumer.h"
#import "ios/clean/chrome/browser/ui/dialogs/dialog_mediator+subclassing.h"
#import "ios/clean/chrome/browser/ui/dialogs/dialog_text_field_configuration.h"
#import "ios/clean/chrome/browser/ui/dialogs/http_auth_dialogs/http_auth_dialog_request.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface HTTPAuthDialogMediator ()

// The request for this dialog.
@property(nonatomic, readonly, strong) HTTPAuthDialogRequest* request;
// Dialog configuration identifiers.
@property(nonatomic, weak) DialogConfigurationIdentifier* OKButtonID;
@property(nonatomic, weak) DialogConfigurationIdentifier* cancelButtonID;
@property(nonatomic, weak) DialogConfigurationIdentifier* usernameTextFieldID;
@property(nonatomic, weak) DialogConfigurationIdentifier* passwordTextFieldID;

@end

@implementation HTTPAuthDialogMediator
@synthesize dispatcher = _dispatcher;
@synthesize request = _request;
@synthesize OKButtonID = _OKButtonID;
@synthesize cancelButtonID = cancelButtonID;
@synthesize usernameTextFieldID = usernameTextFieldID;
@synthesize passwordTextFieldID = _passwordTextFieldID;

- (instancetype)initWithRequest:(HTTPAuthDialogRequest*)request {
  DCHECK(request);
  if ((self = [super init])) {
    _request = request;
  }
  return self;
}

#pragma mark - DialogDismissalCommands

- (void)dismissDialogWithButtonID:(id)buttonID
                  textFieldValues:
                      (NSDictionary<id, NSString*>*)textFieldValues {
  NSString* username = nil;
  NSString* password = nil;
  if (buttonID == self.OKButtonID) {
    username = textFieldValues[self.usernameTextFieldID];
    username = username ? username : @"";
    password = textFieldValues[self.passwordTextFieldID];
    password = password ? password : @"";
  } else if (buttonID == self.cancelButtonID) {
    // Use nil in callbacks for cancelled HTTP auth dialogs.
  } else {
    NOTREACHED() << "Received dialog dismissal for unknown button ID.";
  }
  [self.request completeAuthenticationWithUsername:username password:password];
  [self.dispatcher dismissHTTPAuthDialog];
}

@end

@implementation HTTPAuthDialogMediator (DialogMediatorSubclassing)

- (NSString*)dialogTitle {
  return self.request.title;
}

- (NSString*)dialogMessage {
  return self.request.message;
}

- (NSArray<DialogButtonConfiguration*>*)buttonConfigs {
  DialogButtonConfiguration* OKButtonConfig =
      [DialogButtonConfiguration configWithText:l10n_util::GetNSString(IDS_OK)
                                          style:DialogButtonStyle::DEFAULT];
  self.OKButtonID = OKButtonConfig.identifier;
  DialogButtonConfiguration* cancelButtonConfig = [DialogButtonConfiguration
      configWithText:l10n_util::GetNSString(IDS_CANCEL)
               style:DialogButtonStyle::CANCEL];
  self.cancelButtonID = cancelButtonConfig.identifier;
  return @[ OKButtonConfig, cancelButtonConfig ];
}

- (NSArray<DialogTextFieldConfiguration*>*)textFieldConfigs {
  NSString* defaultUsername = self.request.defaultUserNameText;
  NSString* usernamePlaceholder =
      l10n_util::GetNSString(IDS_IOS_HTTP_LOGIN_DIALOG_USERNAME_PLACEHOLDER);
  DialogTextFieldConfiguration* usernameConfig =
      [DialogTextFieldConfiguration configWithDefaultText:defaultUsername
                                          placeholderText:usernamePlaceholder
                                                   secure:NO];
  self.usernameTextFieldID = usernameConfig.identifier;
  NSString* passwordPlaceholder =
      l10n_util::GetNSString(IDS_IOS_HTTP_LOGIN_DIALOG_PASSWORD_PLACEHOLDER);
  DialogTextFieldConfiguration* passwordConfig =
      [DialogTextFieldConfiguration configWithDefaultText:nil
                                          placeholderText:passwordPlaceholder
                                                   secure:YES];
  self.passwordTextFieldID = passwordConfig.identifier;
  return @[ usernameConfig, passwordConfig ];
}

@end
