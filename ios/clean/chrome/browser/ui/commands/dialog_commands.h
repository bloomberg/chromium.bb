// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CLEAN_CHROME_BROWSER_UI_COMMANDS_DIALOG_COMMANDS_H_
#define IOS_CLEAN_CHROME_BROWSER_UI_COMMANDS_DIALOG_COMMANDS_H_

#import <Foundation/Foundation.h>

@class DialogConfigurationIdentifier;

// Convenience typedef to improve formatting.
using DialogTextFieldValues =
    NSDictionary<DialogConfigurationIdentifier*, NSString*>;

// Command protocol for dismissing DialogConsumers.
@protocol DialogDismissalCommands

// Called to dismiss the dialog.  |buttonID| is the identifier of the
// DialogButtonConfiguration corresponding with the dialog button that was
// tapped, if any.  |textFieldValues| contains the user input text.  The keys
// are the identifiers of the DialogTextFieldConfigurations passed to the
// consumer, and the values are the text in their corresponding text fields.
- (void)
dismissDialogWithButtonID:(nonnull DialogConfigurationIdentifier*)buttonID
          textFieldValues:(nonnull DialogTextFieldValues*)textFieldValues;

@end

#endif  // IOS_CLEAN_CHROME_BROWSER_UI_COMMANDS_DIALOG_COMMANDS_H_
