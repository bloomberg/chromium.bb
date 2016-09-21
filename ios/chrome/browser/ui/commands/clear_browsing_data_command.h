// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_COMMANDS_CLEAR_BROWSING_DATA_COMMAND_H_
#define IOS_CHROME_BROWSER_UI_COMMANDS_CLEAR_BROWSING_DATA_COMMAND_H_

#import <Foundation/Foundation.h>

#import "components/browsing_data/core/browsing_data_utils.h"
#import "ios/chrome/browser/ui/commands/generic_chrome_command.h"

namespace ios {
class ChromeBrowserState;
}

// Command sent to clear the browsing data associated with a browser state.
@interface ClearBrowsingDataCommand : GenericChromeCommand

// Mark inherited initializer as unavailable to prevent calling it by mistake.
- (instancetype)initWithTag:(NSInteger)tag NS_UNAVAILABLE;

// Initializes a command intented to clear browsing data for |browserState|
// that corresponds to removal mask |mask| for the time period |timePeriod|.
- (instancetype)initWithBrowserState:(ios::ChromeBrowserState*)browserState
                                mask:(int)mask
                          timePeriod:(browsing_data::TimePeriod)timePeriod
    NS_DESIGNATED_INITIALIZER;

// When executed this command will remove browsing data for this BrowserState.
@property(nonatomic, readonly) ios::ChromeBrowserState* browserState;

// Removal mask: see BrowsingDataRemover::RemoveDataMask.
@property(nonatomic, readonly) int mask;

// Time period for which the browsing data will be removed.
@property(nonatomic, readonly) browsing_data::TimePeriod timePeriod;

@end

#endif  // IOS_CHROME_BROWSER_UI_COMMANDS_CLEAR_BROWSING_DATA_COMMAND_H_
