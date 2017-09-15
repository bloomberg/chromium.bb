// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/clean/chrome/browser/ui/dialogs/java_script_dialogs/java_script_dialog_mediator.h"

#include "base/logging.h"
#include "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/ui/commands/command_dispatcher.h"
#import "ios/chrome/browser/ui/dialogs/java_script_dialog_blocking_state.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/clean/chrome/browser/ui/commands/java_script_dialog_commands.h"
#import "ios/clean/chrome/browser/ui/dialogs/dialog_button_configuration.h"
#import "ios/clean/chrome/browser/ui/dialogs/dialog_button_style.h"
#import "ios/clean/chrome/browser/ui/dialogs/dialog_mediator+subclassing.h"
#import "ios/clean/chrome/browser/ui/dialogs/dialog_text_field_configuration.h"
#import "ios/clean/chrome/browser/ui/dialogs/java_script_dialogs/java_script_dialog_request.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface JavaScriptDialogMediator ()<DialogDismissalCommands>

// The dismissal dispatcher.
@property(nonatomic, readonly, strong) id<JavaScriptDialogDismissalCommands>
    dismissalDispatcher;
// The request passed on initializaton.
@property(nonatomic, readonly, strong) JavaScriptDialogRequest* request;
// The configuration identifiers.
@property(nonatomic, weak) DialogConfigurationIdentifier* OKButtonID;
@property(nonatomic, weak) DialogConfigurationIdentifier* cancelButtonID;
@property(nonatomic, weak) DialogConfigurationIdentifier* blockButtonID;
@property(nonatomic, weak) DialogConfigurationIdentifier* promptTextFieldID;

// Called when buttons are tapped to dispatch JavaScriptDialogDismissalCommands.
- (void)OKButtonWasTappedWithTextFieldValues:
    (NSDictionary<id, NSString*>*)textFieldValues;
- (void)cancelButtonWasTapped;

@end

@implementation JavaScriptDialogMediator
@synthesize dismissalDispatcher = _dismissalDispatcher;
@synthesize request = _request;
@synthesize OKButtonID = _OKButtonID;
@synthesize cancelButtonID = _cancelButtonID;
@synthesize blockButtonID = _blockButtonID;
@synthesize promptTextFieldID = _promptTextFieldID;

- (instancetype)initWithRequest:(JavaScriptDialogRequest*)request
                     dispatcher:
                         (id<JavaScriptDialogDismissalCommands>)dispatcher {
  DCHECK(request);
  DCHECK(dispatcher);
  if ((self = [super init])) {
    _request = request;
    _dismissalDispatcher = dispatcher;
  }
  return self;
}

#pragma mark - DialogDismissalCommands

- (void)dismissDialogWithButtonID:(DialogConfigurationIdentifier*)buttonID
                  textFieldValues:(DialogTextFieldValues*)textFieldValues {
  DCHECK(buttonID);
  if (buttonID == self.OKButtonID) {
    [self OKButtonWasTappedWithTextFieldValues:textFieldValues];
  } else if (buttonID == self.cancelButtonID) {
    [self cancelButtonWasTapped];
  } else if (buttonID == self.blockButtonID) {
    [self.dismissalDispatcher dismissJavaScriptDialogWithBlockingConfirmation];
  } else {
    NOTREACHED() << "Received dialog dismissal for unknown button tag.";
  }
}

#pragma mark -

- (void)OKButtonWasTappedWithTextFieldValues:
    (NSDictionary<id, NSString*>*)textFieldValues {
  NSString* userInput = nil;
  if (self.request.type == web::JAVASCRIPT_DIALOG_TYPE_PROMPT) {
    DCHECK(self.promptTextFieldID);
    userInput = textFieldValues[self.promptTextFieldID];
    userInput = userInput ? userInput : @"";
  }
  [self.request finishRequestWithSuccess:YES userInput:userInput];
  [self.dismissalDispatcher dismissJavaScriptDialog];
}

- (void)cancelButtonWasTapped {
  DCHECK_NE(self.request.type, web::JAVASCRIPT_DIALOG_TYPE_ALERT);
  [self.request finishRequestWithSuccess:NO userInput:nil];
  [self.dismissalDispatcher dismissJavaScriptDialog];
}

@end

@implementation JavaScriptDialogMediator (DialogMediatorSubclassing)

- (NSString*)dialogTitle {
  return self.request.title;
}

- (NSString*)dialogMessage {
  return self.request.message;
}

- (NSArray<DialogButtonConfiguration*>*)buttonConfigs {
  NSMutableArray<DialogButtonConfiguration*>* configs = [NSMutableArray array];
  // All JavaScript dialogs have an OK button.
  [configs addObject:[DialogButtonConfiguration
                         configWithText:l10n_util::GetNSString(IDS_OK)
                                  style:DialogButtonStyle::DEFAULT]];
  self.OKButtonID = [configs lastObject].identifier;
  // Only confirmations and prompts get cancel buttons.
  if (self.request.type != web::JAVASCRIPT_DIALOG_TYPE_ALERT) {
    NSString* cancelText = l10n_util::GetNSString(IDS_CANCEL);
    [configs addObject:[DialogButtonConfiguration
                           configWithText:cancelText
                                    style:DialogButtonStyle::CANCEL]];
    self.cancelButtonID = [configs lastObject].identifier;
  }
  // Add the blocking option if indicated to by the WebState's blocking state.
  JavaScriptDialogBlockingState* blockingState =
      JavaScriptDialogBlockingState::FromWebState(self.request.webState);
  if (blockingState && blockingState->show_blocking_option()) {
    NSString* blockingText =
        l10n_util::GetNSString(IDS_IOS_JAVA_SCRIPT_DIALOG_BLOCKING_BUTTON_TEXT);
    [configs addObject:[DialogButtonConfiguration
                           configWithText:blockingText
                                    style:DialogButtonStyle::DEFAULT]];
    self.blockButtonID = [configs lastObject].identifier;
  }
  return [configs copy];
}

- (NSArray<DialogTextFieldConfiguration*>*)textFieldConfigs {
  // Only prompts have text fields.
  if (self.request.type != web::JAVASCRIPT_DIALOG_TYPE_PROMPT)
    return nil;
  DialogTextFieldConfiguration* config = [DialogTextFieldConfiguration
      configWithDefaultText:self.request.defaultPromptText
            placeholderText:nil
                     secure:NO];
  self.promptTextFieldID = config.identifier;
  return @[ config ];
}

@end
