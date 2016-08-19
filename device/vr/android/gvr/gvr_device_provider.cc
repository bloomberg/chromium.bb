// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/android/gvr/gvr_device_provider.h"

#include "base/android/context_utils.h"
#include "base/android/jni_android.h"
#include "base/android/jni_utils.h"
#include "base/android/scoped_java_ref.h"
#include "device/vr/android/gvr/gvr_device.h"
#include "jni/GvrDeviceProvider_jni.h"

using base::android::AttachCurrentThread;
using base::android::GetApplicationContext;

namespace device {

GvrDeviceProvider::GvrDeviceProvider() : VRDeviceProvider() {
  GvrApiManager::GetInstance()->AddClient(this);
}

GvrDeviceProvider::~GvrDeviceProvider() {
  // TODO: This should eventually be handled by an actual GvrLayout instance in
  // the view tree.
  GvrApiManager::GetInstance()->Shutdown();
  if (!j_device_.is_null()) {
    JNIEnv* env = AttachCurrentThread();
    Java_GvrDeviceProvider_shutdown(env, j_device_.obj());
  }

  GvrApiManager::GetInstance()->RemoveClient(this);
}

void GvrDeviceProvider::GetDevices(std::vector<VRDevice*>* devices) {
  Initialize();

  if (vr_device_)
    devices->push_back(vr_device_.get());
}

void GvrDeviceProvider::Initialize() {
  // TODO: This should eventually be handled by an actual GvrLayout instance in
  // the view tree.
  if (j_device_.is_null()) {
    JNIEnv* env = AttachCurrentThread();

    j_device_.Reset(
        Java_GvrDeviceProvider_create(env, GetApplicationContext()));
    jlong gvr_api =
        Java_GvrDeviceProvider_getNativeContext(env, j_device_.obj());

    if (!gvr_api)
      return;

    GvrApiManager::GetInstance()->Initialize(
        reinterpret_cast<gvr_context*>(gvr_api));
  }
}

void GvrDeviceProvider::OnGvrApiInitialized(gvr::GvrApi* gvr_api) {
  if (!vr_device_)
    vr_device_.reset(new GvrDevice(this, gvr_api));

  // Should fire a vrdisplayconnected event here.
}

void GvrDeviceProvider::OnGvrApiShutdown() {
  // Nothing to do here just yet. Eventually want to shut down the VRDevice
}

}  // namespace device
