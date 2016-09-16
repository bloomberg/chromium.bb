// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_GENERIC_SENSOR_PLATFORM_SENSOR_ANDROID_H_
#define DEVICE_GENERIC_SENSOR_PLATFORM_SENSOR_ANDROID_H_

#include "device/generic_sensor/platform_sensor.h"
#include "base/android/scoped_java_ref.h"
#include "base/memory/ref_counted.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace device {

class PlatformSensorAndroid : public PlatformSensor {
 public:
  // Register C++ methods exposed to Java using JNI.
  static bool RegisterJNI(JNIEnv* env);

  PlatformSensorAndroid(mojom::SensorType type,
                        mojo::ScopedSharedBufferMapping mapping,
                        uint64_t buffer_size,
                        PlatformSensorProvider* provider,
                        const base::android::JavaRef<jobject>& java_sensor);

  mojom::ReportingMode GetReportingMode() override;
  PlatformSensorConfiguration GetDefaultConfiguration() override;

  void NotifyPlatformSensorReadingChanged(
      JNIEnv*,
      const base::android::JavaRef<jobject>& caller);
  void NotifyPlatformSensorError(JNIEnv*,
                                 const base::android::JavaRef<jobject>& caller);

 protected:
  ~PlatformSensorAndroid() override;
  bool StartSensor(const PlatformSensorConfiguration& configuration) override;
  void StopSensor() override;
  bool CheckSensorConfiguration(
      const PlatformSensorConfiguration& configuration) override;

 private:
  // Java object org.chromium.device.sensors.PlatformSensor
  base::android::ScopedJavaGlobalRef<jobject> j_object_;
  // Task runner that is used by mojo objects for the IPC. Android sensor
  // objects share separate handler thread that processes sensor
  // events. Notifications from Java side are forwarded to |task_runner_|.
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  DISALLOW_COPY_AND_ASSIGN(PlatformSensorAndroid);
};

}  // namespace device

#endif  // DEVICE_GENERIC_SENSOR_PLATFORM_SENSOR_ANDROID_H_
