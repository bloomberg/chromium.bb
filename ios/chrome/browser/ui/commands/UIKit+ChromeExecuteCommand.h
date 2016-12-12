// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_COMMANDS_UIKIT_CHROMEEXECUTECOMMAND_H_
#define IOS_CHROME_BROWSER_UI_COMMANDS_UIKIT_CHROMEEXECUTECOMMAND_H_

#import <UIKit/UIKit.h>

@interface UIResponder (ChromeExecuteCommand)
// Executes a Chrome command.  |sender| must implement the |-tag| method and
// return the id of the command to execute.  The default implementation of this
// method simply forwards the call to the next responder.
- (IBAction)chromeExecuteCommand:(id)sender;
@end

@interface UIWindow (ChromeExecuteCommand)
// UIResponder addition to execute a Chrome command.  Overridden in UIWindow to
// forward the call to the application's delegate.
- (void)chromeExecuteCommand:(id)sender;
@end

#endif  // IOS_CHROME_BROWSER_UI_COMMANDS_UIKIT_CHROMEEXECUTECOMMAND_H_
