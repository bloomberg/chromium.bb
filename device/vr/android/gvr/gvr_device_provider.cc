// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/android/gvr/gvr_device_provider.h"

#include <jni.h>

#include "base/android/context_utils.h"
#include "base/android/jni_android.h"
#include "base/android/jni_utils.h"
#include "base/android/scoped_java_ref.h"
#include "device/vr/android/gvr/gvr_delegate.h"
#include "device/vr/android/gvr/gvr_device.h"
#include "device/vr/android/gvr/gvr_gamepad_data_fetcher.h"
#include "device/vr/vr_device_manager.h"
#include "jni/GvrDeviceProvider_jni.h"
#include "third_party/gvr-android-sdk/src/ndk/include/vr/gvr/capi/include/gvr.h"
#include "third_party/gvr-android-sdk/src/ndk/include/vr/gvr/capi/include/gvr_controller.h"
#include "third_party/gvr-android-sdk/src/ndk/include/vr/gvr/capi/include/gvr_types.h"

using base::android::AttachCurrentThread;
using base::android::GetApplicationContext;

namespace device {

// A temporary delegate till a VrShell instance becomes available.
class GvrNonPresentingDelegate : public GvrDelegate {
 public:
  GvrNonPresentingDelegate() { Initialize(); }

  virtual ~GvrNonPresentingDelegate() { Shutdown(); }

  void Initialize() {
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
    }
  }

  void Shutdown() {
    if (!j_device_.is_null()) {
      gvr_api_ = nullptr;
      JNIEnv* env = AttachCurrentThread();
      Java_GvrDeviceProvider_shutdown(env, j_device_.obj());
      j_device_ = nullptr;
    }
  }

  // GvrDelegate implementation
  void SetWebVRSecureOrigin(bool secure_origin) override {}
  void SubmitWebVRFrame() override {}
  void UpdateWebVRTextureBounds(int eye,
                                float left,
                                float top,
                                float width,
                                float height) override {}
  void SetGvrPoseForWebVr(const gvr::Mat4f& pose,
                          uint32_t pose_index) override {}

  gvr::GvrApi* gvr_api() override { return gvr_api_.get(); }

 private:
  base::android::ScopedJavaGlobalRef<jobject> j_device_;
  std::unique_ptr<gvr::GvrApi> gvr_api_;
};

GvrDeviceProvider::GvrDeviceProvider()
    : VRDeviceProvider(),
      main_thread_task_runner_(base::ThreadTaskRunnerHandle::Get()) {}

GvrDeviceProvider::~GvrDeviceProvider() {
  ExitPresent();
}

void GvrDeviceProvider::GetDevices(std::vector<VRDevice*>* devices) {
  Initialize();

  if (vr_device_)
    devices->push_back(vr_device_.get());
}

void GvrDeviceProvider::SetClient(VRClientDispatcher* client) {
  if (!client_)
    client_.reset(client);
}

void GvrDeviceProvider::Initialize() {
  if (!vr_device_) {
    vr_device_.reset(new GvrDevice(this, nullptr));
    client_->OnDeviceConnectionStatusChanged(vr_device_.get(), true);
  }
}

bool GvrDeviceProvider::RequestPresent() {
  GvrDelegateProvider* delegate_provider = GvrDelegateProvider::GetInstance();
  if (!delegate_provider)
    return false;

  return delegate_provider->RequestWebVRPresent(this);
}

void GvrDeviceProvider::ExitPresent() {
  DCHECK(main_thread_task_runner_->BelongsToCurrentThread());

  if (!vr_device_)
    return;

  vr_device_->SetDelegate(nullptr);

  GamepadDataFetcherManager::GetInstance()->RemoveSourceFactory(
      GAMEPAD_SOURCE_GVR);

  GvrDelegateProvider* delegate_provider = GvrDelegateProvider::GetInstance();
  if (delegate_provider)
    delegate_provider->ExitWebVRPresent();

  if (client_)
    client_->OnPresentEnded(vr_device_.get());
}

void GvrDeviceProvider::OnGvrDelegateReady(GvrDelegate* delegate) {
  main_thread_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&GvrDeviceProvider::GvrDelegateReady, base::Unretained(this),
                 base::Unretained(delegate)));
}

void GvrDeviceProvider::OnGvrDelegateRemoved() {
  if (!vr_device_)
    return;

  main_thread_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&GvrDeviceProvider::ExitPresent, base::Unretained(this)));
}

void GvrDeviceProvider::GvrDelegateReady(GvrDelegate* delegate) {
  DCHECK(main_thread_task_runner_->BelongsToCurrentThread());

  vr_device_->SetDelegate(delegate);
  GamepadDataFetcherManager::GetInstance()->AddFactory(
      new GvrGamepadDataFetcher::Factory(delegate, vr_device_->id()));
}

}  // namespace device
