// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_CRW_BROWSING_DATA_STORE_H_
#define IOS_WEB_PUBLIC_CRW_BROWSING_DATA_STORE_H_

#import <Foundation/Foundation.h>

#import "base/ios/block_types.h"
#import "ios/web/public/crw_browsing_data_store_delegate.h"

namespace web {
class BrowserState;

// Represents the various kinds of browsing data that a CRWBrowsingDataStore
// can handle.
typedef NS_OPTIONS(NSUInteger, BrowsingDataTypes) {
  // Represents the cookie browsing data that a web view stores.
  BROWSING_DATA_TYPE_COOKIES = 1 << 0,
  // Represents all the browsing data that a web view stores.
  BROWSING_DATA_TYPE_ALL = BROWSING_DATA_TYPE_COOKIES,
};

// Represents the modes that a CRWBrowsingDataStore can be in.
typedef NS_ENUM(NSUInteger, BrowsingDataStoreMode) {
  // Web views (associated transitively through the BrowseState) are
  // flushing/reading their data from disk.
  ACTIVE = 1,
  // The CRWBrowsingDataStore's mode is in the process of becoming either ACTIVE
  // or INACTIVE.
  CHANGING,
  // Browsing data is stored in a path unique to the BrowserState and is
  // currently not being read or written to by web views.
  INACTIVE,
};

}  // namespace web

// A CRWBrowsingDataStore represents various types of data that a web view uses.
// All methods must be called on the main thread.
@interface CRWBrowsingDataStore : NSObject

// The delegate that is consulted when the mode needs to change.
@property(nonatomic, weak) id<CRWBrowsingDataStoreDelegate> delegate;

// The mode that the CRWBrowsingDataStore is in. KVO compliant.
@property(nonatomic, assign, readonly) web::BrowsingDataStoreMode mode;

// A BOOL indicating whether there is still a pending operation that has not
// finished running. Creating web views with this CRWBrowsingDataStore when
// there are pending operations results in undefined behavior.
@property(nonatomic, assign, readonly) BOOL hasPendingOperations;

// |browserState| cannot be null. The initial mode of the
// CRWBrowsingDataStore is obtained from the active state of the
// |web::ActiveStateManager| associated with |browserState|.
- (instancetype)initWithBrowserState:(web::BrowserState*)browserState
    NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

// Changes the mode to |ACTIVE|.
// If there is no delegate present, the default behavior of this method is to
// restore browsing data from |browserState|'s stash path to the canonical path
// where web views read/write browsing data to.
// |completionHandler| is called on the main thread. This block has no return
// value and takes a single BOOL argument that indicates whether or not the
// the mode was successfully changed to |ACTIVE|.
// The mode change to |ACTIVE| can fail if another |makeActive| or
// |makeInactive| was enqueued after this call.
// Precondition: There must be no web views associated with the BrowserState.
- (void)makeActiveWithCompletionHandler:
    (void (^)(BOOL success))completionHandler;

// Changes the mode to |INACTIVE|.
// If there is no delegate present, the default behavior of this method is to
// stash browsing data created by the web view in to the |browserState|'s stash
// path.
// |completionHandler| is called on the main thread. This block has no return
// value and takes a single BOOL argument that indicates whether or not the
// the mode was successfully changed to |INACTIVE|.
// The mode change to |INACTIVE| can fail if another |makeActive| or
// |makeInactive| was enqueued after this call.
// Precondition: There must be no web views associated with the BrowserState.
- (void)makeInactiveWithCompletionHandler:
    (void (^)(BOOL success))completionHandler;

// Removes all browsing data of the provided |browsingDataTypes|.
// |completionHandler| is called on the main thread after the browsing data has
// been removed.
// Precondition: There must be no web views associated with the BrowserState.
- (void)removeDataOfTypes:(web::BrowsingDataTypes)browsingDataTypes
        completionHandler:(ProceduralBlock)completionHandler;

@end

#endif  // IOS_WEB_PUBLIC_CRW_BROWSING_DATA_STORE_H_
