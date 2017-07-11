// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_COMMANDS_APPLICATION_COMMANDS_H_
#define IOS_CHROME_BROWSER_UI_COMMANDS_APPLICATION_COMMANDS_H_

#import <Foundation/Foundation.h>

// Protocol for commands that will generally be handled by the application,
// rather than a specific tab; in practice this means the MainController
// instance.

@protocol ApplicationCommands<NSObject>

// Shows the Settings UI.
- (void)showSettings;

@end

#endif  // IOS_CHROME_BROWSER_UI_COMMANDS_APPLICATION_COMMANDS_H_
