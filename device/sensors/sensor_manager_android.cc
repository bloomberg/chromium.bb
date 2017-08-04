// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/sensors/sensor_manager_android.h"

#include <string.h>

#include "base/android/jni_android.h"
#include "base/bind.h"
#include "base/memory/singleton.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/histogram_macros.h"
#include "jni/DeviceSensors_jni.h"

using base::android::AttachCurrentThread;
using base::android::JavaParamRef;

namespace {

void UpdateDeviceOrientationHistogram(
    device::SensorManagerAndroid::OrientationSensorType type) {
  UMA_HISTOGRAM_ENUMERATION(
      "InertialSensor.DeviceOrientationSensorAndroid", type,
      device::SensorManagerAndroid::ORIENTATION_SENSOR_MAX);
}

void SetOrientation(device::DeviceOrientationHardwareBuffer* buffer,
                    double alpha,
                    double beta,
                    double gamma) {
  buffer->seqlock.WriteBegin();
  buffer->data.alpha = alpha;
  buffer->data.has_alpha = true;
  buffer->data.beta = beta;
  buffer->data.has_beta = true;
  buffer->data.gamma = gamma;
  buffer->data.has_gamma = true;
  buffer->seqlock.WriteEnd();
}

void SetOrientationBufferStatus(device::DeviceOrientationHardwareBuffer* buffer,
                                bool ready,
                                bool absolute) {
  buffer->seqlock.WriteBegin();
  buffer->data.absolute = absolute;
  buffer->data.all_available_sensors_are_active = ready;
  buffer->seqlock.WriteEnd();
}

}  // namespace

namespace device {

SensorManagerAndroid::SensorManagerAndroid()
    : number_active_device_motion_sensors_(0),
      device_motion_buffer_(nullptr),
      device_orientation_buffer_(nullptr),
      motion_buffer_initialized_(false),
      orientation_buffer_initialized_(false),
      is_shutdown_(false) {
  DCHECK(thread_checker_.CalledOnValidThread());
  memset(received_motion_data_, 0, sizeof(received_motion_data_));
  device_sensors_.Reset(Java_DeviceSensors_create(AttachCurrentThread()));
}

SensorManagerAndroid::~SensorManagerAndroid() {}

SensorManagerAndroid* SensorManagerAndroid::GetInstance() {
  DCHECK(base::MessageLoopForUI::IsCurrent());
  return base::Singleton<
      SensorManagerAndroid,
      base::LeakySingletonTraits<SensorManagerAndroid>>::get();
}

void SensorManagerAndroid::GotOrientation(JNIEnv*,
                                          const JavaParamRef<jobject>&,
                                          double alpha,
                                          double beta,
                                          double gamma) {
  base::AutoLock autolock(orientation_buffer_lock_);

  if (!device_orientation_buffer_)
    return;

  SetOrientation(device_orientation_buffer_, alpha, beta, gamma);

  if (!orientation_buffer_initialized_) {
    OrientationSensorType type =
        static_cast<OrientationSensorType>(GetOrientationSensorTypeUsed());
    SetOrientationBufferStatus(device_orientation_buffer_, true,
                               type != GAME_ROTATION_VECTOR);
    orientation_buffer_initialized_ = true;
    UpdateDeviceOrientationHistogram(type);
  }
}

void SensorManagerAndroid::GotOrientationAbsolute(JNIEnv*,
                                                  const JavaParamRef<jobject>&,
                                                  double alpha,
                                                  double beta,
                                                  double gamma) {
  base::AutoLock autolock(orientation_absolute_buffer_lock_);

  if (!device_orientation_absolute_buffer_)
    return;

  SetOrientation(device_orientation_absolute_buffer_, alpha, beta, gamma);

  if (!orientation_absolute_buffer_initialized_) {
    SetOrientationBufferStatus(device_orientation_absolute_buffer_, true, true);
    orientation_absolute_buffer_initialized_ = true;
    // TODO(timvolodine): Add UMA.
  }
}

void SensorManagerAndroid::GotAcceleration(JNIEnv*,
                                           const JavaParamRef<jobject>&,
                                           double x,
                                           double y,
                                           double z) {
  base::AutoLock autolock(motion_buffer_lock_);

  if (!device_motion_buffer_)
    return;

  device_motion_buffer_->seqlock.WriteBegin();
  device_motion_buffer_->data.acceleration_x = x;
  device_motion_buffer_->data.has_acceleration_x = true;
  device_motion_buffer_->data.acceleration_y = y;
  device_motion_buffer_->data.has_acceleration_y = true;
  device_motion_buffer_->data.acceleration_z = z;
  device_motion_buffer_->data.has_acceleration_z = true;
  device_motion_buffer_->seqlock.WriteEnd();

  if (!motion_buffer_initialized_) {
    received_motion_data_[RECEIVED_MOTION_DATA_ACCELERATION] = 1;
    CheckMotionBufferReadyToRead();
  }
}

void SensorManagerAndroid::GotAccelerationIncludingGravity(
    JNIEnv*,
    const JavaParamRef<jobject>&,
    double x,
    double y,
    double z) {
  base::AutoLock autolock(motion_buffer_lock_);

  if (!device_motion_buffer_)
    return;

  device_motion_buffer_->seqlock.WriteBegin();
  device_motion_buffer_->data.acceleration_including_gravity_x = x;
  device_motion_buffer_->data.has_acceleration_including_gravity_x = true;
  device_motion_buffer_->data.acceleration_including_gravity_y = y;
  device_motion_buffer_->data.has_acceleration_including_gravity_y = true;
  device_motion_buffer_->data.acceleration_including_gravity_z = z;
  device_motion_buffer_->data.has_acceleration_including_gravity_z = true;
  device_motion_buffer_->seqlock.WriteEnd();

  if (!motion_buffer_initialized_) {
    received_motion_data_[RECEIVED_MOTION_DATA_ACCELERATION_INCL_GRAVITY] = 1;
    CheckMotionBufferReadyToRead();
  }
}

void SensorManagerAndroid::GotRotationRate(JNIEnv*,
                                           const JavaParamRef<jobject>&,
                                           double alpha,
                                           double beta,
                                           double gamma) {
  base::AutoLock autolock(motion_buffer_lock_);

  if (!device_motion_buffer_)
    return;

  device_motion_buffer_->seqlock.WriteBegin();
  device_motion_buffer_->data.rotation_rate_alpha = alpha;
  device_motion_buffer_->data.has_rotation_rate_alpha = true;
  device_motion_buffer_->data.rotation_rate_beta = beta;
  device_motion_buffer_->data.has_rotation_rate_beta = true;
  device_motion_buffer_->data.rotation_rate_gamma = gamma;
  device_motion_buffer_->data.has_rotation_rate_gamma = true;
  device_motion_buffer_->seqlock.WriteEnd();

  if (!motion_buffer_initialized_) {
    received_motion_data_[RECEIVED_MOTION_DATA_ROTATION_RATE] = 1;
    CheckMotionBufferReadyToRead();
  }
}

bool SensorManagerAndroid::Start(ConsumerType consumer_type) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!device_sensors_.is_null());
  return Java_DeviceSensors_start(
      AttachCurrentThread(), device_sensors_, reinterpret_cast<intptr_t>(this),
      static_cast<jint>(consumer_type), kDeviceSensorIntervalMicroseconds);
}

