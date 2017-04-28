// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_SENSORS_DEVICE_SENSOR_SERVICE_H_
#define DEVICE_SENSORS_DEVICE_SENSOR_SERVICE_H_

#include <memory>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/shared_memory.h"
#include "base/memory/singleton.h"
#include "base/message_loop/message_loop.h"
#include "base/threading/thread_checker.h"
#include "device/sensors/device_sensor_export.h"
#include "device/sensors/device_sensors_consts.h"
#include "mojo/public/cpp/system/buffer.h"

namespace device {

class DataFetcherSharedMemory;

// Owns the data fetcher for Device Motion and Orientation and keeps track of
// the number of consumers currently using the data. The data fetcher is stopped
// when there are no consumers.
class DEVICE_SENSOR_EXPORT DeviceSensorService
    : public base::MessageLoop::DestructionObserver {
 public:
  // Returns the DeviceSensorService singleton.
  static DeviceSensorService* GetInstance();

  // Increments the number of users of the provider. The Provider is running
  // when there's > 0 users, and is paused when the count drops to 0.
  // Must be called on the I/O thread.
  void AddConsumer(ConsumerType consumer_type);

  // Removes a consumer. Should be matched with an AddConsumer call.
  // Must be called on the I/O thread.
  void RemoveConsumer(ConsumerType cosumer_type);

  // Returns the shared memory handle of the device motion data.
  mojo::ScopedSharedBufferHandle GetSharedMemoryHandle(
      ConsumerType consumer_type);

  // base::MessageLoop::DestructionObserver:
  void WillDestroyCurrentMessageLoop() override;

  // Stop/join with the background polling thread in |provider_|.
  void Shutdown();

  // Injects a custom data fetcher for testing purposes. This class takes
  // ownership of the injected object.
  void SetDataFetcherForTesting(DataFetcherSharedMemory* test_data_fetcher);

 private:
  friend struct base::DefaultSingletonTraits<DeviceSensorService>;

  DeviceSensorService();
  ~DeviceSensorService() override;

  bool ChangeNumberConsumers(ConsumerType consumer_type, int delta);
  int GetNumberConsumers(ConsumerType consumer_type) const;

  int num_motion_readers_;
  int num_orientation_readers_;
  int num_orientation_absolute_readers_;
  bool is_shutdown_;
  std::unique_ptr<DataFetcherSharedMemory> data_fetcher_;
  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(DeviceSensorService);
};

}  // namespace device

#endif  // DEVICE_SENSORS_DEVICE_SENSOR_SERVICE_H_
