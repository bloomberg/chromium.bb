// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/geolocation/geolocation_delegate.h"

#include "device/geolocation/location_provider.h"

namespace device {

std::unique_ptr<LocationProvider>
GeolocationDelegate::OverrideSystemLocationProvider() {
  return nullptr;
}

}  // namespace device
