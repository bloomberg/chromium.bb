// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CLEAN_CHROME_BROWSER_UI_DIALOGS_DIALOG_MEDIATOR_SUBCLASSING_H_
#define IOS_CLEAN_CHROME_BROWSER_UI_DIALOGS_DIALOG_MEDIATOR_SUBCLASSING_H_

#import "ios/clean/chrome/browser/ui/dialogs/dialog_mediator.h"

#import "ios/clean/chrome/browser/ui/commands/dialog_commands.h"

@class DialogButtonConfiguration;
@class DialogTextFieldConfiguration;

// DialogMediator functionality exposed to subclasses.
@interface DialogMediator (DialogMediatorSubclassing)

// The title to provide to the consumer.
- (NSString*)dialogTitle;

// The message to provide to the consumer.
- (NSString*)dialogMessage;

// The DialogButtonConfigurations to provide to the consumer.
- (NSArray<DialogButtonConfiguration*>*)buttonConfigs;

// The DialogTextFieldConfigurations to provide to the consumer.
- (NSArray<DialogTextFieldConfiguration*>*)textFieldConfigs;

@end

#endif  // IOS_CLEAN_CHROME_BROWSER_UI_DIALOGS_DIALOG_MEDIATOR_SUBCLASSING_H_
