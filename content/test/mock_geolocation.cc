// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/mock_geolocation.h"

#include "base/logging.h"
#include "content/browser/geolocation/arbitrator_dependency_factories_for_test.h"
#include "content/browser/geolocation/location_arbitrator.h"
#include "content/browser/geolocation/mock_location_provider.h"
#include "content/common/geoposition.h"

namespace content {

MockGeolocation::MockGeolocation() {
  dependency_factory_ =
      new GeolocationArbitratorDependencyFactoryWithLocationProvider(
          &NewAutoSuccessMockLocationProvider);
}

MockGeolocation::~MockGeolocation() {
}

void MockGeolocation::Setup() {
  GeolocationArbitrator::SetDependencyFactoryForTest(
      dependency_factory_.get());
}

void MockGeolocation::TearDown() {
  GeolocationArbitrator::SetDependencyFactoryForTest(NULL);
}

void MockGeolocation::GetCurrentPosition(double* latitude,
                                         double* longitude) const {
  *latitude = MockLocationProvider::instance_->position_.latitude;
  *longitude = MockLocationProvider::instance_->position_.longitude;
}

void MockGeolocation::SetCurrentPosition(double latitude, double longitude) {
  Geoposition geoposition;
  geoposition.latitude = latitude;
  geoposition.longitude = longitude;
  geoposition.accuracy = 0;
  geoposition.error_code = Geoposition::ERROR_CODE_NONE;
  // Webkit compares the timestamp to wall clock time, so we need
  // it to be contemporary.
  geoposition.timestamp = base::Time::Now();
  DCHECK(geoposition.IsValidFix());

  MockLocationProvider::instance_->HandlePositionChanged(geoposition);
}

}  // namespace content
