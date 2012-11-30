// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/geolocation/mock_location_arbitrator.h"

#include "base/message_loop.h"
#include "content/public/common/geoposition.h"

namespace content {

MockGeolocationArbitrator::MockGeolocationArbitrator()
    : permission_granted_(false),
      providers_started_(false) {
}

void MockGeolocationArbitrator::StartProviders(
    const GeolocationObserverOptions& options) {
  providers_started_ = true;;
}

void MockGeolocationArbitrator::StopProviders() {
  providers_started_ = false;
}

void MockGeolocationArbitrator::OnPermissionGranted() {
  permission_granted_ = true;
}

bool MockGeolocationArbitrator::HasPermissionBeenGranted() const {
  return permission_granted_;
}

}  // namespace content
