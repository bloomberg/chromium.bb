// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_COMMANDS_CLEAR_BROWSING_DATA_COMMAND_H_
#define IOS_CHROME_BROWSER_UI_COMMANDS_CLEAR_BROWSING_DATA_COMMAND_H_

#import <Foundation/Foundation.h>

#import "base/ios/block_types.h"
#import "components/browsing_data/core/browsing_data_utils.h"
#include "ios/chrome/browser/browsing_data/browsing_data_remove_mask.h"
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
// |completionBlock| will be invoked when the data has been cleared.
- (instancetype)initWithBrowserState:(ios::ChromeBrowserState*)browserState
                                mask:(BrowsingDataRemoveMask)mask
                          timePeriod:(browsing_data::TimePeriod)timePeriod
                     completionBlock:(ProceduralBlock)completionBlock
    NS_DESIGNATED_INITIALIZER;

// When executed this command will remove browsing data for this BrowserState.
@property(nonatomic, readonly) ios::ChromeBrowserState* browserState;

// Removal mask: see BrowsingDataRemover::RemoveDataMask.
@property(nonatomic, readonly) BrowsingDataRemoveMask mask;

// Time period for which the browsing data will be removed.
@property(nonatomic, readonly) browsing_data::TimePeriod timePeriod;

// Completion block invoked when the data has been cleared.
@property(nonatomic, readonly) ProceduralBlock completionBlock;

@end

#endif  // IOS_CHROME_BROWSER_UI_COMMANDS_CLEAR_BROWSING_DATA_COMMAND_H_
