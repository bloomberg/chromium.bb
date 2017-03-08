// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_STORE_KIT_STORE_KIT_TAB_HELPER_H_
#define IOS_CHROME_BROWSER_STORE_KIT_STORE_KIT_TAB_HELPER_H_

#include "base/ios/weak_nsobject.h"
#include "base/macros.h"
#import "ios/chrome/browser/store_kit/store_kit_launcher.h"
#import "ios/web/public/web_state/web_state_user_data.h"

// A Tab Helper object that can open to a page on iOS App Store for a given
// app product ID.
class StoreKitTabHelper : public web::WebStateUserData<StoreKitTabHelper> {
 public:
  explicit StoreKitTabHelper(web::WebState* web_state);
  ~StoreKitTabHelper() override;

  void SetLauncher(id<StoreKitLauncher> launcher);
  id<StoreKitLauncher> GetLauncher();

  void OpenAppStore(NSString* app_id);

 private:
  base::WeakNSProtocol<id<StoreKitLauncher>> store_kit_launcher_;

  DISALLOW_COPY_AND_ASSIGN(StoreKitTabHelper);
};

#endif  // IOS_CHROME_BROWSER_STORE_KIT_STORE_KIT_TAB_HELPER_H_
