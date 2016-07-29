// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_ANDROID_INTERFACE_PROVIDER_ANDROID_H_
#define CONTENT_PUBLIC_BROWSER_ANDROID_INTERFACE_PROVIDER_ANDROID_H_

#include <jni.h>
#include <memory>

#include "base/android/scoped_java_ref.h"
#include "content/common/content_export.h"

namespace shell {
class InterfaceProvider;
}

namespace content {

// Android wrapper around shell::InterfaceProvider, allowing code in Java to
// request interfaces implemented in other environments.
class CONTENT_EXPORT InterfaceProviderAndroid {
 public:
  virtual ~InterfaceProviderAndroid() {}

  // The |remote_interfaces| parameter must outlive |InterfaceProviderAndroid|.
  static std::unique_ptr<InterfaceProviderAndroid> Create(
      shell::InterfaceProvider* remote_interfaces);

  // Called from Java.
  virtual void GetInterface(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& j_interface_provider,
      const base::android::JavaParamRef<jstring>& j_name,
      jint handle) = 0;

  // Accessor to the Java object.
  virtual const base::android::ScopedJavaGlobalRef<jobject>& GetObj() = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_ANDROID_INTERFACE_PROVIDER_ANDROID_H_
