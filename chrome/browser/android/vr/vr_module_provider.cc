// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/vr/vr_module_provider.h"

#include "chrome/browser/android/vr/register_jni.h"
#include "chrome/browser/android/vr/vr_module_provider.h"
#include "device/vr/android/gvr/vr_module_delegate.h"
#include "jni/VrModuleProvider_jni.h"

namespace vr {

VrModuleProvider::VrModuleProvider() = default;

VrModuleProvider::~VrModuleProvider() {
  if (!j_vr_module_provider_.obj()) {
    return;
  }
  Java_VrModuleProvider_onNativeDestroy(base::android::AttachCurrentThread(),
                                        j_vr_module_provider_);
}

bool VrModuleProvider::ModuleInstalled() {
  return Java_VrModuleProvider_isModuleInstalled(
      base::android::AttachCurrentThread());
}

void VrModuleProvider::InstallModule(
    base::OnceCallback<void(bool)> on_finished) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  on_finished_callbacks_.push(std::move(on_finished));
  // Don't request VR module multiple times in parallel.
  if (on_finished_callbacks_.size() > 1) {
    return;
  }
  DCHECK(!j_vr_module_provider_.obj());
  j_vr_module_provider_.Reset(Java_VrModuleProvider_create(
      base::android::AttachCurrentThread(), (jlong) this));
  Java_VrModuleProvider_installModule(base::android::AttachCurrentThread(),
                                      j_vr_module_provider_);
}

void VrModuleProvider::OnInstalledModule(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    bool success) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(on_finished_callbacks_.size() > 0);
  while (!on_finished_callbacks_.empty()) {
    std::move(on_finished_callbacks_.front()).Run(success);
    on_finished_callbacks_.pop();
  }
  j_vr_module_provider_ = nullptr;
}

static void JNI_VrModuleProvider_Init(JNIEnv* env) {
  device::VrModuleDelegate::Set(std::make_unique<VrModuleProvider>());
}

static void JNI_VrModuleProvider_RegisterJni(JNIEnv* env) {
  CHECK(RegisterJni(env));
}

}  // namespace vr
