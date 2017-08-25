// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CLEAN_CHROME_BROWSER_UI_DIALOGS_TEST_HELPERS_TEST_DIALOG_VIEW_CONTROLLER_H_
#define IOS_CLEAN_CHROME_BROWSER_UI_DIALOGS_TEST_HELPERS_TEST_DIALOG_VIEW_CONTROLLER_H_

#import "ios/clean/chrome/browser/ui/dialogs/dialog_view_controller.h"

// A test version of DialogViewController that doesn't dispatch commands.
@interface TestDialogViewController : DialogViewController

// A DialogViewController with |style|.
- (instancetype)initWithStyle:(UIAlertControllerStyle)style;

@end

#endif  // IOS_CLEAN_CHROME_BROWSER_UI_DIALOGS_TEST_HELPERS_TEST_DIALOG_VIEW_CONTROLLER_H_
