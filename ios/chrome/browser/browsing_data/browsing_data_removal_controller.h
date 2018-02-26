// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BROWSING_DATA_BROWSING_DATA_REMOVAL_CONTROLLER_H_
#define IOS_CHROME_BROWSER_BROWSING_DATA_BROWSING_DATA_REMOVAL_CONTROLLER_H_

#import <Foundation/Foundation.h>

#import "base/ios/block_types.h"
#include "components/browsing_data/core/browsing_data_utils.h"
#include "ios/chrome/browser/browsing_data/browsing_data_remove_mask.h"

namespace ios {
class ChromeBrowserState;
}  // namespace ios

// This class is responsible for removing browsing data associated with a
// ChromeBrowserState.
@interface BrowsingDataRemovalController : NSObject

// Removes browsing data from |browserState| for datatypes in |mask|. |mask| is
// obtained from BrowsingDataRemoveMask. |browserState| cannot be null and must
// not be off the record.
- (void)removeBrowsingDataFromBrowserState:
            (ios::ChromeBrowserState*)browserState
                                      mask:(BrowsingDataRemoveMask)mask
                                timePeriod:(browsing_data::TimePeriod)timePeriod
                         completionHandler:(ProceduralBlock)completionHandler;

// Returns YES if browsing data for |browserState| is still being cleared.
- (BOOL)hasPendingRemovalOperations:(ios::ChromeBrowserState*)browserState;

@end

#endif  // IOS_CHROME_BROWSER_BROWSING_DATA_BROWSING_DATA_REMOVAL_CONTROLLER_H_
