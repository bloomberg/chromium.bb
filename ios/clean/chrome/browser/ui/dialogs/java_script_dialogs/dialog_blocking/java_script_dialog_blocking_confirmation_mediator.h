// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CLEAN_CHROME_BROWSER_UI_DIALOGS_JAVA_SCRIPT_DIALOGS_DIALOG_BLOCKING_JAVA_SCRIPT_DIALOG_BLOCKING_CONFIRMATION_MEDIATOR_H_
#define IOS_CLEAN_CHROME_BROWSER_UI_DIALOGS_JAVA_SCRIPT_DIALOGS_DIALOG_BLOCKING_JAVA_SCRIPT_DIALOG_BLOCKING_CONFIRMATION_MEDIATOR_H_

#import <Foundation/Foundation.h>

#import "ios/clean/chrome/browser/ui/dialogs/dialog_mediator.h"

@protocol JavaScriptDialogBlockingDismissalCommands;
@class JavaScriptDialogRequest;

// Class responsible for setting up a DialogConsumer for a dialog blocking
// confirmation UI.
@interface JavaScriptDialogBlockingConfirmationMediator : DialogMediator

// Designated initializer for a mediator that uses |request| to configure
// consumers and |dispatcher| for dismissal.  Both arguments are expected to be
// non-nil.
- (instancetype)initWithRequest:(JavaScriptDialogRequest*)request
                     dispatcher:(id<JavaScriptDialogBlockingDismissalCommands>)
                                    dispatcher NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CLEAN_CHROME_BROWSER_UI_DIALOGS_JAVA_SCRIPT_DIALOGS_DIALOG_BLOCKING_JAVA_SCRIPT_DIALOG_BLOCKING_CONFIRMATION_MEDIATOR_H_
