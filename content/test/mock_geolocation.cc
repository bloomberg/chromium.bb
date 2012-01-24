// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/mock_geolocation.h"

#include "content/browser/geolocation/arbitrator_dependency_factories_for_test.h"
#include "content/browser/geolocation/location_arbitrator.h"
#include "content/browser/geolocation/mock_location_provider.h"

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

Geoposition MockGeolocation::GetCurrentPosition() const {
  return MockLocationProvider::instance_->position_;
}

void MockGeolocation::SetCurrentPosition(const Geoposition& position) {
  MockLocationProvider::instance_->HandlePositionChanged(position);
}

}  // namespace content
