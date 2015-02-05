// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/device_sensors/sensor_manager_chromeos.h"

#include <math.h>

#include "base/logging.h"
#include "chromeos/accelerometer/accelerometer_reader.h"
#include "chromeos/accelerometer/accelerometer_types.h"
#include "ui/gfx/geometry/vector3d_f.h"

namespace {
// Conversion ratio from radians to degrees.
const double kRad2deg = 180.0 / M_PI;
}

namespace content {

SensorManagerChromeOS::SensorManagerChromeOS() : orientation_buffer_(nullptr) {
}

SensorManagerChromeOS::~SensorManagerChromeOS() {
}

bool SensorManagerChromeOS::StartFetchingDeviceOrientationData(
    DeviceOrientationHardwareBuffer* buffer) {
  DCHECK(buffer);
  {
    base::AutoLock autolock(orientation_buffer_lock_);
    if (orientation_buffer_)
      return false;
    orientation_buffer_ = buffer;

    // No compass information, so we cannot provide absolute orientation.
    orientation_buffer_->seqlock.WriteBegin();
    orientation_buffer_->data.absolute = false;
    orientation_buffer_->data.hasAbsolute = true;
    orientation_buffer_->seqlock.WriteEnd();
  }

  StartObservingAccelerometer();
  return true;
}

bool SensorManagerChromeOS::StopFetchingDeviceOrientationData() {
  {
    base::AutoLock autolock(orientation_buffer_lock_);
    if (!orientation_buffer_)
      return false;
    // Make sure to indicate that the sensor data is no longer available.
    orientation_buffer_->seqlock.WriteBegin();
    orientation_buffer_->data.allAvailableSensorsAreActive = false;
    orientation_buffer_->seqlock.WriteEnd();
    orientation_buffer_ = nullptr;
  }

  StopObservingAccelerometer();
  return true;
}

void SensorManagerChromeOS::OnAccelerometerUpdated(
    const chromeos::AccelerometerUpdate& update) {
  base::AutoLock autolock(orientation_buffer_lock_);
  if (!orientation_buffer_)
    return;

  chromeos::AccelerometerSource source;
  if (update.has(chromeos::ACCELEROMETER_SOURCE_SCREEN)) {
    source = chromeos::ACCELEROMETER_SOURCE_SCREEN;
  } else if (update.has(chromeos::ACCELEROMETER_SOURCE_ATTACHED_KEYBOARD)) {
    source = chromeos::ACCELEROMETER_SOURCE_ATTACHED_KEYBOARD;
  } else {
    return;
  }

  double x = update.get(source).x;
  double y = update.get(source).y;
  double z = update.get(source).z;

  // Create a unit vector for trigonometry
  // TODO(jonross): Stop reversing signs for vector components once
  // accelerometer values have been fixed. crbug.com/431391
  // Ternaries are to remove -0.0f which gives incorrect trigonometrical
  // results.
  gfx::Vector3dF data(x, y ? -y : 0.0f, z ? -z : 0.0f);
  data.Scale(1.0f / data.Length());

  // Transform accelerometer to W3C angles, using the Z-X-Y Eulerangles matrix.
  // x = sin(gamma)
  // y = -cos(gamma) * sin(beta)
  // z = cos(beta) * cos(gamma)
  // With only accelerometer alpha cannot be provided.
  double beta = kRad2deg * atan2(data.y(), data.z());
  double gamma = kRad2deg * asin(data.x());

  // Convert beta and gamma to fit the intervals in the specification. Beta is
  // [-180, 180) and gamma is [-90, 90).
  if (beta >= 180.0f)
    beta = -180.0f;
  if (gamma >= 90.0f)
    gamma = -90.0f;
  orientation_buffer_->seqlock.WriteBegin();
  orientation_buffer_->data.beta = beta;
  orientation_buffer_->data.hasBeta = true;
  orientation_buffer_->data.gamma = gamma;
  orientation_buffer_->data.hasGamma = true;
  orientation_buffer_->data.allAvailableSensorsAreActive = true;
  orientation_buffer_->seqlock.WriteEnd();
}

void SensorManagerChromeOS::StartObservingAccelerometer() {
  chromeos::AccelerometerReader::GetInstance()->AddObserver(this);
}

void SensorManagerChromeOS::StopObservingAccelerometer() {
  chromeos::AccelerometerReader::GetInstance()->RemoveObserver(this);
}

}  // namespace content
