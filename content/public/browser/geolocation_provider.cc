// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/geolocation_provider.h"

namespace content {

bool GeolocationProvider::Delegate::UseNetworkLocationProviders() {
  return true;
}

AccessTokenStore*
GeolocationProvider::Delegate::CreateAccessTokenStore() {
  return nullptr;
}

LocationProvider*
GeolocationProvider::Delegate::OverrideSystemLocationProvider() {
  return nullptr;
}

}  // namespace content
