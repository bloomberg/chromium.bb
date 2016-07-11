// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/geolocation_delegate.h"
#include "content/public/browser/location_provider.h"

namespace content {

bool GeolocationDelegate::UseNetworkLocationProviders() {
  return true;
}

AccessTokenStore* GeolocationDelegate::CreateAccessTokenStore() {
  return nullptr;
}

std::unique_ptr<LocationProvider>
GeolocationDelegate::OverrideSystemLocationProvider() {
  return nullptr;
}

}  // namespace content
