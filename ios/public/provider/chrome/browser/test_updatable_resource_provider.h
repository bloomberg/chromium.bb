// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_PUBLIC_PROVIDER_CHROME_BROWSER_TEST_UPDATABLE_RESOURCE_PROVIDER_H_
#define IOS_PUBLIC_PROVIDER_CHROME_BROWSER_TEST_UPDATABLE_RESOURCE_PROVIDER_H_

#import "ios/public/provider/chrome/browser/updatable_resource_provider.h"

namespace ios {

// Dummy UpdatableResourceProvider implementation that simply loads data from
// the specified plist file.
class TestUpdatableResourceProvider : public ios::UpdatableResourceProvider {
 private:
  // UpdatableResourceProvider implementation.
  NSString* GetUpdateNotificationName() override;
  id<UpdatableResourceBridge> CreateUpdatableResource(
      NSString* resource_identifier,
      id<UpdatableResourceDelegate> delegate) override;
};

}  // namespace ios

#endif  // IOS_PUBLIC_PROVIDER_CHROME_BROWSER_TEST_UPDATABLE_RESOURCE_PROVIDER_H_
