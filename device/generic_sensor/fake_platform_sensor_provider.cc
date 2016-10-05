// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/singleton.h"

#include "device/generic_sensor/fake_platform_sensor.h"
#include "device/generic_sensor/fake_platform_sensor_provider.h"

namespace device {

// static
FakePlatformSensorProvider* FakePlatformSensorProvider::GetInstance() {
  return base::Singleton<
      FakePlatformSensorProvider,
      base::LeakySingletonTraits<FakePlatformSensorProvider>>::get();
}

FakePlatformSensorProvider::FakePlatformSensorProvider() = default;

FakePlatformSensorProvider::~FakePlatformSensorProvider() = default;

void FakePlatformSensorProvider::CreateSensorInternal(
    mojom::SensorType type,
    mojo::ScopedSharedBufferMapping mapping,
    uint64_t buffer_size,
    const CreateSensorCallback& callback) {
  scoped_refptr<FakePlatformSensor> sensor =
      new FakePlatformSensor(type, std::move(mapping), buffer_size, this);
  callback.Run(std::move(sensor));
}

}  // namespace device
