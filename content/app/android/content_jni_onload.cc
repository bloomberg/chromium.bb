// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/app/content_jni_onload.h"

#include <vector>

#include "base/android/base_jni_onload.h"
#include "base/android/library_loader/library_loader_hooks.h"
#include "base/bind.h"
#include "content/app/android/library_loader_hooks.h"
#include "content/public/app/content_main.h"

namespace content {
namespace android {

namespace {

bool RegisterJNI(JNIEnv* env) {
  return content::EnsureJniRegistered(env);
}

bool Init() {
  base::android::SetLibraryLoadedHook(&content::LibraryLoaded);
  return true;
}

}  // namespace


bool OnJNIOnLoadRegisterJNI(
    JavaVM* vm,
    std::vector<base::android::RegisterCallback> callbacks) {
  callbacks.push_back(base::Bind(&RegisterJNI));
  return base::android::OnJNIOnLoadRegisterJNI(vm, callbacks);
}

bool OnJNIOnLoadInit(
    std::vector<base::android::InitCallback> callbacks) {
  callbacks.push_back(base::Bind(&Init));
  return base::android::OnJNIOnLoadInit(callbacks);
}

}  // namespace android
}  // namespace content
