// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/store_kit/store_kit_tab_helper.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

DEFINE_WEB_STATE_USER_DATA_KEY(StoreKitTabHelper);

StoreKitTabHelper::StoreKitTabHelper(web::WebState* web_state) {}

StoreKitTabHelper::~StoreKitTabHelper() {}

void StoreKitTabHelper::SetLauncher(id<StoreKitLauncher> launcher) {
  store_kit_launcher_ = launcher;
}

id<StoreKitLauncher> StoreKitTabHelper::GetLauncher() {
  return store_kit_launcher_;
}

void StoreKitTabHelper::OpenAppStore(NSString* app_id) {
  [store_kit_launcher_ openAppStore:app_id];
}
