// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/engine/feature/geolocation/blimp_location_provider.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/memory/weak_ptr.h"
#include "device/geolocation/geoposition.h"

namespace blimp {
namespace engine {

BlimpLocationProvider::BlimpLocationProvider(
    base::WeakPtr<BlimpLocationProvider::Delegate> delegate)
    : delegate_(delegate), is_started_(false) {}

BlimpLocationProvider::~BlimpLocationProvider() {
  if (is_started_) {
    StopProvider();
  }
}

bool BlimpLocationProvider::StartProvider(bool high_accuracy) {
  if (delegate_) {
    if (high_accuracy) {
      delegate_->RequestAccuracy(
          GeolocationSetInterestLevelMessage::HIGH_ACCURACY);
    } else {
      delegate_->RequestAccuracy(
          GeolocationSetInterestLevelMessage::LOW_ACCURACY);
    }
    is_started_ = true;
  }
  return is_started_;
}

void BlimpLocationProvider::StopProvider() {
  DCHECK(is_started_);
  if (delegate_) {
    delegate_->RequestAccuracy(GeolocationSetInterestLevelMessage::NO_INTEREST);
    is_started_ = false;
  }
}

void BlimpLocationProvider::GetPosition(device::Geoposition* position) {
  *position = cached_position_;
}

void BlimpLocationProvider::RequestRefresh() {
  DCHECK(is_started_);
  if (delegate_) {
    delegate_->RequestRefresh();
  }
}

void BlimpLocationProvider::OnPermissionGranted() {
  RequestRefresh();
}

void BlimpLocationProvider::SetUpdateCallback(
    const LocationProviderUpdateCallback& callback) {
  if (delegate_) {
    delegate_->SetUpdateCallback(base::Bind(callback, base::Unretained(this)));
  }
}

}  // namespace engine
}  // namespace blimp
