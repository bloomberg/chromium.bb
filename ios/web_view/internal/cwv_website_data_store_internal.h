// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_INTERNAL_CWV_WEBSITE_DATA_STORE_INTERNAL_H_
#define IOS_WEB_VIEW_INTERNAL_CWV_WEBSITE_DATA_STORE_INTERNAL_H_

#import "ios/web_view/public/cwv_website_data_store.h"

namespace ios_web_view {
class WebViewBrowserState;
}  // namespace ios_web_view

@interface CWVWebsiteDataStore ()

// The browser state associated with this website data store.
@property(nonatomic, readonly, nonnull)
    ios_web_view::WebViewBrowserState* browserState;

@end

#endif  // IOS_WEB_VIEW_INTERNAL_CWV_WEBSITE_DATA_STORE_INTERNAL_H_
