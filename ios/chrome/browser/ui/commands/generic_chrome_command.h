// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_COMMANDS_GENERIC_CHROME_COMMAND_H_
#define IOS_CHROME_BROWSER_UI_COMMANDS_GENERIC_CHROME_COMMAND_H_

#import <Foundation/Foundation.h>

// Generic command that can be passed to |chromeExecuteCommand|.
@interface GenericChromeCommand : NSObject

@property(nonatomic, assign) NSInteger tag;

// Designated initializer.
- (instancetype)initWithTag:(NSInteger)tag;

// Convenience method to execute this command on the main window.
- (void)executeOnMainWindow;

@end

#endif  // IOS_CHROME_BROWSER_UI_COMMANDS_GENERIC_CHROME_COMMAND_H_
