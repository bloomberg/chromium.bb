// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_ANDROID_INTERFACE_REGISTRY_ANDROID_H_
#define CONTENT_PUBLIC_BROWSER_ANDROID_INTERFACE_REGISTRY_ANDROID_H_

#include <jni.h>
#include <memory>

#include "base/android/scoped_java_ref.h"
#include "content/common/content_export.h"

namespace shell {
class InterfaceRegistry;
}

namespace content {

// Android wrapper around shell::InterfaceRegistry, allowing interfaces
// implemented in Java to bind requests from other environments.
// Abstracts away all the JNI calls.
class CONTENT_EXPORT InterfaceRegistryAndroid {
 public:
  virtual ~InterfaceRegistryAndroid() {}

  // The |interface_registry| parameter must outlive |InterfaceRegistryAndroid|.
  static std::unique_ptr<InterfaceRegistryAndroid> Create(
      shell::InterfaceRegistry* interface_registry);

  // Called from Java.
  virtual void AddInterface(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& j_interface_registry,
      const base::android::JavaParamRef<jobject>& j_manager,
      const base::android::JavaParamRef<jobject>& j_factory,
      const base::android::JavaParamRef<jstring>& j_name) = 0;
  virtual void RemoveInterface(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& j_interface_registry,
      const base::android::JavaParamRef<jstring>& j_name) = 0;

  // Accessor to the Java object.
  virtual const base::android::ScopedJavaGlobalRef<jobject>& GetObj() = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_ANDROID_INTERFACE_REGISTRY_ANDROID_H_
