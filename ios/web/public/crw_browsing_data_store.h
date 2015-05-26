// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_CRW_BROWSING_DATA_STORE_H_
#define IOS_WEB_CRW_BROWSING_DATA_STORE_H_

#import <Foundation/Foundation.h>

#import "base/ios/block_types.h"

namespace web {
class BrowserState;

// Represents the cookie browsing data that a web view stores.
extern NSString* const kBrowsingDataTypeCookies;
}

// Represents the modes that a CRWBrowsingDataStore is currently at.
enum CRWBrowsingDataStoreMode {
  // Web views (associated transitively through the BrowseState) are
  // flushing/reading their data from disk.
  ACTIVE,
  // The CRWBrowsingDataStore's mode is in the process of becoming either ACTIVE
  // or INACTIVE.
  SYNCHRONIZING,
  // Browsing data is stored in a path unique to the BrowserState and is
  // currently not being read or written to by web views.
  INACTIVE
};

// A CRWBrowsingDataStore represents various types of data that a web view
// (UIWebView and WKWebView) uses.
// All methods must be called on the UI thread.
@interface CRWBrowsingDataStore : NSObject

// Designated initializer. |browserState| cannot be a nullptr.
// The |web::ActiveStateManager| associated with |browserState| needs to be in
// active state.
- (instancetype)initWithBrowserState:(web::BrowserState*)browserState
    NS_DESIGNATED_INITIALIZER;

// Returns a set of all available browsing data types.
+ (NSSet*)allBrowsingDataTypes;

// The mode that the CRWBrowsingDataStore is in.
@property(nonatomic, assign, readonly) CRWBrowsingDataStoreMode mode;

// TODO(shreyasv): Verify the preconditions for the following 3 methods when
// web::WebViewCounter class is implemented. crbug.com/480507

// Changes the mode to |ACTIVE|.
// |completionHandler| is called on the main thread.
// Precondition: There must be no web views associated with the BrowserState.
// Note: If there is another operation driven to change the mode, the mode will
// still be |SYNCHRONIZING| rather than |ACTIVE| when the callback is received.
- (void)makeActiveWithCompletionHandler:(ProceduralBlock)completionHandler;

// Changes the mode to |INACTIVE|.
// |completionHandler| is called on the main thread.
// Precondition: There must be no web views associated with the BrowserState.
// Note: If there is another operation driven to change the mode, the mode will
// still be |SYNCHRONIZING| rather than |INACTIVE| when the callback is
// received.
- (void)makeInactiveWithCompletionHandler:(ProceduralBlock)completionHandler;

// Removes all browsing data of the provided |browsingDataTypes|.
// |completionHandler| is called on the main thread after the browsing data has
// been removed.
// Precondition: There must be no web views associated with the BrowserState.
- (void)removeDataOfTypes:(NSSet*)browsingDataTypes
        completionHandler:(ProceduralBlock)completionHandler;

// Returns YES if there is still a pending operation that has not finished
// running. Creating web views (UIWebViews and WKWebViews) with this
// CRWBrowsingDataStore when there are pending operations is undefined
// behavior.
@property(nonatomic, assign, readonly) BOOL hasPendingOperations;

@end

#endif  // IOS_WEB_CRW_BROWSING_DATA_STORE_H_
