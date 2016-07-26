// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_ENGINE_FEATURE_GEOLOCATION_BLIMP_LOCATION_PROVIDER_H_
#define BLIMP_ENGINE_FEATURE_GEOLOCATION_BLIMP_LOCATION_PROVIDER_H_

#include "base/memory/weak_ptr.h"
#include "blimp/common/proto/geolocation.pb.h"
#include "content/public/browser/location_provider.h"
#include "content/public/common/geoposition.h"

namespace blimp {
namespace engine {

// Location provider for Blimp using the device's provider over the network.
class BlimpLocationProvider : public content::LocationProvider {
 public:
  // A delegate that implements a subset of LocationProvider's functions.
  class Delegate {
   public:
    using GeopositionReceivedCallback =
        base::Callback<void(const content::Geoposition&)>;

    virtual ~Delegate() {}

    virtual void RequestAccuracy(
        GeolocationSetInterestLevelMessage::Level level) = 0;
    virtual void RequestRefresh() = 0;
    virtual void SetUpdateCallback(
        const GeopositionReceivedCallback& callback) = 0;
  };

  explicit BlimpLocationProvider(base::WeakPtr<Delegate> delegate);
  ~BlimpLocationProvider() override;

  // content::LocationProvider implementation.
  bool StartProvider(bool high_accuracy) override;
  void StopProvider() override;
  void GetPosition(content::Geoposition* position) override;
  void RequestRefresh() override;
  void OnPermissionGranted() override;
  void SetUpdateCallback(
      const LocationProviderUpdateCallback& callback) override;

 private:
  // This delegate handles a subset of the LocationProvider functionality.
  base::WeakPtr<Delegate> delegate_;

  content::Geoposition cached_position_;

  // True if a successful StartProvider call has occured.
  bool is_started_;

  DISALLOW_COPY_AND_ASSIGN(BlimpLocationProvider);
};

}  // namespace engine
}  // namespace blimp

#endif  // BLIMP_ENGINE_FEATURE_GEOLOCATION_BLIMP_LOCATION_PROVIDER_H_
