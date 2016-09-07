// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/android/gvr/gvr_device_provider.h"

#include <jni.h>

#include "base/android/context_utils.h"
#include "base/android/jni_android.h"
#include "base/android/jni_utils.h"
#include "base/android/scoped_java_ref.h"
#include "device/vr/android/gvr/gvr_device.h"
#include "jni/GvrDeviceProvider_jni.h"

using base::android::AttachCurrentThread;
using base::android::GetApplicationContext;

namespace device {

// A temporary delegate till the VrShell is available.
class GvrDeviceProviderDelegate : public GvrDelegate {
 public:
  GvrDeviceProviderDelegate() {
    // TODO: This should eventually be handled by an actual GvrLayout instance
    // in the view tree.
    if (j_device_.is_null()) {
      JNIEnv* env = AttachCurrentThread();

      j_device_.Reset(
          Java_GvrDeviceProvider_create(env, GetApplicationContext()));
      jlong context =
          Java_GvrDeviceProvider_getNativeContext(env, j_device_.obj());

      if (!context)
        return;

      gvr_api_ =
          gvr::GvrApi::WrapNonOwned(reinterpret_cast<gvr_context*>(context));

      GvrDelegateManager::GetInstance()->Initialize(this);
    }
  }

  virtual ~GvrDeviceProviderDelegate() {
    // TODO: This should eventually be handled by an actual GvrLayout instance
    // in the view tree.
    GvrDelegateManager::GetInstance()->Shutdown();
    if (!j_device_.is_null()) {
      JNIEnv* env = AttachCurrentThread();
      Java_GvrDeviceProvider_shutdown(env, j_device_.obj());
    }
  }

  // GvrDelegate implementation
  void RequestWebVRPresent() override {}
  void ExitWebVRPresent() override {}

  void SubmitWebVRFrame() override {}
  void UpdateWebVRTextureBounds(int eye,
                                float left,
                                float top,
                                float width,
                                float height) override {}

  gvr::GvrApi* gvr_api() override { return gvr_api_.get(); }

 private:
  base::android::ScopedJavaGlobalRef<jobject> j_device_;
  std::unique_ptr<gvr::GvrApi> gvr_api_;
};

GvrDeviceProvider::GvrDeviceProvider() : VRDeviceProvider() {
  GvrDelegateManager::GetInstance()->AddClient(this);
}

GvrDeviceProvider::~GvrDeviceProvider() {
  GvrDelegateManager::GetInstance()->RemoveClient(this);
}

void GvrDeviceProvider::GetDevices(std::vector<VRDevice*>* devices) {
  Initialize();

  if (vr_device_)
    devices->push_back(vr_device_.get());
}

void GvrDeviceProvider::Initialize() {
  if (!delegate_) {
    delegate_.reset(new GvrDeviceProviderDelegate());
  }
}

void GvrDeviceProvider::OnDelegateInitialized(GvrDelegate* delegate) {
  if (!vr_device_)
    vr_device_.reset(new GvrDevice(this, delegate));

  // Should fire a vrdisplayconnected event here.
}

void GvrDeviceProvider::OnDelegateShutdown() {
  // Nothing to do here just yet. Eventually want to shut down the VRDevice and
  // fire a vrdisplaydisconnected event.
}

}  // namespace device
