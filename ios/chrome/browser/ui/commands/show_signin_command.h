// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_COMMANDS_SHOW_SIGNIN_COMMAND_H_
#define IOS_CHROME_BROWSER_UI_COMMANDS_SHOW_SIGNIN_COMMAND_H_

#import <Foundation/Foundation.h>

#include "components/signin/core/browser/signin_metrics.h"
#include "ios/chrome/browser/ui/commands/generic_chrome_command.h"

typedef void (^ShowSigninCommandCompletionCallback)(BOOL succeeded);

enum AuthenticationOperation {
  // Operation to cancel the current authentication operation and dismiss any
  // UI presented by this operation.
  AUTHENTICATION_OPERATION_DISMISS,

  // Operation to start a re-authenticate operation. The user is presented with
  // the SSOAuth re-authenticate web page.
  AUTHENTICATION_OPERATION_REAUTHENTICATE,

  // Operation to start a sign-in operation. The user is presented with the
  // SSOAuth sign in page (SSOAuth account picker or SSOAuth sign-in web page).
  AUTHENTICATION_OPERATION_SIGNIN,

  // Operation to start a sign-in operation based on the first known identity.
  // The users are presented with the sync confirmation screen.
  AUTHENTICATION_OPERATION_SIGNIN_PROMO_CONTINUE_AS,
};

// A command to perform a sign in operation.
@interface ShowSigninCommand : GenericChromeCommand

// Mark inherited initializer as unavailable to prevent calling it by mistake.
- (instancetype)initWithTag:(NSInteger)tag NS_UNAVAILABLE;

// Initializes a command to perform the specified operation with a
// SigninInteractionController and invoke a possibly-nil callback when finished.
- (instancetype)initWithOperation:(AuthenticationOperation)operation
                      accessPoint:(signin_metrics::AccessPoint)accessPoint
                         callback:(ShowSigninCommandCompletionCallback)callback
    NS_DESIGNATED_INITIALIZER;

// Initializes a ShowSigninCommand with a nil callback.
- (instancetype)initWithOperation:(AuthenticationOperation)operation
                      accessPoint:(signin_metrics::AccessPoint)accessPoint;

// The callback to be invoked after the operation is complete.
@property(copy, nonatomic, readonly)
    ShowSigninCommandCompletionCallback callback;

// The operation to perform during the sign-in flow.
@property(nonatomic, readonly) AuthenticationOperation operation;

// The access point of this authentication operation.
@property(nonatomic, readonly) signin_metrics::AccessPoint accessPoint;

@end

#endif  // IOS_CHROME_BROWSER_UI_COMMANDS_SHOW_SIGNIN_COMMAND_H_
