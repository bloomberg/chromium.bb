// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_GENERIC_SENSOR_PUBLIC_CPP_SENSOR_READING_H_
#define DEVICE_GENERIC_SENSOR_PUBLIC_CPP_SENSOR_READING_H_

#include "device/base/synchronization/one_writer_seqlock.h"
#include "device/generic_sensor/public/cpp/generic_sensor_public_export.h"
#include "device/generic_sensor/public/interfaces/sensor.mojom.h"

namespace device {

// This class is guarantied to have a fixed size of 64 bits on every platform.
// It is introduce to simplify sensors shared buffer memory calculation.
template <typename Data>
class SensorReadingField {
 public:
  static_assert(sizeof(Data) <= sizeof(int64_t),
                "The field size must be <= 64 bits.");
  SensorReadingField() = default;
  SensorReadingField(Data value) { storage_.value = value; }
  SensorReadingField& operator=(Data value) {
    storage_.value = value;
    return *this;
  }
  Data& value() { return storage_.value; }
  const Data& value() const { return storage_.value; }

  operator Data() const { return storage_.value; }

 private:
  union Storage {
    int64_t unused;
    Data value;
    Storage() { new (&value) Data(); }
    ~Storage() { value.~Data(); }
  };
  Storage storage_;
};

// This structure represents sensor reading data: timestamp and 4 values.
struct DEVICE_GENERIC_SENSOR_PUBLIC_EXPORT SensorReading {
  SensorReading();
  ~SensorReading();
  SensorReading(const SensorReading& other);
  SensorReadingField<double> timestamp;
  constexpr static int kValuesCount = 4;
  // AMBIENT_LIGHT:
  // values[0]: ambient light level in SI lux units.
  //
  // PROXIMITY:
  // values[0]: proximity sensor distance measured in centimeters.
  //
  // ACCELEROMETER:
  // values[0]: acceleration minus Gx on the x-axis.
  // values[1]: acceleration minus Gy on the y-axis.
  // values[2]: acceleration minus Gz on the z-axis.
  //
  // LINEAR_ACCELERATION:
  // values[0]: acceleration on the x-axis.
  // values[1]: acceleration on the y-axis.
  // values[2]: acceleration on the z-axis.
  //
  // GYROSCOPE:
  // values[0]: angular speed around the x-axis.
  // values[1]: angular speed around the y-axis.
  // values[2]: angular speed around the z-axis.
  //
  // MAGNETOMETER:
  // values[0]: ambient magnetic field in the x-axis in micro-Tesla (uT).
  // values[1]: ambient magnetic field in the y-axis in micro-Tesla (uT).
  // values[2]: ambient magnetic field in the z-axis in micro-Tesla (uT).
  //
  // PRESSURE:
  // values[0]: atmospheric pressure in hPa (millibar).
  //
  // ABSOLUTE_ORIENTATION:
  // values[0]: x value of a quaternion representing the orientation of the
  // device in 3D space.
  // values[1]: y value of a quaternion representing the orientation of the
  // device in 3D space.
  // values[2]: z value of a quaternion representing the orientation of the
  // device in 3D space.
  // values[3]: w value of a quaternion representing the orientation of the
  // device in 3D space.
  //
  // RELATIVE_ORIENTATION:
  // (Identical to ABSOLUTE_ORIENTATION except that it doesn't use the
  // geomagnetic field.)
  // values[0]: x value of a quaternion representing the orientation of the
  // device in 3D space.
  // values[1]: y value of a quaternion representing the orientation of the
  // device in 3D space.
  // values[2]: z value of a quaternion representing the orientation of the
  // device in 3D space.
  // values[3]: w value of a quaternion representing the orientation of the
  // device in 3D space.
  SensorReadingField<double> values[kValuesCount];
};

// This structure represents sensor reading buffer: sensor reading and seqlock
// for synchronization.
struct DEVICE_GENERIC_SENSOR_PUBLIC_EXPORT SensorReadingSharedBuffer {
  SensorReadingSharedBuffer();
  ~SensorReadingSharedBuffer();
  SensorReadingField<OneWriterSeqLock> seqlock;
  SensorReading reading;

  // Gets the shared reading buffer offset for the given sensor type.
  static uint64_t GetOffset(mojom::SensorType type);
};

}  // namespace device

#endif  // DEVICE_GENERIC_SENSOR_PUBLIC_CPP_SENSOR_READING_H_
