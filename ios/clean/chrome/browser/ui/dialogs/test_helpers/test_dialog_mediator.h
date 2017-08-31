// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CLEAN_CHROME_BROWSER_UI_DIALOGS_TEST_HELPERS_TEST_DIALOG_MEDIATOR_H_
#define IOS_CLEAN_CHROME_BROWSER_UI_DIALOGS_TEST_HELPERS_TEST_DIALOG_MEDIATOR_H_

#import "ios/clean/chrome/browser/ui/dialogs/dialog_mediator.h"

@class DialogButtonConfiguration;
@class DialogTextFieldConfiguration;

// A DialogMediator subclass that can be configured via readwrite properties.
@interface TestDialogMediator : DialogMediator
@property(nonatomic, copy) NSString* dialogTitle;
@property(nonatomic, copy) NSString* dialogMessage;
@property(nonatomic, copy) NSArray<DialogButtonConfiguration*>* buttonConfigs;
@property(nonatomic, copy)
    NSArray<DialogTextFieldConfiguration*>* textFieldConfigs;
@end

#endif  // IOS_CLEAN_CHROME_BROWSER_UI_DIALOGS_TEST_HELPERS_TEST_DIALOG_MEDIATOR_H_
