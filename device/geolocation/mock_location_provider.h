// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_GEOLOCATION_MOCK_LOCATION_PROVIDER_H_
#define DEVICE_GEOLOCATION_MOCK_LOCATION_PROVIDER_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread.h"
#include "device/geolocation/geoposition.h"
#include "device/geolocation/location_provider_base.h"

namespace device {

// Mock implementation of a location provider for testing.
class MockLocationProvider : public LocationProviderBase {
 public:
  enum State { STOPPED, LOW_ACCURACY, HIGH_ACCURACY } state_;

  MockLocationProvider();
  ~MockLocationProvider() override;

  bool is_started() const { return state_ != STOPPED; }
  bool is_permission_granted() const { return is_permission_granted_; }
  const Geoposition& position() const { return position_; }

  // Updates listeners with the new position.
  void HandlePositionChanged(const Geoposition& position);

  // LocationProvider implementation.
  bool StartProvider(bool high_accuracy) override;
  void StopProvider() override;
  const Geoposition& GetPosition() override;
  void OnPermissionGranted() override;

 private:
  bool is_permission_granted_;
  Geoposition position_;
  scoped_refptr<base::SingleThreadTaskRunner> provider_task_runner_;

  DISALLOW_COPY_AND_ASSIGN(MockLocationProvider);
};

}  // namespace device

#endif  // DEVICE_GEOLOCATION_MOCK_LOCATION_PROVIDER_H_
