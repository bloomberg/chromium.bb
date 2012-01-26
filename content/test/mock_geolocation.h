// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_TEST_MOCK_GEOLOCATION_H_
#define CONTENT_TEST_MOCK_GEOLOCATION_H_
#pragma once

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"

class GeolocationArbitratorDependencyFactory;

namespace content {

// Creates a mock location provider that gives a valid location.
class MockGeolocation {
 public:
  MockGeolocation();
  ~MockGeolocation();

  // Call this in the test's Setup function.
  void Setup();

  // Call this in the test's TearDown function.
  void TearDown();

  void GetCurrentPosition(double* latitude, double* longitude) const;
  void SetCurrentPosition(double latitude, double longitude);

 private:
  scoped_refptr<GeolocationArbitratorDependencyFactory> dependency_factory_;

  DISALLOW_COPY_AND_ASSIGN(MockGeolocation);
};

}  // namespace content

#endif  // CONTENT_TEST_MOCK_GEOLOCATION_H_
