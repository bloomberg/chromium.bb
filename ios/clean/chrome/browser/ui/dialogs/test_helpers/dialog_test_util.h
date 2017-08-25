// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CLEAN_CHROME_BROWSER_UI_DIALOGS_TEST_HELPERS_DIALOG_TEST_UTIL_H_
#define IOS_CLEAN_CHROME_BROWSER_UI_DIALOGS_TEST_HELPERS_DIALOG_TEST_UTIL_H_

#import <Foundation/Foundation.h>

@class DialogButtonConfiguration;
@class DialogMediator;
@class DialogTextFieldConfiguration;
@class DialogViewController;

namespace dialogs_test_util {

// Tests that |view_controller| is set up appropriately for the provided
// DialogConsumer configuration parameters.
void TestAlertSetup(DialogViewController* view_controller,
                    NSString* title,
                    NSString* message,
                    NSArray<DialogButtonConfiguration*>* buttons,
                    NSArray<DialogTextFieldConfiguration*>* fields);

// Tests that |view_controller| is set up appropriately for |mediator|.
void TestAlertSetup(DialogViewController* view_controller,
                    DialogMediator* mediator);

}  // namespace dialogs_test_util

#endif  // IOS_CLEAN_CHROME_BROWSER_UI_DIALOGS_TEST_HELPERS_DIALOG_TEST_UTIL_H_