void SensorManagerAndroid::Stop(ConsumerType consumer_type) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!device_sensors_.is_null());
  Java_DeviceSensors_stop(AttachCurrentThread(), device_sensors_,
                          static_cast<jint>(consumer_type));
}

int SensorManagerAndroid::GetNumberActiveDeviceMotionSensors() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!device_sensors_.is_null());
  return Java_DeviceSensors_getNumberActiveDeviceMotionSensors(
      AttachCurrentThread(), device_sensors_);
}

SensorManagerAndroid::OrientationSensorType
SensorManagerAndroid::GetOrientationSensorTypeUsed() {
  DCHECK(!device_sensors_.is_null());
  return static_cast<SensorManagerAndroid::OrientationSensorType>(
      Java_DeviceSensors_getOrientationSensorTypeUsed(AttachCurrentThread(),
                                                      device_sensors_));
}

// ----- Shared memory API methods

// --- Device Motion

void SensorManagerAndroid::StartFetchingDeviceMotionData(
    DeviceMotionHardwareBuffer* buffer) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(buffer);
  if (is_shutdown_)
    return;

  {
    base::AutoLock autolock(motion_buffer_lock_);
    device_motion_buffer_ = buffer;
    ClearInternalMotionBuffers();
  }
  Start(CONSUMER_TYPE_MOTION);

  // If no motion data can ever be provided, the number of active device motion
  // sensors will be zero. In that case flag the shared memory buffer
  // as ready to read, as it will not change anyway.
  number_active_device_motion_sensors_ = GetNumberActiveDeviceMotionSensors();
  {
    base::AutoLock autolock(motion_buffer_lock_);
    CheckMotionBufferReadyToRead();
  }
}

void SensorManagerAndroid::StopFetchingDeviceMotionData() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (is_shutdown_)
    return;

  Stop(CONSUMER_TYPE_MOTION);
  {
    base::AutoLock autolock(motion_buffer_lock_);
    if (device_motion_buffer_) {
      ClearInternalMotionBuffers();
      device_motion_buffer_ = nullptr;
    }
  }
}

