// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CLEAN_CHROME_BROWSER_UI_DIALOGS_HTTP_AUTH_DIALOGS_HTTP_AUTH_DIALOG_MEDIATOR_H_
#define IOS_CLEAN_CHROME_BROWSER_UI_DIALOGS_HTTP_AUTH_DIALOGS_HTTP_AUTH_DIALOG_MEDIATOR_H_

#import "ios/clean/chrome/browser/ui/dialogs/dialog_mediator.h"

@protocol HTTPAuthDialogDismissalCommands;
@class HTTPAuthDialogRequest;

// Class responsible for setting up a DialogConsumer for HTTP auth dialogs.
@interface HTTPAuthDialogMediator : DialogMediator

// Designated initializer for a mediator that uses |request| to provide data to
// consumers, and |dispatcher| to communicate HTTP auth dismissal commands.
- (nullable instancetype)initWithRequest:(nonnull HTTPAuthDialogRequest*)request
    NS_DESIGNATED_INITIALIZER;
- (nullable instancetype)init NS_UNAVAILABLE;

// The dispatcher to use for dismissal.
@property(nonatomic, weak, nullable) id<HTTPAuthDialogDismissalCommands>
    dispatcher;

@end

#endif  // IOS_CLEAN_CHROME_BROWSER_UI_DIALOGS_HTTP_AUTH_DIALOGS_HTTP_AUTH_DIALOG_MEDIATOR_H_
