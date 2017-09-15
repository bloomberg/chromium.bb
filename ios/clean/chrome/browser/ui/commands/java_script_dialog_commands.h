// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CLEAN_CHROME_BROWSER_UI_COMMANDS_JAVA_SCRIPT_DIALOG_COMMANDS_H_
#define IOS_CLEAN_CHROME_BROWSER_UI_COMMANDS_JAVA_SCRIPT_DIALOG_COMMANDS_H_

// Command protocol for dismissing JavaScript dialogs.
@protocol JavaScriptDialogDismissalCommands<NSObject>

// Dismiss the current JavaScript dialog.
- (void)dismissJavaScriptDialog;

// Dismiss the current JavaScript dialog and show the blocking confirmation UI.
- (void)dismissJavaScriptDialogWithBlockingConfirmation;

@end

#endif  // IOS_CLEAN_CHROME_BROWSER_UI_COMMANDS_JAVA_SCRIPT_DIALOG_COMMANDS_H_
