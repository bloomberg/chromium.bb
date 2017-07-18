// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_COMMANDS_OPEN_NEW_TAB_COMMAND_H_
#define IOS_CHROME_BROWSER_UI_COMMANDS_OPEN_NEW_TAB_COMMAND_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/commands/generic_chrome_command.h"

// Command sent to open a new tab, optionally including a point (in UIWindow
// coordinates).
@interface OpenNewTabCommand : GenericChromeCommand

- (instancetype)initWithIncognito:(BOOL)incognito
                      originPoint:(CGPoint)originPoint
    NS_DESIGNATED_INITIALIZER;

// Mark inherited initializer as unavailable to prevent calling it by mistake.
- (instancetype)initWithTag:(NSInteger)tag NS_UNAVAILABLE;

// Convenience initializers

// Creates an OpenTabCommand with |incognito| and an |originPoint| of
// CGPointZero.
+ (instancetype)commandWithIncognito:(BOOL)incognito;

// Creates an OpenTabCommand with |incognito| NO and an |originPoint| of
// CGPointZero.
+ (instancetype)command;

// Creates an OpenTabCommand with |incognito| YES and an |originPoint| of
// CGPointZero.
+ (instancetype)incognitoTabCommand;

@property(nonatomic, readonly) BOOL incognito;

@property(nonatomic, readonly) CGPoint originPoint;

@end

#endif  // IOS_CHROME_BROWSER_UI_COMMANDS_OPEN_NEW_TAB_COMMAND_H_
