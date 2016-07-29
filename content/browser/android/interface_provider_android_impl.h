// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ANDROID_INTERFACE_PROVIDER_ANDROID_IMPL_H_
#define CONTENT_BROWSER_ANDROID_INTERFACE_PROVIDER_ANDROID_IMPL_H_

#include <jni.h>

#include "base/macros.h"
#include "content/public/browser/android/interface_provider_android.h"

namespace content {

class InterfaceProviderAndroidImpl : public InterfaceProviderAndroid {
 public:
  static bool Register(JNIEnv* env);

  ~InterfaceProviderAndroidImpl() override;

 private:
  friend class InterfaceProviderAndroid;

  // Use InterfaceProviderAndroid::Create() to create an instance.
  InterfaceProviderAndroidImpl(shell::InterfaceProvider* remote_interfaces);

  // InterfaceProviderAndroid implementation:
  void GetInterface(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& j_interface_provider,
      const base::android::JavaParamRef<jstring>& j_name,
      jint handle) override;
  const base::android::ScopedJavaGlobalRef<jobject>& GetObj() override;

  shell::InterfaceProvider* remote_interfaces_ = nullptr;
  base::android::ScopedJavaGlobalRef<jobject> obj_;

  DISALLOW_COPY_AND_ASSIGN(InterfaceProviderAndroidImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_ANDROID_INTERFACE_PROVIDER_ANDROID_IMPL_H_