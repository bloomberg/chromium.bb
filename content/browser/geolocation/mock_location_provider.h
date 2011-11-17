// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_GEOLOCATION_MOCK_LOCATION_PROVIDER_H_
#define CONTENT_BROWSER_GEOLOCATION_MOCK_LOCATION_PROVIDER_H_
#pragma once


#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/threading/thread.h"
#include "content/browser/geolocation/location_provider.h"
#include "content/common/geoposition.h"
#include "googleurl/src/gurl.h"

// Mock implementation of a location provider for testing.
class MockLocationProvider : public LocationProviderBase {
 public:
  // Will update |*self_ref| to point to |this| on construction, and to NULL
  // on destruction.
  explicit MockLocationProvider(MockLocationProvider** self_ref);
  virtual ~MockLocationProvider();

  // Updates listeners with the new position.
  void HandlePositionChanged(const Geoposition& position);

  // LocationProviderBase implementation.
  virtual bool StartProvider(bool high_accuracy) OVERRIDE;
  virtual void StopProvider() OVERRIDE;
  virtual void GetPosition(Geoposition* position) OVERRIDE;
  virtual void OnPermissionGranted(const GURL& requesting_frame) OVERRIDE;

  Geoposition position_;
  enum State { STOPPED, LOW_ACCURACY, HIGH_ACCURACY } state_;
  GURL permission_granted_url_;
  MockLocationProvider** self_ref_;

  scoped_refptr<base::MessageLoopProxy> provider_loop_;

  // Set when an instance of the mock is created via a factory function.
  static MockLocationProvider* instance_;

  DISALLOW_COPY_AND_ASSIGN(MockLocationProvider);
};

// Factory functions for the various sorts of mock location providers,
// for use with GeolocationArbitrator::SetProviderFactoryForTest (i.e.
// not intended for test code to use to get access to the mock, you can use
// MockLocationProvider::instance_ for this, or make a custom factory method).

// Creates a mock location provider with no default behavior.
LocationProviderBase* NewMockLocationProvider();
// Creates a mock location provider that automatically notifies its
// listeners with a valid location when StartProvider is called.
LocationProviderBase* NewAutoSuccessMockLocationProvider();
// Creates a mock location provider that automatically notifies its
// listeners with an error when StartProvider is called.
LocationProviderBase* NewAutoFailMockLocationProvider();
// Similar to NewAutoSuccessMockLocationProvider but mimicks the behavior of
// the Network Location provider, in deferring making location updates until
// a permission request has been confirmed.
LocationProviderBase* NewAutoSuccessMockNetworkLocationProvider();

#endif  // CONTENT_BROWSER_GEOLOCATION_MOCK_LOCATION_PROVIDER_H_
