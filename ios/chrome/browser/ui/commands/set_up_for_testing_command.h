// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_COMMANDS_SET_UP_FOR_TESTING_COMMAND_H_
#define IOS_CHROME_BROWSER_UI_COMMANDS_SET_UP_FOR_TESTING_COMMAND_H_

#import <Foundation/Foundation.h>

class GURL;

// Set up for testing command that can be passed to |chromeExecuteCommand|.
@interface SetUpForTestingCommand : NSObject

// Initializes this command by parsing the url query.
- (instancetype)initWithURL:(const GURL&)url;

// Initializes this command.
- (instancetype)initWithClearBrowsingData:(BOOL)clearBrowsingData
                                closeTabs:(BOOL)closeTabs
                          numberOfNewTabs:(NSInteger)numberOfNewTabs;

@property(nonatomic, readonly, assign) BOOL clearBrowsingData;
@property(nonatomic, readonly, assign) BOOL closeTabs;
@property(nonatomic, assign) NSInteger numberOfNewTabs;

@end

#endif  // IOS_CHROME_BROWSER_UI_COMMANDS_SET_UP_FOR_TESTING_COMMAND_H_
