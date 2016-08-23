// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_GENERIC_SENSOR_SENSOR_PROVIDER_IMPL_H_
#define DEVICE_GENERIC_SENSOR_SENSOR_PROVIDER_IMPL_H_

#include "base/macros.h"
#include "device/generic_sensor/public/interfaces/sensor_provider.mojom.h"
#include "device/generic_sensor/sensor_export.h"
#include "mojo/public/cpp/bindings/strong_binding.h"

namespace device {

class PlatformSensorProvider;

// Implementation of SensorProvider mojo interface.
// Uses PlatformSensorProvider singleton to create platform specific instances
// of PlatformSensor which are used by SensorImpl.
class SensorProviderImpl final : public mojom::SensorProvider {
 public:
  DEVICE_SENSOR_EXPORT static void Create(
      mojo::InterfaceRequest<mojom::SensorProvider> request);

  ~SensorProviderImpl() override;

 private:
  SensorProviderImpl(mojo::InterfaceRequest<mojom::SensorProvider> request,
                     PlatformSensorProvider* provider);

  // SensorProvider implementation.
  void GetSensor(mojom::SensorType type,
                 mojom::SensorRequest sensor_request,
                 const GetSensorCallback& callback) override;

  mojo::StrongBinding<mojom::SensorProvider> binding_;
  PlatformSensorProvider* provider_;
  DISALLOW_COPY_AND_ASSIGN(SensorProviderImpl);
};

}  // namespace device

#endif  // DEVICE_GENERIC_SENSOR_SENSOR_PROVIDER_IMPL_H_
