// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVICE_SENSORS_SENSOR_MANAGER_ANDROID_H_
#define CONTENT_BROWSER_DEVICE_SENSORS_SENSOR_MANAGER_ANDROID_H_

#include "base/android/scoped_java_ref.h"
#include "base/memory/scoped_ptr.h"
#include "base/synchronization/lock.h"
#include "content/browser/device_sensors/device_sensors_consts.h"
#include "content/common/content_export.h"
#include "content/common/device_sensors/device_light_hardware_buffer.h"
#include "content/common/device_sensors/device_motion_hardware_buffer.h"
#include "content/common/device_sensors/device_orientation_hardware_buffer.h"

namespace base {
template <typename T>
struct DefaultSingletonTraits;
}

namespace content {

// Android implementation of Device {Motion|Orientation|Light} API.
//
// Android's SensorManager has a push API, so when Got*() methods are called
// by the system the browser process puts the received data into a shared
// memory buffer, which is read by the renderer processes.
class CONTENT_EXPORT SensorManagerAndroid {
 public:
  // Must be called at startup, before GetInstance().
  static bool Register(JNIEnv* env);

  // Needs to be thread-safe, because accessed from different threads.
  static SensorManagerAndroid* GetInstance();

  // Called from Java via JNI.
  void GotLight(JNIEnv*, jobject, double value);
  void GotOrientation(JNIEnv*, jobject,
                      double alpha, double beta, double gamma);
  void GotAcceleration(JNIEnv*, jobject,
                       double x, double y, double z);
  void GotAccelerationIncludingGravity(JNIEnv*, jobject,
                                       double x, double y, double z);
  void GotRotationRate(JNIEnv*, jobject,
                       double alpha, double beta, double gamma);

  // Shared memory related methods.
  bool StartFetchingDeviceLightData(DeviceLightHardwareBuffer* buffer);
  void StopFetchingDeviceLightData();

  bool StartFetchingDeviceMotionData(DeviceMotionHardwareBuffer* buffer);
  void StopFetchingDeviceMotionData();

  bool StartFetchingDeviceOrientationData(
      DeviceOrientationHardwareBuffer* buffer);
  void StopFetchingDeviceOrientationData();

  void Shutdown();

  // A Java counterpart will be generated for this enum.
  // GENERATED_JAVA_ENUM_PACKAGE: org.chromium.content.browser
  // When adding new constants don't modify the order as they are used for UMA.
  enum OrientationSensorType {
    NOT_AVAILABLE = 0,
    ROTATION_VECTOR = 1,
    ACCELEROMETER_MAGNETIC = 2,
    GAME_ROTATION_VECTOR = 3,
    ORIENTATION_SENSOR_MAX = 4,
  };

 protected:
  SensorManagerAndroid();
  virtual ~SensorManagerAndroid();

  virtual bool Start(ConsumerType event_type);
  virtual void Stop(ConsumerType event_type);
  virtual int GetOrientationSensorTypeUsed();
  virtual int GetNumberActiveDeviceMotionSensors();

  void StartFetchingLightDataOnUI(DeviceLightHardwareBuffer* buffer);
  void StopFetchingLightDataOnUI();

  void StartFetchingMotionDataOnUI(DeviceMotionHardwareBuffer* buffer);
  void StopFetchingMotionDataOnUI();

  void StartFetchingOrientationDataOnUI(
      DeviceOrientationHardwareBuffer* buffer);
  void StopFetchingOrientationDataOnUI();

 private:
  friend struct base::DefaultSingletonTraits<SensorManagerAndroid>;

  enum {
    RECEIVED_MOTION_DATA_ACCELERATION = 0,
    RECEIVED_MOTION_DATA_ACCELERATION_INCL_GRAVITY = 1,
    RECEIVED_MOTION_DATA_ROTATION_RATE = 2,
    RECEIVED_MOTION_DATA_MAX = 3,
  };

  void SetLightBufferValue(double lux);

  void CheckMotionBufferReadyToRead();
  void SetMotionBufferReadyStatus(bool ready);
  void ClearInternalMotionBuffers();

  void SetOrientationBufferStatus(bool ready, bool absolute);

  // The Java provider of sensors info.
  base::android::ScopedJavaGlobalRef<jobject> device_sensors_;
  int number_active_device_motion_sensors_;
  int received_motion_data_[RECEIVED_MOTION_DATA_MAX];
  DeviceLightHardwareBuffer* device_light_buffer_;
  DeviceMotionHardwareBuffer* device_motion_buffer_;
  DeviceOrientationHardwareBuffer* device_orientation_buffer_;

  bool motion_buffer_initialized_;
  bool orientation_buffer_initialized_;

  base::Lock light_buffer_lock_;
  base::Lock motion_buffer_lock_;
  base::Lock orientation_buffer_lock_;

  bool is_shutdown_;

  DISALLOW_COPY_AND_ASSIGN(SensorManagerAndroid);
};

}  // namespace content

#endif  // CONTENT_BROWSER_DEVICE_SENSORS_SENSOR_MANAGER_ANDROID_H_
