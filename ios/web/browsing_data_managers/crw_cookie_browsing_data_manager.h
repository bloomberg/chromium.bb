// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_BROWSING_DATA_MANAGERS_CRW_COOKIE_BROWSING_DATA_MANAGER_H_
#define IOS_WEB_BROWSING_DATA_MANAGERS_CRW_COOKIE_BROWSING_DATA_MANAGER_H_

#import <Foundation/Foundation.h>

#import "ios/web/browsing_data_managers/crw_browsing_data_manager.h"

namespace web {
class BrowserState;
}

@interface CRWCookieBrowsingDataManager : NSObject<CRWBrowsingDataManager>

// Designated initializer. |browserState| cannot be a nullptr.
- (instancetype)initWithBrowserState:(web::BrowserState*)browserState
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_WEB_BROWSING_DATA_MANAGERS_CRW_COOKIE_BROWSING_DATA_MANAGER_H_
