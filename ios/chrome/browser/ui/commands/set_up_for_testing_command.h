// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_COMMANDS_SET_UP_FOR_TESTING_COMMAND_H_
#define IOS_CHROME_BROWSER_UI_COMMANDS_SET_UP_FOR_TESTING_COMMAND_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/ui/commands/generic_chrome_command.h"

class GURL;

// Set up for testing command that can be passed to |chromeExecuteCommand|.
@interface SetUpForTestingCommand : GenericChromeCommand

// Mark inherited initializer as unavailable to prevent calling it by mistake.
- (instancetype)initWithTag:(NSInteger)tag NS_UNAVAILABLE;

// Initializes this command by parsing the url query.
- (instancetype)initWithURL:(const GURL&)url;

// Initializes this command.
- (instancetype)initWithClearBrowsingData:(BOOL)clearBrowsingData
                                closeTabs:(BOOL)closeTabs
                          numberOfNewTabs:(NSInteger)numberOfNewTabs
    NS_DESIGNATED_INITIALIZER;

// Whether the browsing data should be cleared.
@property(nonatomic, readonly) BOOL clearBrowsingData;

// Whether the existing tabs should be closed.
@property(nonatomic, readonly) BOOL closeTabs;

// The number of new tabs to create.
@property(nonatomic, readonly) NSInteger numberOfNewTabs;

@end

#endif  // IOS_CHROME_BROWSER_UI_COMMANDS_SET_UP_FOR_TESTING_COMMAND_H_
