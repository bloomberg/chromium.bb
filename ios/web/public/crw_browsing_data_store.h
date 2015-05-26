// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_CRW_BROWSING_DATA_STORE_H_
#define IOS_WEB_CRW_BROWSING_DATA_STORE_H_

#import <Foundation/Foundation.h>

namespace web {
class BrowserState;
}

// A CRWBrowsingDataStore represents various types of data that a web view
// (UIWebView and WKWebView) uses.
// All methods must be called on the UI thread.
@interface CRWBrowsingDataStore : NSObject

// Designated initializer. |browserState| cannot be a nullptr.
- (instancetype)initWithBrowserState:(web::BrowserState*)browserState
    NS_DESIGNATED_INITIALIZER;

// TODO(shreyasv): Implement fetch/restore/delete methods. crbug.com/480654

@end

#endif  // IOS_WEB_CRW_BROWSING_DATA_STORE_H_
