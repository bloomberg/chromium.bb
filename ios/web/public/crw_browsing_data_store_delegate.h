// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_CRW_BROWSING_DATA_STORE_DELEGATE_H_
#define IOS_WEB_PUBLIC_CRW_BROWSING_DATA_STORE_DELEGATE_H_

#import <Foundation/Foundation.h>

@class CRWBrowsingDataStore;

namespace web {

// The policy to pass back to the CRWBrowsingDataStore when the
// CRWBrowsingDataStore's mode wants to become |ACTIVE|.
typedef NS_ENUM(NSUInteger, BrowsingDataStoreMakeActivePolicy) {
  // Adopt browsing data to the canonical path where web views store their
  // browsing data.
  ADOPT = 1,
  // Restore browsing data from the stash path. This is the default policy and
  // is the same policy that is used if the delegate was not implemented.
  RESTORE,
};

// The policy to pass back to the CRWBrowsingDataStore when the
// CRWBrowsingDataStore's mode wants to become |INACTIVE|.
typedef NS_ENUM(NSUInteger, BrowsingDataStoreMakeInactivePolicy) {
  // Delete browsing data created by the web views.
  DELETE = 1,
  // Stash browsing data created by the web views. This is the default policy
  // and is the same policy that is used if the delegate was not implemented.
  STASH,
};

}  // namespace web

// The CRWBrowsingDataStoreDelegate has methods that can override the default
// behavior of a CRWBrowsingDataStore when a mode change occurs.
@protocol CRWBrowsingDataStoreDelegate<NSObject>

// Called when a CRWBrowsingDataStore wants to change its mode to |ACTIVE|.
- (web::BrowsingDataStoreMakeActivePolicy)
    decideMakeActiveOperationPolicyForBrowsingDataStore:
        (CRWBrowsingDataStore*)browsingDataStore;

// Called when a CRWBrowsingDataStore wants to change its mode to |INACTIVE|.
- (web::BrowsingDataStoreMakeInactivePolicy)
    decideMakeInactiveOperationPolicyForBrowsingDataStore:
        (CRWBrowsingDataStore*)browsingDataStore;

@end

#endif  // IOS_WEB_PUBLIC_CRW_BROWSING_DATA_STORE_DELEGATE_H_
