// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/public/provider/chrome/browser/updatable_resource_provider.h"

namespace ios {

UpdatableResourceProvider::UpdatableResourceProvider() {
}

UpdatableResourceProvider::~UpdatableResourceProvider() {
}

NSString* UpdatableResourceProvider::GetUpdateNotificationName() {
  return nil;
}

id<UpdatableResourceBridge> UpdatableResourceProvider::CreateUpdatableResource(
    NSString* resource_identifier,
    id<UpdatableResourceDelegate> delegate) {
  return nil;
}

}  // namespace ios
