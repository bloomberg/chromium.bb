// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_GEOLOCATION_LOCATION_PROVIDER_ANDROID_H_
#define CONTENT_BROWSER_GEOLOCATION_LOCATION_PROVIDER_ANDROID_H_

#include "base/compiler_specific.h"
#include "content/browser/geolocation/location_provider.h"
#include "content/public/common/geoposition.h"

class AndroidLocationApiAdapter;
struct Geoposition;

// Location provider for Android using the platform provider over JNI.
class LocationProviderAndroid : public LocationProviderBase {
 public:
  LocationProviderAndroid();
  virtual ~LocationProviderAndroid();

  // Called by the AndroidLocationApiAdapter.
  void NotifyNewGeoposition(const content::Geoposition& position);

  // LocationProviderBase.
  virtual bool StartProvider(bool high_accuracy) OVERRIDE;
  virtual void StopProvider() OVERRIDE;
  virtual void GetPosition(content::Geoposition* position) OVERRIDE;
  virtual void UpdatePosition() OVERRIDE;
  virtual void OnPermissionGranted() OVERRIDE;

 private:
  content::Geoposition last_position_;
};

#endif  // CONTENT_BROWSER_GEOLOCATION_LOCATION_PROVIDER_ANDROID_H_
