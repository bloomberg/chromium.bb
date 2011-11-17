// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file declares a CoreLocation provider that runs on Mac OS X (10.6).
// Public for testing only - for normal usage this  header should not be
// required, as location_provider.h declares the needed factory function.

#ifndef CONTENT_BROWSER_GEOLOCATION_CORE_LOCATION_PROVIDER_MAC_H_
#define CONTENT_BROWSER_GEOLOCATION_CORE_LOCATION_PROVIDER_MAC_H_
#pragma once

#include "content/browser/geolocation/location_provider.h"
#include "content/common/geoposition.h"

class CoreLocationDataProviderMac;

class CoreLocationProviderMac : public LocationProviderBase {
 public:
  explicit CoreLocationProviderMac();
  virtual ~CoreLocationProviderMac();

  // LocationProvider
  virtual bool StartProvider(bool high_accuracy) OVERRIDE;
  virtual void StopProvider() OVERRIDE;
  virtual void GetPosition(Geoposition* position) OVERRIDE;

  // Receives new positions and calls UpdateListeners
  void SetPosition(Geoposition* position);

 private:
  bool is_updating_;
  CoreLocationDataProviderMac* data_provider_;
  Geoposition position_;
};

#endif  // CONTENT_BROWSER_GEOLOCATION_CORE_LOCATION_PROVIDER_MAC_H_
