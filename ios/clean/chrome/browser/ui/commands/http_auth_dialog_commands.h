// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CLEAN_CHROME_BROWSER_UI_COMMANDS_HTTP_AUTH_DIALOG_COMMANDS_H_
#define IOS_CLEAN_CHROME_BROWSER_UI_COMMANDS_HTTP_AUTH_DIALOG_COMMANDS_H_

// Commands used to dismiss HTTP authentication dialogs.
@protocol HTTPAuthDialogDismissalCommands<NSObject>

// Called when the HTTP authentication flow has been completed to stop the
// dialog's coordinator.
- (void)dismissHTTPAuthDialog;

@end

#endif  // IOS_CLEAN_CHROME_BROWSER_UI_COMMANDS_HTTP_AUTH_DIALOG_COMMANDS_H_
