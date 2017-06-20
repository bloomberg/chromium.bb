// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_SENSORS_DEVICE_SENSOR_HOST_H_
#define DEVICE_SENSORS_DEVICE_SENSOR_HOST_H_

#include "base/macros.h"
#include "base/memory/shared_memory.h"
#include "base/threading/thread_checker.h"
#include "device/sensors/device_sensors_consts.h"
#include "device/sensors/public/interfaces/motion.mojom.h"
#include "device/sensors/public/interfaces/orientation.mojom.h"
#include "mojo/public/cpp/bindings/interface_request.h"

namespace device {

// A base class for device sensor related mojo interface implementations.
template <typename MojoInterface, ConsumerType consumer_type>
class DeviceSensorHost : NON_EXPORTED_BASE(public MojoInterface) {
 public:
  static void Create(mojo::InterfaceRequest<MojoInterface> request);

  // All methods below to be called on the IO thread.
  ~DeviceSensorHost() override;

 private:
  DeviceSensorHost();

  void StartPolling(
      typename MojoInterface::StartPollingCallback callback) override;
  void StopPolling() override;

  bool is_started_;

  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(DeviceSensorHost);
};

using DeviceMotionHost =
    DeviceSensorHost<device::mojom::MotionSensor, CONSUMER_TYPE_MOTION>;
using DeviceOrientationHost = DeviceSensorHost<device::mojom::OrientationSensor,
                                               CONSUMER_TYPE_ORIENTATION>;
using DeviceOrientationAbsoluteHost =
    DeviceSensorHost<device::mojom::OrientationAbsoluteSensor,
                     CONSUMER_TYPE_ORIENTATION_ABSOLUTE>;

}  // namespace device

#endif  // DEVICE_SENSORS_DEVICE_SENSOR_HOST_H_
