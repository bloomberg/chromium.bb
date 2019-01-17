// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/navigation_handle_proxy.h"

#include "content/public/browser/navigation_handle.h"
#include "jni/NavigationHandleProxy_jni.h"

namespace content {

// Called from Java.
void NavigationHandleProxy::SetRequestHeader(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    const base::android::JavaParamRef<jstring>& name,
    const base::android::JavaParamRef<jstring>& value) {
  navigation_handle_->SetRequestHeader(
      base::android::ConvertJavaStringToUTF8(name),
      base::android::ConvertJavaStringToUTF8(value));
}

// Called from Java.
void NavigationHandleProxy::RemoveRequestHeader(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    const base::android::JavaParamRef<jstring>& name) {
  navigation_handle_->RemoveRequestHeader(
      base::android::ConvertJavaStringToUTF8(name));
}

}  // namespace content
