// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_GENERIC_SENSOR_PLATFORM_SENSOR_PROVIDER_BASE_H_
#define DEVICE_GENERIC_SENSOR_PLATFORM_SENSOR_PROVIDER_BASE_H_

#include "base/macros.h"

#include "base/threading/non_thread_safe.h"
#include "device/generic_sensor/platform_sensor.h"

namespace device {

// Base class that defines factory methods for PlatformSensor creation.
// Its implementations must be accessed via GetInstance() method.
class PlatformSensorProviderBase : public base::NonThreadSafe {
 public:
  // Returns the PlatformSensorProviderBase singleton.
  // Note: returns 'nullptr' if there is no available implementation for
  // the current platform.
  static PlatformSensorProviderBase* GetInstance();

  // Creates new instance of PlatformSensor.
  scoped_refptr<PlatformSensor> CreateSensor(mojom::SensorType type,
                                             uint64_t size,
                                             uint64_t offset);

  // Gets previously created instance of PlatformSensor by sensor type |type|.
  scoped_refptr<PlatformSensor> GetSensor(mojom::SensorType type);

  // Shared buffer getters.
  mojo::ScopedSharedBufferHandle CloneSharedBufferHandle();

 protected:
  PlatformSensorProviderBase();
  virtual ~PlatformSensorProviderBase();

  // Method that must be implemented by platform specific classes.
  virtual scoped_refptr<PlatformSensor> CreateSensorInternal(
      mojom::SensorType type,
      mojo::ScopedSharedBufferMapping mapping,
      uint64_t buffer_size) = 0;

 private:
  friend class PlatformSensor;  // To call RemoveSensor();

  bool CreateSharedBufferIfNeeded();
  void RemoveSensor(mojom::SensorType type);

 private:
  std::map<mojom::SensorType, PlatformSensor*> sensor_map_;
  mojo::ScopedSharedBufferHandle shared_buffer_handle_;

  DISALLOW_COPY_AND_ASSIGN(PlatformSensorProviderBase);
};

}  // namespace device

#endif  // DEVICE_GENERIC_SENSOR_PLATFORM_SENSOR_PROVIDER_BASE_H_
