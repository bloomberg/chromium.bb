// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/sensors/data_fetcher_shared_memory.h"

#include <stdint.h>

#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/single_thread_task_runner.h"
#include "third_party/sudden_motion_sensor/sudden_motion_sensor_mac.h"
#include "ui/gfx/geometry/angle_conversions.h"

namespace device {

const double kMeanGravity = 9.80665;

void FetchMotion(SuddenMotionSensor* sensor,
                 DeviceMotionHardwareBuffer* buffer) {
  DCHECK(sensor);
  DCHECK(buffer);

  float axis_value[3];
  if (!sensor->ReadSensorValues(axis_value))
    return;

  buffer->seqlock.WriteBegin();
  buffer->data.acceleration_including_gravity_x = axis_value[0] * kMeanGravity;
  buffer->data.has_acceleration_including_gravity_x = true;
  buffer->data.acceleration_including_gravity_y = axis_value[1] * kMeanGravity;
  buffer->data.has_acceleration_including_gravity_y = true;
  buffer->data.acceleration_including_gravity_z = axis_value[2] * kMeanGravity;
  buffer->data.has_acceleration_including_gravity_z = true;
  buffer->data.all_available_sensors_are_active = true;
  buffer->seqlock.WriteEnd();
}

void FetchOrientation(SuddenMotionSensor* sensor,
                      DeviceOrientationHardwareBuffer* buffer) {
  DCHECK(sensor);
  DCHECK(buffer);

  // Retrieve per-axis calibrated values.
  float axis_value[3];
  if (!sensor->ReadSensorValues(axis_value))
    return;

  // Transform the accelerometer values to W3C draft angles.
  //
  // Accelerometer values are just dot products of the sensor axes
  // by the gravity vector 'g' with the result for the z axis inverted.
  //
  // To understand this transformation calculate the 3rd row of the z-x-y
  // Euler angles rotation matrix (because of the 'g' vector, only 3rd row
  // affects to the result). Note that z-x-y matrix means R = Ry * Rx * Rz.
  // Then, assume alpha = 0 and you get this:
  //
  // x_acc = sin(gamma)
  // y_acc = - cos(gamma) * sin(beta)
  // z_acc = cos(beta) * cos(gamma)
  //
  // After that the rest is just a bit of trigonometry.
  //
  // Also note that alpha can't be provided but it's assumed to be always zero.
  // This is necessary in order to provide enough information to solve
  // the equations.
  //
  // TODO(timvolodine): crbug.com/765802 Check if cast-to-double is necessary.
  double beta =
      gfx::RadToDeg(static_cast<double>(atan2(-axis_value[1], axis_value[2])));
  double gamma = gfx::RadToDeg(static_cast<double>(asin(axis_value[0])));

  // Make sure that the interval boundaries comply with the specification. At
  // this point, beta is [-180, 180] and gamma is [-90, 90], but the spec has
  // the upper bound open on both.
  if (beta == 180.0)
    beta = -180;  // -180 == 180 (upside-down)
  if (gamma == 90.0)
    gamma = nextafter(90, 0);

  // At this point, DCHECKing is paranoia. Never hurts.
  DCHECK_GE(beta, -180.0);
  DCHECK_LT(beta, 180.0);
  DCHECK_GE(gamma, -90.0);
  DCHECK_LT(gamma, 90.0);

  buffer->seqlock.WriteBegin();
  buffer->data.beta = beta;
  buffer->data.has_beta = true;
  buffer->data.gamma = gamma;
  buffer->data.has_gamma = true;
  buffer->data.all_available_sensors_are_active = true;
  buffer->seqlock.WriteEnd();
}

DataFetcherSharedMemory::DataFetcherSharedMemory() {}

DataFetcherSharedMemory::~DataFetcherSharedMemory() {}

void DataFetcherSharedMemory::Fetch(unsigned consumer_bitmask) {
  DCHECK(GetPollingMessageLoop()->task_runner()->BelongsToCurrentThread());
  DCHECK(consumer_bitmask & CONSUMER_TYPE_ORIENTATION ||
         consumer_bitmask & CONSUMER_TYPE_MOTION);

  if (consumer_bitmask & CONSUMER_TYPE_ORIENTATION)
    FetchOrientation(sudden_motion_sensor_.get(), orientation_buffer_);
  if (consumer_bitmask & CONSUMER_TYPE_MOTION)
    FetchMotion(sudden_motion_sensor_.get(), motion_buffer_);
}

DataFetcherSharedMemory::FetcherType DataFetcherSharedMemory::GetType() const {
  return FETCHER_TYPE_POLLING_CALLBACK;
}

bool DataFetcherSharedMemory::Start(ConsumerType consumer_type, void* buffer) {
  DCHECK(GetPollingMessageLoop()->task_runner()->BelongsToCurrentThread());
  DCHECK(buffer);

  switch (consumer_type) {
    case CONSUMER_TYPE_MOTION: {
      if (!sudden_motion_sensor_)
        sudden_motion_sensor_.reset(SuddenMotionSensor::Create());
      bool sudden_motion_sensor_available =
          sudden_motion_sensor_.get() != nullptr;

      motion_buffer_ = static_cast<DeviceMotionHardwareBuffer*>(buffer);
      UMA_HISTOGRAM_BOOLEAN("InertialSensor.MotionMacAvailable",
                            sudden_motion_sensor_available);
      if (!sudden_motion_sensor_available) {
        // No motion sensor available, fire an all-null event.
        motion_buffer_->seqlock.WriteBegin();
        motion_buffer_->data.all_available_sensors_are_active = true;
        motion_buffer_->seqlock.WriteEnd();
      }
      return sudden_motion_sensor_available;
    }
    case CONSUMER_TYPE_ORIENTATION: {
      if (!sudden_motion_sensor_)
        sudden_motion_sensor_.reset(SuddenMotionSensor::Create());
      bool sudden_motion_sensor_available =
          sudden_motion_sensor_.get() != nullptr;

      orientation_buffer_ =
          static_cast<DeviceOrientationHardwareBuffer*>(buffer);
      UMA_HISTOGRAM_BOOLEAN("InertialSensor.OrientationMacAvailable",
                            sudden_motion_sensor_available);
      if (sudden_motion_sensor_available) {
        // On Mac we cannot provide absolute orientation.
        orientation_buffer_->seqlock.WriteBegin();
        orientation_buffer_->data.absolute = false;
        orientation_buffer_->seqlock.WriteEnd();
      } else {
        // No motion sensor available, fire an all-null event.
        orientation_buffer_->seqlock.WriteBegin();
        orientation_buffer_->data.all_available_sensors_are_active = true;
        orientation_buffer_->seqlock.WriteEnd();
      }
      return sudden_motion_sensor_available;
    }
    case CONSUMER_TYPE_ORIENTATION_ABSOLUTE: {
      orientation_absolute_buffer_ =
          static_cast<DeviceOrientationHardwareBuffer*>(buffer);
      // Absolute device orientation not available on Mac, let the
      // implementation fire an all-null event to signal this to blink.
      orientation_absolute_buffer_->seqlock.WriteBegin();
      orientation_absolute_buffer_->data.absolute = true;
      orientation_absolute_buffer_->data.all_available_sensors_are_active =
          true;
      orientation_absolute_buffer_->seqlock.WriteEnd();
      return false;
    }
    default:
      NOTREACHED();
  }
  return false;
}

bool DataFetcherSharedMemory::Stop(ConsumerType consumer_type) {
  DCHECK(GetPollingMessageLoop()->task_runner()->BelongsToCurrentThread());

  switch (consumer_type) {
    case CONSUMER_TYPE_MOTION:
      if (motion_buffer_) {
        motion_buffer_->seqlock.WriteBegin();
        motion_buffer_->data.all_available_sensors_are_active = false;
        motion_buffer_->seqlock.WriteEnd();
        motion_buffer_ = nullptr;
      }
      return true;
    case CONSUMER_TYPE_ORIENTATION:
      if (orientation_buffer_) {
        orientation_buffer_->seqlock.WriteBegin();
        orientation_buffer_->data.all_available_sensors_are_active = false;
        orientation_buffer_->seqlock.WriteEnd();
        orientation_buffer_ = nullptr;
      }
      return true;
    case CONSUMER_TYPE_ORIENTATION_ABSOLUTE:
      if (orientation_absolute_buffer_) {
        orientation_absolute_buffer_->seqlock.WriteBegin();
        orientation_absolute_buffer_->data.all_available_sensors_are_active =
            false;
        orientation_absolute_buffer_->seqlock.WriteEnd();
        orientation_absolute_buffer_ = nullptr;
      }
      return true;
    default:
      NOTREACHED();
  }
  return false;
}

}  // namespace device
