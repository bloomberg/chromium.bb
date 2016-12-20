// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_GENERIC_SENSOR_PLATFORM_SENSOR_READER_LINUX_H_
#define DEVICE_GENERIC_SENSOR_PLATFORM_SENSOR_READER_LINUX_H_

#include "base/memory/ref_counted.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace device {

class PlatformSensorLinux;
class PlatformSensorConfiguration;
struct SensorInfoLinux;

// A generic reader class that can be implemented with two different strategies:
// polling and on trigger.
class SensorReader {
 public:
  // Creates a new instance of SensorReader. At the moment, only polling
  // reader is supported.
  static std::unique_ptr<SensorReader> Create(
      const SensorInfoLinux* sensor_device,
      PlatformSensorLinux* sensor,
      scoped_refptr<base::SingleThreadTaskRunner> polling_thread_task_runner);

  virtual ~SensorReader();

  // Starts fetching data based on strategy this reader has chosen.
  // Only polling strategy is supported at the moment. Thread safe.
  virtual bool StartFetchingData(
      const PlatformSensorConfiguration& configuration) = 0;

  // Stops fetching data. Thread safe.
  virtual void StopFetchingData() = 0;

 protected:
  SensorReader(PlatformSensorLinux* sensor,
               scoped_refptr<base::SingleThreadTaskRunner> polling_task_runner);

  // Notifies |sensor_| about an error.
  void NotifyReadError();

  // Non-owned pointer to a sensor that created this reader.
  PlatformSensorLinux* sensor_;

  // A task runner that is used to poll data.
  scoped_refptr<base::SingleThreadTaskRunner> polling_task_runner_;

  // A task runner that belongs to a thread this reader is created on.
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  // Indicates if reading is active.
  bool is_reading_active_;

  DISALLOW_COPY_AND_ASSIGN(SensorReader);
};

}  // namespace device

#endif  // DEVICE_GENERIC_SENSOR_PLATFORM_SENSOR_READER_LINUX_H_
