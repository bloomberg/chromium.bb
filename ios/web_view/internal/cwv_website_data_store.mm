// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/cwv_website_data_store_internal.h"

#include <memory.h>

#include "ios/web_view/internal/web_view_browser_state.h"
#import "ios/web_view/internal/web_view_web_client.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation CWVWebsiteDataStore {
  // TODO(crbug.com/690182): CWVWebsiteDataStore should own _browserState.
  ios_web_view::WebViewBrowserState* _browserState;
}

- (BOOL)isPersistent {
  return !_browserState->IsOffTheRecord();
}

- (ios_web_view::WebViewBrowserState*)browserState {
  return _browserState;
}

- (void)setBrowserState:
    (ios_web_view::WebViewBrowserState* _Nonnull)browserState {
  _browserState = browserState;
}

+ (instancetype)defaultDataStore {
  CWVWebsiteDataStore* dataStore = [[CWVWebsiteDataStore alloc] init];

  ios_web_view::WebViewWebClient* client =
      static_cast<ios_web_view::WebViewWebClient*>(web::GetWebClient());
  [dataStore setBrowserState:client->browser_state()];

  return dataStore;
}

+ (instancetype)nonPersistentDataStore {
  CWVWebsiteDataStore* dataStore = [[CWVWebsiteDataStore alloc] init];

  ios_web_view::WebViewWebClient* client =
      static_cast<ios_web_view::WebViewWebClient*>(web::GetWebClient());
  [dataStore setBrowserState:client->off_the_record_browser_state()];

  return dataStore;
}

@end
