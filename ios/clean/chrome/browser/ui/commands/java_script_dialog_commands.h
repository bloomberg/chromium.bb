// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CLEAN_CHROME_BROWSER_UI_COMMANDS_JAVA_SCRIPT_DIALOG_COMMANDS_H_
#define IOS_CLEAN_CHROME_BROWSER_UI_COMMANDS_JAVA_SCRIPT_DIALOG_COMMANDS_H_

// Command protocol for dismissing JavaScript dialogs.
@protocol JavaScriptDialogDismissalCommands<NSObject>

// Called after the user interaction with the JavaScript dialog has been handled
// and the dialog can be dismissed.
- (void)dismissJavaScriptDialog;

@end

#endif  // IOS_CLEAN_CHROME_BROWSER_UI_COMMANDS_JAVA_SCRIPT_DIALOG_COMMANDS_H_
