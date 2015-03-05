// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_COMMANDS_CLEAR_BROWSING_DATA_COMMAND_H_
#define IOS_CHROME_BROWSER_UI_COMMANDS_CLEAR_BROWSING_DATA_COMMAND_H_

#import <Foundation/Foundation.h>

namespace ios {
class ChromeBrowserState;
}

// Command sent to clear the browsing data associated with a browser state.
@interface ClearBrowsingDataCommand : NSObject

- (instancetype)initWithBrowserState:(ios::ChromeBrowserState*)browserState
                                mask:(int)mask;

@property(nonatomic, readonly, assign) ios::ChromeBrowserState* browserState;
@property(nonatomic, readonly, assign) int mask;

@end

#endif  // IOS_CHROME_BROWSER_UI_COMMANDS_CLEAR_BROWSING_DATA_COMMAND_H_
