// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/sensors/sensor_manager_chromeos.h"

#include "chromeos/accelerometer/accelerometer_reader.h"
#include "chromeos/accelerometer/accelerometer_types.h"
#include "device/sensors/device_sensors_consts.h"
#include "ui/gfx/geometry/angle_conversions.h"
#include "ui/gfx/geometry/vector3d_f.h"

namespace device {

SensorManagerChromeOS::SensorManagerChromeOS()
    : motion_buffer_(nullptr), orientation_buffer_(nullptr) {}

SensorManagerChromeOS::~SensorManagerChromeOS() = default;

void SensorManagerChromeOS::StartFetchingDeviceMotionData(
    DeviceMotionHardwareBuffer* buffer) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!motion_buffer_);
  motion_buffer_ = buffer;

  motion_buffer_->seqlock.WriteBegin();
  // The interval between updates is the longer of the rate set on the buffer,
  // and the rate at which AccelerometerReader polls the sensor.
  motion_buffer_->data.interval =
      std::max(kDeviceSensorIntervalMicroseconds / 1000,
               chromeos::AccelerometerReader::kDelayBetweenReadsMs);
  motion_buffer_->seqlock.WriteEnd();

  if (!orientation_buffer_)
    StartObservingAccelerometer();
}

bool SensorManagerChromeOS::StopFetchingDeviceMotionData() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!motion_buffer_)
    return false;

  // Make sure to indicate that the sensor data is no longer available.
  motion_buffer_->seqlock.WriteBegin();
  motion_buffer_->data.all_available_sensors_are_active = false;
  motion_buffer_->seqlock.WriteEnd();

  motion_buffer_ = nullptr;

  if (!orientation_buffer_)
    StopObservingAccelerometer();
  return true;
}

void SensorManagerChromeOS::StartFetchingDeviceOrientationData(
    DeviceOrientationHardwareBuffer* buffer) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!orientation_buffer_);
  orientation_buffer_ = buffer;

  // No compass information, so we cannot provide absolute orientation.
  orientation_buffer_->seqlock.WriteBegin();
  orientation_buffer_->data.absolute = false;
  orientation_buffer_->seqlock.WriteEnd();

  if (!motion_buffer_)
    StartObservingAccelerometer();
}

bool SensorManagerChromeOS::StopFetchingDeviceOrientationData() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!orientation_buffer_)
    return false;
  // Make sure to indicate that the sensor data is no longer available.
  orientation_buffer_->seqlock.WriteBegin();
  orientation_buffer_->data.all_available_sensors_are_active = false;
  orientation_buffer_->seqlock.WriteEnd();
  orientation_buffer_ = nullptr;

  if (!motion_buffer_)
    StopObservingAccelerometer();
  return true;
}

void SensorManagerChromeOS::OnAccelerometerUpdated(
    scoped_refptr<const chromeos::AccelerometerUpdate> update) {
  DCHECK(thread_checker_.CalledOnValidThread());
  chromeos::AccelerometerSource source;
  if (update->has(chromeos::ACCELEROMETER_SOURCE_SCREEN))
    source = chromeos::ACCELEROMETER_SOURCE_SCREEN;
  else if (update->has(chromeos::ACCELEROMETER_SOURCE_ATTACHED_KEYBOARD))
    source = chromeos::ACCELEROMETER_SOURCE_ATTACHED_KEYBOARD;
  else
    return;

  double x = update->get(source).x;
  double y = update->get(source).y;
  double z = update->get(source).z;

  GenerateMotionEvent(x, y, z);
  GenerateOrientationEvent(x, y, z);
}

void SensorManagerChromeOS::StartObservingAccelerometer() {
  chromeos::AccelerometerReader::GetInstance()->AddObserver(this);
}

void SensorManagerChromeOS::StopObservingAccelerometer() {
  chromeos::AccelerometerReader::GetInstance()->RemoveObserver(this);
}

void SensorManagerChromeOS::GenerateMotionEvent(double x, double y, double z) {
  if (!motion_buffer_)
    return;

  motion_buffer_->seqlock.WriteBegin();
  motion_buffer_->data.acceleration_including_gravity_x = x;
  motion_buffer_->data.has_acceleration_including_gravity_x = true;
  motion_buffer_->data.acceleration_including_gravity_y = y;
  motion_buffer_->data.has_acceleration_including_gravity_y = true;
  motion_buffer_->data.acceleration_including_gravity_z = z;
  motion_buffer_->data.has_acceleration_including_gravity_z = true;
  motion_buffer_->data.all_available_sensors_are_active = true;
  motion_buffer_->seqlock.WriteEnd();
}

void SensorManagerChromeOS::GenerateOrientationEvent(double x,
                                                     double y,
                                                     double z) {
  if (!orientation_buffer_)
    return;

  // Create a unit vector for trigonometry
  gfx::Vector3dF data(x, y, z);
  data.Scale(1.0f / data.Length());

  // Transform accelerometer to W3C angles, using the Z-X-Y Eulerangles matrix.
  // x = sin(gamma)
  // y = -cos(gamma) * sin(beta)
  // z = cos(beta) * cos(gamma)
  // With only accelerometer alpha cannot be provided.
  // TODO(timvolodine): crbug.com/765802 Check if cast-to-double is necessary.
  double beta = gfx::RadToDeg(static_cast<double>(atan2(data.y(), data.z())));
  double gamma = gfx::RadToDeg(static_cast<double>(asin(-data.x())));

  // Convert beta and gamma to fit the intervals in the specification. Beta is
  // [-180, 180) and gamma is [-90, 90).
  if (beta >= 180.0f)
    beta = -180.0f;
  if (gamma >= 90.0f)
    gamma = -90.0f;
  orientation_buffer_->seqlock.WriteBegin();
  orientation_buffer_->data.beta = beta;
  orientation_buffer_->data.has_beta = true;
  orientation_buffer_->data.gamma = gamma;
  orientation_buffer_->data.has_gamma = true;
  orientation_buffer_->data.all_available_sensors_are_active = true;
  orientation_buffer_->seqlock.WriteEnd();
}

}  // namespace device
