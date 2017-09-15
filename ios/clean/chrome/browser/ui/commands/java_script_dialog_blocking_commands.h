// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CLEAN_CHROME_BROWSER_UI_COMMANDS_JAVA_SCRIPT_DIALOG_BLOCKING_COMMANDS_H_
#define IOS_CLEAN_CHROME_BROWSER_UI_COMMANDS_JAVA_SCRIPT_DIALOG_BLOCKING_COMMANDS_H_

// Command protocol for dismissing JavaScript dialog blocking confirmation UI.
@protocol JavaScriptDialogBlockingDismissalCommands<NSObject>

// Called to dismiss the JavaScript dialog blocking confirmation UI.
// |confirmation| indicates whether the dialog blocking action was confirmed.
- (void)dismissJavaScriptDialogBlockingConfirmation;

@end

#endif  // IOS_CLEAN_CHROME_BROWSER_UI_COMMANDS_JAVA_SCRIPT_DIALOG_BLOCKING_COMMANDS_H_
