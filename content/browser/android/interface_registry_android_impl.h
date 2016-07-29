// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ANDROID_INTERFACE_REGISTRY_ANDROID_IMPL_H_
#define CONTENT_BROWSER_ANDROID_INTERFACE_REGISTRY_ANDROID_IMPL_H_

#include <jni.h>

#include "base/macros.h"
#include "content/public/browser/android/interface_registry_android.h"

namespace content {

class InterfaceRegistryAndroidImpl : public InterfaceRegistryAndroid {
 public:
  static bool Register(JNIEnv* env);

  ~InterfaceRegistryAndroidImpl() override;

 private:
  friend class InterfaceRegistryAndroid;

  // Use InterfaceRegistryAndroid::Create() to create an instance.
  InterfaceRegistryAndroidImpl(shell::InterfaceRegistry* interface_registry);

  // InterfaceRegistryAndroid implementation:
  void AddInterface(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& j_interface_registry,
      const base::android::JavaParamRef<jobject>& j_manager,
      const base::android::JavaParamRef<jobject>& j_factory,
      const base::android::JavaParamRef<jstring>& j_name) override;
  void RemoveInterface(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& j_interface_registry,
      const base::android::JavaParamRef<jstring>& j_name) override;
  const base::android::ScopedJavaGlobalRef<jobject>& GetObj() override;

  shell::InterfaceRegistry* interface_registry_ = nullptr;
  base::android::ScopedJavaGlobalRef<jobject> obj_;

  DISALLOW_COPY_AND_ASSIGN(InterfaceRegistryAndroidImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_ANDROID_INTERFACE_REGISTRY_ANDROID_IMPL_H_
