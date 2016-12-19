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

// A protocol required by delegates of the BrowsingDataRemovalController.
@protocol BrowsingDataRemovalControllerDelegate
@required
// Removes files received from other applications by |browserState|.
// |completionHandler| is called when the files have been removed.
- (void)removeExternalFilesForBrowserState:
            (ios::ChromeBrowserState*)browserState
                         completionHandler:(ProceduralBlock)completionHandler;
// TODO(crbug.com/543213): Add a method here to inform the delegate when there
// are no pending removal operations associated with a BrowserState.
@end

// This class is responsible for removing browsing data associated with a
// ChromeBrowserState.
@interface BrowsingDataRemovalController : NSObject

- (instancetype)initWithDelegate:
    (id<BrowsingDataRemovalControllerDelegate>)delegate
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Removes browsing data from |browserState| for datatypes in |mask|.
// |mask| is obtained from IOSChromeBrowsingDataRemover::RemoveDataMask.
// |browserState| cannot be null and must not be off the record.
- (void)removeBrowsingDataFromBrowserState:
            (ios::ChromeBrowserState*)browserState
                                      mask:(int)mask
                                timePeriod:(browsing_data::TimePeriod)timePeriod
                         completionHandler:(ProceduralBlock)completionHandler;

// Removes browsing data that iOS has associated with |browserState| and which
// is not removed when the |browserState| is destroyed.
// |mask| is obtained from IOSChromeBrowsingDataRemover::RemoveDataMask
// |browserState| cannot be  null. |completionHandler| is called when this
// operation finishes. This method finishes removal of the browsing data even if
// |browserState| is destroyed after this method call.
- (void)removeIOSSpecificIncognitoBrowsingDataFromBrowserState:
            (ios::ChromeBrowserState*)browserState
                                                          mask:(int)mask
                                             completionHandler:
                                                 (ProceduralBlock)
                                                     completionHandler;

// Called when |browserState| is destroyed.
- (void)browserStateDestroyed:(ios::ChromeBrowserState*)browserState;

// Returns YES if browsing data for |browserState| is still being cleared.
- (BOOL)hasPendingRemovalOperations:(ios::ChromeBrowserState*)browserState;

@end

#endif  // IOS_CHROME_BROWSER_BROWSING_DATA_BROWSING_DATA_REMOVAL_CONTROLLER_H_
