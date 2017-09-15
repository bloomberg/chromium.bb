// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CLEAN_CHROME_BROWSER_UI_COMMANDS_UNAVAILABLE_FEATURE_DIALOG_COMMANDS_H_
#define IOS_CLEAN_CHROME_BROWSER_UI_COMMANDS_UNAVAILABLE_FEATURE_DIALOG_COMMANDS_H_

// Command protocol for dismissing Unavailable Feature dialogs.
@protocol UnavailableFeatureDialogDismissalCommands<NSObject>

// Called when the user has dismissed the alert.
- (void)dismissUnavailableFeatureDialog;

@end

#endif  // IOS_CLEAN_CHROME_BROWSER_UI_COMMANDS_UNAVAILABLE_FEATURE_DIALOG_COMMANDS_H_
