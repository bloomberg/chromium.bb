// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/interface_provider_android_impl.h"

#include <utility>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/callback.h"
#include "base/memory/ptr_util.h"
#include "jni/InterfaceProvider_jni.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "services/shell/public/cpp/interface_provider.h"
#include "services/shell/public/cpp/interface_registry.h"

using base::android::AttachCurrentThread;
using base::android::ConvertJavaStringToUTF8;
using base::android::ScopedJavaGlobalRef;

namespace content {

// static
std::unique_ptr<InterfaceProviderAndroid> InterfaceProviderAndroid::Create(
    shell::InterfaceProvider* remote_interfaces) {
  return base::WrapUnique(new InterfaceProviderAndroidImpl(remote_interfaces));
}

// static
bool InterfaceProviderAndroidImpl::Register(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

InterfaceProviderAndroidImpl::~InterfaceProviderAndroidImpl() {
  Java_InterfaceProvider_destroy(AttachCurrentThread(), obj_.obj());
}

// Constructor and destructor call into Java.
InterfaceProviderAndroidImpl::InterfaceProviderAndroidImpl(
    shell::InterfaceProvider* remote_interfaces)
    : remote_interfaces_(remote_interfaces) {
  JNIEnv* env = AttachCurrentThread();
  obj_.Reset(
      env,
      Java_InterfaceProvider_create(
          env, reinterpret_cast<intptr_t>(this)).obj());
}

const base::android::ScopedJavaGlobalRef<jobject>&
InterfaceProviderAndroidImpl::GetObj() {
  return obj_;
}

// Methods called from Java.
void InterfaceProviderAndroidImpl::GetInterface(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_interface_provider,
    const JavaParamRef<jstring>& j_name,
    jint j_handle) {
  std::string name(ConvertJavaStringToUTF8(env, j_name));
  mojo::ScopedMessagePipeHandle handle((mojo::MessagePipeHandle(j_handle)));
  remote_interfaces_->GetInterface(name, std::move(handle));
}

}  // namespace content
