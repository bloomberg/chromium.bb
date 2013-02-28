// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/video/capture/android/video_capture_device_android.h"

#include <string>

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/string_number_conversions.h"
#include "base/stringprintf.h"
#include "jni/Camera_jni.h"
#include "jni/VideoCapture_jni.h"
#include "media/base/video_util.h"

using base::android::AttachCurrentThread;
using base::android::CheckException;
using base::android::GetClass;
using base::android::MethodID;
using base::android::JavaRef;
using base::android::ScopedJavaLocalRef;

namespace {

int GetIntField(JNIEnv* env,
                const JavaRef<jclass>& clazz,
                const JavaRef<jobject>& instance,
                const char* field_name) {
  jfieldID field = GetFieldID(env, clazz, field_name, "I");
  jint int_value = env->GetIntField(instance.obj(), field);
  return int_value;
}

}  // namespace

namespace media {

// static
void VideoCaptureDevice::GetDeviceNames(Names* device_names) {
  device_names->clear();

  JNIEnv* env = AttachCurrentThread();

  int num_cameras = JNI_Camera::Java_Camera_getNumberOfCameras(env);
  DVLOG(1) << "VideoCaptureDevice::GetDeviceNames: num_cameras=" << num_cameras;
  if (num_cameras <= 0)
    return;

  // TODO(wjia): switch to using same approach as Camera when
  // jar_file_jni_generator.gypi supports system inner classes.
  std::string camera_info_string("android/hardware/Camera$CameraInfo");

  ScopedJavaLocalRef<jclass> camera_info_class(
      GetClass(env, camera_info_string.c_str()));
  jmethodID constructor = MethodID::Get<MethodID::TYPE_INSTANCE>(
      env, camera_info_class.obj(), "<init>", "()V");

  ScopedJavaLocalRef<jobject> object_camera_info(
      env, env->NewObject(camera_info_class.obj(), constructor));

  jfieldID field_facing = GetFieldID(env, camera_info_class, "facing", "I");
  jfieldID field_facing_front = GetStaticFieldID(
      env, camera_info_class, "CAMERA_FACING_FRONT", "I");

  for (int camera_id = num_cameras - 1; camera_id >= 0; --camera_id) {
    JNI_Camera::Java_Camera_getCameraInfo(
        env, camera_id, object_camera_info.obj());

    Name name;
    name.unique_id = StringPrintf("%d", camera_id);
    std::string facing_string;
    if (env->GetIntField(object_camera_info.obj(), field_facing) ==
        env->GetStaticIntField(camera_info_class.obj(), field_facing_front)) {
      facing_string = "front";
    } else {
      facing_string = "back";
    }
    name.device_name = StringPrintf(
        "camera %d, facing %s", camera_id, facing_string.c_str());
    device_names->push_back(name);
    jfieldID field_orientation = GetFieldID(
        env, camera_info_class, "orientation", "I");
    jint orientation = env->GetIntField(object_camera_info.obj(),
                                        field_orientation);
    DVLOG(1) << "VideoCaptureDevice::GetDeviceNames: camera device_name="
             << name.device_name
             << ", unique_id="
             << name.unique_id
             << ", orientation "
             << orientation;
  }
}

// static
VideoCaptureDevice* VideoCaptureDevice::Create(const Name& device_name) {
  return VideoCaptureDeviceAndroid::Create(device_name);
}

// static
VideoCaptureDevice* VideoCaptureDeviceAndroid::Create(const Name& device_name) {
  scoped_ptr<VideoCaptureDeviceAndroid> ret(
      new VideoCaptureDeviceAndroid(device_name));
  if (ret->Init())
    return ret.release();
  return NULL;
}

// static
bool VideoCaptureDeviceAndroid::RegisterVideoCaptureDevice(JNIEnv* env) {
  return RegisterNativesImpl(env) && JNI_Camera::RegisterNativesImpl(env);
}

VideoCaptureDeviceAndroid::VideoCaptureDeviceAndroid(const Name& device_name)
    : state_(kIdle),
      observer_(NULL),
      device_name_(device_name),
      current_settings_() {
}

VideoCaptureDeviceAndroid::~VideoCaptureDeviceAndroid() {
  DeAllocate();
}

bool VideoCaptureDeviceAndroid::Init() {
  int id;
  if (!base::StringToInt(device_name_.unique_id, &id))
    return false;

  JNIEnv* env = AttachCurrentThread();

  j_capture_.Reset(Java_VideoCapture_createVideoCapture(
      env, base::android::GetApplicationContext(), id,
      reinterpret_cast<jlong>(this)));

  return true;
}

const VideoCaptureDevice::Name& VideoCaptureDeviceAndroid::device_name() {
  return device_name_;
}

void VideoCaptureDeviceAndroid::Allocate(
    int width,
    int height,
    int frame_rate,
    EventHandler* observer) {
  {
    base::AutoLock lock(lock_);
    if (state_ != kIdle)
      return;
    observer_ = observer;
    state_ = kAllocated;
  }

  JNIEnv* env = AttachCurrentThread();

  jboolean ret = Java_VideoCapture_allocate(env,
                                            j_capture_.obj(),
                                            width,
                                            height,
                                            frame_rate);
  if (!ret) {
    SetErrorState("failed to allocate");
    return;
  }

  // Store current width and height.
  current_settings_.width =
      Java_VideoCapture_queryWidth(env, j_capture_.obj());
  current_settings_.height =
      Java_VideoCapture_queryHeight(env, j_capture_.obj());
  current_settings_.frame_rate =
      Java_VideoCapture_queryFrameRate(env, j_capture_.obj());
  current_settings_.color = VideoCaptureCapability::kYV12;
  CHECK(current_settings_.width > 0 && !(current_settings_.width % 2));
  CHECK(current_settings_.height > 0 && !(current_settings_.height % 2));

  DVLOG(1) << "VideoCaptureDeviceAndroid::Allocate: queried width="
           << current_settings_.width
           << ", height="
           << current_settings_.height
           << ", frame_rate="
           << current_settings_.frame_rate;
  // Report the frame size to the observer.
  observer_->OnFrameInfo(current_settings_);
}

void VideoCaptureDeviceAndroid::Start() {
  DVLOG(1) << "VideoCaptureDeviceAndroid::Start";
  {
    base::AutoLock lock(lock_);
    DCHECK_EQ(state_, kAllocated);
  }

  JNIEnv* env = AttachCurrentThread();

  jint ret = Java_VideoCapture_startCapture(env, j_capture_.obj());
  if (ret < 0) {
    SetErrorState("failed to start capture");
    return;
  }

  {
    base::AutoLock lock(lock_);
    state_ = kCapturing;
  }
}

void VideoCaptureDeviceAndroid::Stop() {
  DVLOG(1) << "VideoCaptureDeviceAndroid::Stop";
  {
    base::AutoLock lock(lock_);
    if (state_ != kCapturing && state_ != kError)
      return;
    if (state_ == kCapturing)
      state_ = kAllocated;
  }

  JNIEnv* env = AttachCurrentThread();

  jint ret = Java_VideoCapture_stopCapture(env, j_capture_.obj());
  if (ret < 0) {
    SetErrorState("failed to stop capture");
    return;
  }
}

void VideoCaptureDeviceAndroid::DeAllocate() {
  DVLOG(1) << "VideoCaptureDeviceAndroid::DeAllocate";
  {
    base::AutoLock lock(lock_);
    if (state_ == kIdle)
      return;

    if (state_ == kCapturing) {
      base::AutoUnlock unlock(lock_);
      Stop();
    }

    if (state_ == kAllocated)
      state_ = kIdle;

    observer_ = NULL;
  }

  JNIEnv* env = AttachCurrentThread();

  Java_VideoCapture_deallocate(env, j_capture_.obj());
}

void VideoCaptureDeviceAndroid::OnFrameAvailable(
    JNIEnv* env,
    jobject obj,
    jbyteArray data,
    jint length,
    jint rotation,
    jboolean flip_vert,
    jboolean flip_horiz) {
  DVLOG(3) << "VideoCaptureDeviceAndroid::OnFrameAvailable: length =" << length;

  base::AutoLock lock(lock_);
  if (state_ != kCapturing || !observer_)
    return;

  jbyte* buffer = env->GetByteArrayElements(data, NULL);
  if (!buffer) {
    LOG(ERROR) << "VideoCaptureDeviceAndroid::OnFrameAvailable: "
                  "failed to GetByteArrayElements";
    return;
  }

  observer_->OnIncomingCapturedFrame(
      reinterpret_cast<uint8*>(buffer), length, base::Time::Now(),
      rotation, flip_vert, flip_horiz);

  env->ReleaseByteArrayElements(data, buffer, JNI_ABORT);
}

void VideoCaptureDeviceAndroid::SetErrorState(const std::string& reason) {
  LOG(ERROR) << "VideoCaptureDeviceAndroid::SetErrorState: " << reason;
  {
    base::AutoLock lock(lock_);
    state_ = kError;
  }
  observer_->OnError();
}

}  // namespace media
