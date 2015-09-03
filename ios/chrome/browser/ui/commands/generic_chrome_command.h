// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_COMMANDS_GENERIC_CHROME_COMMAND_H_
#define IOS_CHROME_BROWSER_UI_COMMANDS_GENERIC_CHROME_COMMAND_H_

#import <Foundation/Foundation.h>

// Generic command that can be passed to |chromeExecuteCommand|.
@interface GenericChromeCommand : NSObject

// Command tag.
@property(nonatomic, assign) NSInteger tag;

// Returns an autoreleased GenericChromeCommand with given |tag|.
+ (instancetype)commandWithTag:(NSInteger)tag;

// Initializes the GenericChromeCommand with given |tag|.
- (instancetype)initWithTag:(NSInteger)tag NS_DESIGNATED_INITIALIZER;

// Mark inherited initializer as unavailable to prevent calling it by mistake.
- (instancetype)init NS_UNAVAILABLE;

// Convenience method to execute this command on the main window.
- (void)executeOnMainWindow;

@end

#endif  // IOS_CHROME_BROWSER_UI_COMMANDS_GENERIC_CHROME_COMMAND_H_
