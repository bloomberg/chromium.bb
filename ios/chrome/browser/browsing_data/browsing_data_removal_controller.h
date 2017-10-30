// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BROWSING_DATA_BROWSING_DATA_REMOVAL_CONTROLLER_H_
#define IOS_CHROME_BROWSER_BROWSING_DATA_BROWSING_DATA_REMOVAL_CONTROLLER_H_

#import <Foundation/Foundation.h>

#import "base/ios/block_types.h"
#include "components/browsing_data/core/browsing_data_utils.h"

@class BrowserViewController;

namespace ios {
class ChromeBrowserState;
}  // namespace ios

// This class is responsible for removing browsing data associated with a
// ChromeBrowserState.
@interface BrowsingDataRemovalController : NSObject

// Designated initializer,|browserState| cannot be null and must not be off the
// record.
- (instancetype)initWithBrowserState:(ios::ChromeBrowserState*)browserState
    NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

// Removes browsing data from browserState_ for datatypes in |mask|.
// |mask| is obtained from IOSChromeBrowsingDataRemover::RemoveDataMask.
- (void)removeBrowsingData:(int)mask
                timePeriod:(browsing_data::TimePeriod)timePeriod
         completionHandler:(ProceduralBlock)completionHandler;

// Removes browsing data that iOS has associated with browserState_ and which
// is not removed when the browserState_ is destroyed.
// |mask| is obtained from IOSChromeBrowsingDataRemover::RemoveDataMask
// |completionHandler| is called when this operation finishes. This method
// finishes removal of the browsing data even if browserState_ is destroyed
// after this method call.
- (void)removeIOSSpecificIncognitoBrowsingData:(int)mask
                             completionHandler:
                                 (ProceduralBlock)completionHandler;

// Sets browserState_ TabModels web usage to |enabled|.
- (void)setWebUsageEnabled:(BOOL)enabled;

// Called if |browserState| is destroyed. If |browserState| has any
// pendingRemovalOperations they will be cleared.
- (void)browserStateDestroyed:(ios::ChromeBrowserState*)browserState;

// Returns YES if browsing data for browserState_ is still being cleared.
- (BOOL)hasPendingRemovalOperations;

// Sets browserState_ to nullptr.
- (void)shutdown;

@end

#endif  // IOS_CHROME_BROWSER_BROWSING_DATA_BROWSING_DATA_REMOVAL_CONTROLLER_H_
