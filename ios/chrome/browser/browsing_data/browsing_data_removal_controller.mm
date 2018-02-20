// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/browsing_data/browsing_data_removal_controller.h"

#import <WebKit/WebKit.h>

#include <memory>

#include "base/containers/flat_map.h"
#include "base/logging.h"
#import "base/mac/bind_objc_block.h"
#include "components/signin/ios/browser/account_consistency_service.h"
#include "ios/chrome/browser/browsing_data/browsing_data_remove_mask.h"
#include "ios/chrome/browser/browsing_data/ios_chrome_browsing_data_remover.h"
#include "ios/chrome/browser/signin/account_consistency_service_factory.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation BrowsingDataRemovalController {
  // Mapping from ChromeBrowserState to the IOSChromeBrowsingDataRemover used
  // for removal of the data on that instance.
  base::flat_map<ios::ChromeBrowserState*,
                 std::unique_ptr<IOSChromeBrowsingDataRemover>>
      _browingDataRemovers;
}

- (void)removeBrowsingDataFromBrowserState:
            (ios::ChromeBrowserState*)browserState
                                      mask:(BrowsingDataRemoveMask)mask
                                timePeriod:(browsing_data::TimePeriod)timePeriod
                         completionHandler:(ProceduralBlock)completionHandler {
  DCHECK(browserState);
  DLOG_IF(WARNING, mask == BrowsingDataRemoveMask::REMOVE_NOTHING)
      << "Nothing to remove!";

  if (!completionHandler) {
    completionHandler = ^{
    };
  }

  // The block capture |self| (via accessing the ivar _browingDataRemovers).
  // This is a workaround to ensure the callback is invoked even if the object
  // is destroyed when getting out of scope.
  ProceduralBlock browsingDataCleared = ^{
    // Check whether browserState has been destroyed (and ensure that |self|
    // survives until the block is invoked).
    if (_browingDataRemovers.find(browserState) != _browingDataRemovers.end()) {
      if (AccountConsistencyService* accountConsistencyService =
              ios::AccountConsistencyServiceFactory::GetForBrowserState(
                  browserState)) {
        accountConsistencyService->OnBrowsingDataRemoved();
      }
    }
    completionHandler();
  };

  auto iterator = _browingDataRemovers.find(browserState);
  if (iterator == _browingDataRemovers.end()) {
    iterator =
        _browingDataRemovers
            .emplace(
                browserState,
                std::make_unique<IOSChromeBrowsingDataRemover>(browserState))
            .first;
    DCHECK(iterator != _browingDataRemovers.end());
  }
  iterator->second->Remove(timePeriod, mask,
                           base::BindBlockArc(browsingDataCleared));
}

- (BOOL)hasPendingRemovalOperations:(ios::ChromeBrowserState*)browserState {
  auto iterator = _browingDataRemovers.find(browserState);
  return iterator != _browingDataRemovers.end() &&
         iterator->second->is_removing();
}

- (void)browserStateDestroyed:(ios::ChromeBrowserState*)browserState {
  _browingDataRemovers.erase(browserState);
}

@end
