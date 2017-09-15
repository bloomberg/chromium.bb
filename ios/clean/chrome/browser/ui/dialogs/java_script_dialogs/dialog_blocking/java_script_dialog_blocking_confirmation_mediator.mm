// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/clean/chrome/browser/ui/dialogs/java_script_dialogs/dialog_blocking/java_script_dialog_blocking_confirmation_mediator.h"

#include "base/logging.h"
#include "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/ui/commands/command_dispatcher.h"
#import "ios/chrome/browser/ui/dialogs/java_script_dialog_blocking_state.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/clean/chrome/browser/ui/commands/dialog_commands.h"
#import "ios/clean/chrome/browser/ui/commands/java_script_dialog_blocking_commands.h"
#import "ios/clean/chrome/browser/ui/dialogs/dialog_button_configuration.h"
#import "ios/clean/chrome/browser/ui/dialogs/dialog_button_style.h"
#import "ios/clean/chrome/browser/ui/dialogs/dialog_consumer.h"
#import "ios/clean/chrome/browser/ui/dialogs/dialog_mediator+subclassing.h"
#import "ios/clean/chrome/browser/ui/dialogs/java_script_dialogs/java_script_dialog_request.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface JavaScriptDialogBlockingConfirmationMediator ()

// The dismissal dispatcher.
@property(nonatomic, readonly, weak)
    id<JavaScriptDialogBlockingDismissalCommands>
        dismissalDispatcher;
// The request passed on initializaton.
@property(nonatomic, readonly, strong) JavaScriptDialogRequest* request;
// The configuration identifiers.
@property(nonatomic, weak) DialogConfigurationIdentifier* confirmButtonID;
@property(nonatomic, weak) DialogConfigurationIdentifier* cancelButtonID;

@end

@implementation JavaScriptDialogBlockingConfirmationMediator
@synthesize dismissalDispatcher = _dismissalDispatcher;
@synthesize request = _request;
@synthesize confirmButtonID = _confirmButtonID;
@synthesize cancelButtonID = _canelButtonID;

- (instancetype)initWithRequest:(JavaScriptDialogRequest*)request
                     dispatcher:(id<JavaScriptDialogBlockingDismissalCommands>)
                                    dispatcher {
  DCHECK(request.webState);
  DCHECK(dispatcher);
  if (self = [super init]) {
    _request = request;
    _dismissalDispatcher = dispatcher;
  }
  return self;
}

#pragma mark - DialogDismissalCommands

- (void)dismissDialogWithButtonID:(DialogConfigurationIdentifier*)buttonID
                  textFieldValues:
                      (NSDictionary<DialogConfigurationIdentifier*, NSString*>*)
                          textFieldValues {
  DCHECK(buttonID);
  if (buttonID == self.confirmButtonID) {
    JavaScriptDialogBlockingState::FromWebState(self.request.webState)
        ->JavaScriptDialogBlockingOptionSelected();
  } else if (buttonID != self.cancelButtonID) {
    NOTREACHED() << "Received dialog dismissal for unknown button tag.";
  }
  [self.request finishRequestWithSuccess:NO userInput:nil];
  [self.dismissalDispatcher dismissJavaScriptDialogBlockingConfirmation];
}

@end

@implementation JavaScriptDialogBlockingConfirmationMediator (
    DialogMediatorSubclassing)

- (NSString*)dialogMessage {
  return l10n_util::GetNSString(IDS_JAVASCRIPT_MESSAGEBOX_SUPPRESS_OPTION);
}

- (NSArray<DialogButtonConfiguration*>*)buttonConfigs {
  NSMutableArray<DialogButtonConfiguration*>* configs =
      [[NSMutableArray alloc] init];
  // Add the confirmation button.  The style is DESTRUCTIVE because this is a
  // confirmation of a destructive action (i.e. blocking dialogs).
  NSString* confirmText =
      l10n_util::GetNSString(IDS_IOS_JAVA_SCRIPT_DIALOG_BLOCKING_BUTTON_TEXT);
  [configs addObject:[DialogButtonConfiguration
                         configWithText:confirmText
                                  style:DialogButtonStyle::DESTRUCTIVE]];
  self.confirmButtonID = [configs lastObject].identifier;
  // Add the cancel button.
  NSString* cancelText = l10n_util::GetNSString(IDS_CANCEL);
  [configs addObject:[DialogButtonConfiguration
                         configWithText:cancelText
                                  style:DialogButtonStyle::CANCEL]];
  self.cancelButtonID = [configs lastObject].identifier;
  return configs;
}

@end
