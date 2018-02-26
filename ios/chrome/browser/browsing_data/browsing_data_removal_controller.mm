// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/browsing_data/browsing_data_removal_controller.h"

#import <WebKit/WebKit.h>

#include <memory>

#include "base/containers/flat_map.h"
#include "base/logging.h"
#import "base/mac/bind_objc_block.h"
#include "ios/chrome/browser/browsing_data/browsing_data_remove_mask.h"
#include "ios/chrome/browser/browsing_data/browsing_data_remover.h"
#include "ios/chrome/browser/browsing_data/browsing_data_remover_factory.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation BrowsingDataRemovalController

- (void)removeBrowsingDataFromBrowserState:
            (ios::ChromeBrowserState*)browserState
                                      mask:(BrowsingDataRemoveMask)mask
                                timePeriod:(browsing_data::TimePeriod)timePeriod
                         completionHandler:(ProceduralBlock)completionHandler {
  DCHECK(browserState);
  BrowsingDataRemoverFactory::GetForBrowserState(browserState)
      ->Remove(timePeriod, mask, base::BindBlockArc(^{
                 if (completionHandler)
                   completionHandler();
               }));
}

- (BOOL)hasPendingRemovalOperations:(ios::ChromeBrowserState*)browserState {
  DCHECK(browserState);
  BrowsingDataRemover* browsingDataRemover =
      BrowsingDataRemoverFactory::GetForBrowserStateIfExists(browserState);
  return browsingDataRemover && browsingDataRemover->IsRemoving();
}

@end
