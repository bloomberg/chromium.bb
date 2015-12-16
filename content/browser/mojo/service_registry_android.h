// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MOJO_SERVICE_REGISTRY_ANDROID_H_
#define CONTENT_BROWSER_MOJO_SERVICE_REGISTRY_ANDROID_H_

#include <jni.h>

#include "base/android/scoped_java_ref.h"
#include "base/macros.h"
#include "content/common/content_export.h"

namespace content {

class ServiceRegistryImpl;

// Android wrapper over ServiceRegistryImpl, allowing the browser services in
// Java to register with ServiceRegistry.java (and abstracting away the JNI
// calls).
class CONTENT_EXPORT ServiceRegistryAndroid {
 public:
  static bool Register(JNIEnv* env);

  explicit ServiceRegistryAndroid(ServiceRegistryImpl* service_registry);
  virtual ~ServiceRegistryAndroid();

  // Methods called from Java.
  void AddService(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& j_service_registry,
      const base::android::JavaParamRef<jobject>& j_manager,
      const base::android::JavaParamRef<jobject>& j_factory,
      const base::android::JavaParamRef<jstring>& j_name);
  void RemoveService(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& j_service_registry,
      const base::android::JavaParamRef<jstring>& j_name);
  void ConnectToRemoteService(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& j_service_registry,
      const base::android::JavaParamRef<jstring>& j_name,
      jint handle);

  const base::android::ScopedJavaGlobalRef<jobject>& GetObj() { return obj_; }

 private:
  ServiceRegistryImpl* service_registry_;
  base::android::ScopedJavaGlobalRef<jobject> obj_;

  DISALLOW_COPY_AND_ASSIGN(ServiceRegistryAndroid);
};

}  // namespace content

#endif  // CONTENT_BROWSER_MOJO_SERVICE_REGISTRY_ANDROID_H_
