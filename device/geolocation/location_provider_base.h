// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_GEOLOCATION_LOCATION_PROVIDER_BASE_H_
#define DEVICE_GEOLOCATION_LOCATION_PROVIDER_BASE_H_

#include "base/macros.h"
#include "device/geolocation/geolocation_export.h"
#include "device/geolocation/location_provider.h"

namespace device {

class DEVICE_GEOLOCATION_EXPORT LocationProviderBase
    : NON_EXPORTED_BASE(public LocationProvider) {
 public:
  LocationProviderBase();
  ~LocationProviderBase() override;

 protected:
  void NotifyCallback(const Geoposition& position);

  // Overridden from LocationProvider:
  void SetUpdateCallback(
      const LocationProviderUpdateCallback& callback) override;
  void RequestRefresh() override;

 private:
  LocationProviderUpdateCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(LocationProviderBase);
};

}  // namespace device

#endif  // DEVICE_GEOLOCATION_LOCATION_PROVIDER_BASE_H_
