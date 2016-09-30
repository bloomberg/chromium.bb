// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/generic_sensor/platform_sensor_android.h"

#include "base/android/context_utils.h"
#include "base/bind.h"
#include "base/threading/thread_task_runner_handle.h"
#include "jni/PlatformSensor_jni.h"

using base::android::AttachCurrentThread;
using base::android::JavaRef;

namespace device {

// static
bool PlatformSensorAndroid::RegisterJNI(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

PlatformSensorAndroid::PlatformSensorAndroid(
    mojom::SensorType type,
    mojo::ScopedSharedBufferMapping mapping,
    uint64_t buffer_size,
    PlatformSensorProvider* provider,
    const JavaRef<jobject>& java_sensor)
    : PlatformSensor(type, std::move(mapping), provider),
      task_runner_(base::ThreadTaskRunnerHandle::Get()) {
  JNIEnv* env = AttachCurrentThread();
  j_object_.Reset(java_sensor);

  jobject byte_buffer =
      env->NewDirectByteBuffer(shared_buffer_mapping_.get(), buffer_size);
  Java_PlatformSensor_initPlatformSensorAndroid(
      env, j_object_.obj(), reinterpret_cast<jlong>(this), byte_buffer);
}

PlatformSensorAndroid::~PlatformSensorAndroid() {
  StopSensor();
}

mojom::ReportingMode PlatformSensorAndroid::GetReportingMode() {
  JNIEnv* env = AttachCurrentThread();
  return static_cast<mojom::ReportingMode>(
      Java_PlatformSensor_getReportingMode(env, j_object_.obj()));
}

PlatformSensorConfiguration PlatformSensorAndroid::GetDefaultConfiguration() {
  JNIEnv* env = AttachCurrentThread();
  jdouble frequency =
      Java_PlatformSensor_getDefaultConfiguration(env, j_object_.obj());
  return PlatformSensorConfiguration(frequency);
}

bool PlatformSensorAndroid::StartSensor(
    const PlatformSensorConfiguration& configuration) {
  JNIEnv* env = AttachCurrentThread();
  return Java_PlatformSensor_startSensor(env, j_object_.obj(),
                                         configuration.frequency());
}

void PlatformSensorAndroid::StopSensor() {
  JNIEnv* env = AttachCurrentThread();
  Java_PlatformSensor_stopSensor(env, j_object_.obj());
}

bool PlatformSensorAndroid::CheckSensorConfiguration(
    const PlatformSensorConfiguration& configuration) {
  JNIEnv* env = AttachCurrentThread();
  return Java_PlatformSensor_checkSensorConfiguration(
      env, j_object_.obj(), configuration.frequency());
}

void PlatformSensorAndroid::NotifyPlatformSensorReadingChanged(
    JNIEnv*,
    const JavaRef<jobject>& caller) {
  task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&PlatformSensorAndroid::NotifySensorReadingChanged, this));
}

void PlatformSensorAndroid::NotifyPlatformSensorError(
    JNIEnv*,
    const JavaRef<jobject>& caller) {
  task_runner_->PostTask(
      FROM_HERE, base::Bind(&PlatformSensorAndroid::NotifySensorError, this));
}

}  // namespace device