void SensorManagerAndroid::CheckMotionBufferReadyToRead() {
  if (received_motion_data_[RECEIVED_MOTION_DATA_ACCELERATION] +
          received_motion_data_
              [RECEIVED_MOTION_DATA_ACCELERATION_INCL_GRAVITY] +
          received_motion_data_[RECEIVED_MOTION_DATA_ROTATION_RATE] ==
      number_active_device_motion_sensors_) {
    device_motion_buffer_->seqlock.WriteBegin();
    device_motion_buffer_->data.interval =
        kDeviceSensorIntervalMicroseconds / 1000.;
    device_motion_buffer_->seqlock.WriteEnd();
    SetMotionBufferReadyStatus(true);

    UMA_HISTOGRAM_BOOLEAN(
        "InertialSensor.AccelerometerAndroidAvailable",
        received_motion_data_[RECEIVED_MOTION_DATA_ACCELERATION] > 0);
    UMA_HISTOGRAM_BOOLEAN(
        "InertialSensor.AccelerometerIncGravityAndroidAvailable",
        received_motion_data_[RECEIVED_MOTION_DATA_ACCELERATION_INCL_GRAVITY] >
            0);
    UMA_HISTOGRAM_BOOLEAN(
        "InertialSensor.GyroscopeAndroidAvailable",
        received_motion_data_[RECEIVED_MOTION_DATA_ROTATION_RATE] > 0);
  }
}

void SensorManagerAndroid::SetMotionBufferReadyStatus(bool ready) {
  device_motion_buffer_->seqlock.WriteBegin();
  device_motion_buffer_->data.all_available_sensors_are_active = ready;
  device_motion_buffer_->seqlock.WriteEnd();
  motion_buffer_initialized_ = ready;
}

void SensorManagerAndroid::ClearInternalMotionBuffers() {
  memset(received_motion_data_, 0, sizeof(received_motion_data_));
  number_active_device_motion_sensors_ = 0;
  SetMotionBufferReadyStatus(false);
}

// --- Device Orientation

void SensorManagerAndroid::StartFetchingDeviceOrientationData(
    DeviceOrientationHardwareBuffer* buffer) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(buffer);
  if (is_shutdown_)
    return;

  {
    base::AutoLock autolock(orientation_buffer_lock_);
    device_orientation_buffer_ = buffer;
  }
  bool success = Start(CONSUMER_TYPE_ORIENTATION);

  {
    base::AutoLock autolock(orientation_buffer_lock_);
    // If Start() was unsuccessful then set the buffer ready flag to true
    // to start firing all-null events.
    SetOrientationBufferStatus(buffer, !success /* ready */,
                               false /* absolute */);
    orientation_buffer_initialized_ = !success;
  }

  if (!success)
    UpdateDeviceOrientationHistogram(NOT_AVAILABLE);
}

void SensorManagerAndroid::StopFetchingDeviceOrientationData() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (is_shutdown_)
    return;

  Stop(CONSUMER_TYPE_ORIENTATION);
  {
    base::AutoLock autolock(orientation_buffer_lock_);
    if (device_orientation_buffer_) {
      SetOrientationBufferStatus(device_orientation_buffer_, false, false);
      orientation_buffer_initialized_ = false;
      device_orientation_buffer_ = nullptr;
    }
  }
}

void SensorManagerAndroid::StartFetchingDeviceOrientationAbsoluteData(
    DeviceOrientationHardwareBuffer* buffer) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(buffer);
  if (is_shutdown_)
    return;

  {
    base::AutoLock autolock(orientation_absolute_buffer_lock_);
    device_orientation_absolute_buffer_ = buffer;
  }
  bool success = Start(CONSUMER_TYPE_ORIENTATION_ABSOLUTE);

  {
    base::AutoLock autolock(orientation_absolute_buffer_lock_);
    // If Start() was unsuccessful then set the buffer ready flag to true
    // to start firing all-null events.
    SetOrientationBufferStatus(buffer, !success /* ready */,
                               false /* absolute */);
    orientation_absolute_buffer_initialized_ = !success;
  }
}

void SensorManagerAndroid::StopFetchingDeviceOrientationAbsoluteData() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (is_shutdown_)
    return;

  Stop(CONSUMER_TYPE_ORIENTATION_ABSOLUTE);
  {
    base::AutoLock autolock(orientation_absolute_buffer_lock_);
    if (device_orientation_absolute_buffer_) {
      SetOrientationBufferStatus(device_orientation_absolute_buffer_, false,
                                 false);
      orientation_absolute_buffer_initialized_ = false;
      device_orientation_absolute_buffer_ = nullptr;
    }
  }
}

void SensorManagerAndroid::Shutdown() {
  DCHECK(thread_checker_.CalledOnValidThread());
  is_shutdown_ = true;
}

}  // namespace device
