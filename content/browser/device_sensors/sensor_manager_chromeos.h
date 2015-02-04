// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVICE_SENSORS_SENSOR_MANAGER_CHROMEOS_H_
#define CONTENT_BROWSER_DEVICE_SENSORS_SENSOR_MANAGER_CHROMEOS_H_

#include "base/macros.h"
#include "base/synchronization/lock.h"
#include "chromeos/accelerometer/accelerometer_reader.h"
#include "chromeos/accelerometer/accelerometer_types.h"
#include "content/common/content_export.h"
#include "content/common/device_sensors/device_orientation_hardware_buffer.h"

namespace content {

// Observes Chrome OS accelerometer sensors, and provides updated device
// orientation information.
class CONTENT_EXPORT SensorManagerChromeOS
    : public chromeos::AccelerometerReader::Observer {
 public:
  SensorManagerChromeOS();
  ~SensorManagerChromeOS() override;

  // Begins monitoring of orientation events, the shared memory of |buffer| will
  // be updated upon subsequent events.
  bool StartFetchingDeviceOrientationData(
      DeviceOrientationHardwareBuffer* buffer);

  // Stops monitoring orientation events. Returns true if there is an active
  // |orientation_buffer_| and fetching stops. Otherwise returns false.
  bool StopFetchingDeviceOrientationData();

  // chromeos::AccelerometerReader::Observer:
  void OnAccelerometerUpdated(
      const chromeos::AccelerometerUpdate& update) override;

 protected:
  // Begins/ends the observation of accelerometer events.
  virtual void StartObservingAccelerometer();
  virtual void StopObservingAccelerometer();

 private:
  // Shared memory to update.
  DeviceOrientationHardwareBuffer* orientation_buffer_;

  // Synchronize orientation_buffer_ across threads.
  base::Lock orientation_buffer_lock_;

  DISALLOW_COPY_AND_ASSIGN(SensorManagerChromeOS);
};

}  // namespace content

#endif  // CONTENT_BROWSER_DEVICE_SENSORS_SENSOR_MANAGER_CHROMEOS_H_
