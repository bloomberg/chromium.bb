// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_android.h"
#include "base/android/library_loader/library_loader_hooks.h"
#include "base/bind.h"
#include "chromecast/android/cast_jni_registrar.h"
#include "chromecast/android/platform_jni_loader.h"
#include "chromecast/app/cast_main_delegate.h"
#include "chromecast/browser/android/jni_registrar.h"
#include "content/public/app/content_jni_onload.h"
#include "content/public/app/content_main.h"
#include "content/public/browser/android/compositor.h"

namespace {

bool RegisterJNI(JNIEnv* env) {
  // To be called only from the UI thread.  If loading the library is done on
  // a separate thread, this should be moved elsewhere.
  if (!chromecast::android::RegisterJni(env))
    return false;
  if (!chromecast::shell::RegisterJni(env))
    return false;
  // Allow platform-specific implementations to perform more JNI registration.
  if (!chromecast::android::PlatformRegisterJni(env))
    return false;
  return true;
}

bool NativeInit() {
  if (!content::android::OnJNIOnLoadInit())
    return false;
  content::Compositor::Initialize();
  content::SetContentMainDelegate(new chromecast::shell::CastMainDelegate);
  return true;
}

}  // namespace

// This is called by the VM when the shared library is first loaded.
JNI_EXPORT jint JNI_OnLoad(JavaVM* vm, void* reserved) {
  base::android::InitVM(vm);
  JNIEnv* env = base::android::AttachCurrentThread();
  if (!content::android::OnJNIOnLoadRegisterJNI(env) || !RegisterJNI(env) ||
      !NativeInit()) {
    return -1;
  }
  return JNI_VERSION_1_4;
}
