// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/mojo/service_registry_android.h"

#include <utility>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/callback.h"
#include "content/common/mojo/service_registry_impl.h"
#include "jni/ServiceRegistry_jni.h"

using base::android::AttachCurrentThread;
using base::android::ConvertJavaStringToUTF8;
using base::android::ScopedJavaGlobalRef;

namespace content {

namespace {

// Callback passed to the wrapped ServiceRegistry upon AddService(). The
// ServiceRegistry will call it to create a registered Java service
void CreateImplAndAttach(
    const ScopedJavaGlobalRef<jobject>& j_scoped_service_registry,
    const ScopedJavaGlobalRef<jobject>& j_scoped_manager,
    const ScopedJavaGlobalRef<jobject>& j_scoped_factory,
    mojo::ScopedMessagePipeHandle handle) {
  JNIEnv* env = AttachCurrentThread();
  Java_ServiceRegistry_createImplAndAttach(env,
                                           j_scoped_service_registry.obj(),
                                           handle.release().value(),
                                           j_scoped_manager.obj(),
                                           j_scoped_factory.obj());
}

}  // namespace

// static
bool ServiceRegistryAndroid::Register(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

// Constructor and destructor call into Java.
ServiceRegistryAndroid::ServiceRegistryAndroid(
    ServiceRegistryImpl* service_registry)
    : service_registry_(service_registry) {
  JNIEnv* env = AttachCurrentThread();
  obj_.Reset(
      env,
      Java_ServiceRegistry_create(env, reinterpret_cast<intptr_t>(this)).obj());
}

ServiceRegistryAndroid::~ServiceRegistryAndroid() {
  Java_ServiceRegistry_destroy(AttachCurrentThread(), obj_.obj());
}

// Methods called from Java.
void ServiceRegistryAndroid::AddService(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_service_registry,
    const JavaParamRef<jobject>& j_manager,
    const JavaParamRef<jobject>& j_factory,
    const JavaParamRef<jstring>& j_name) {
  std::string name(ConvertJavaStringToUTF8(env, j_name));

  ScopedJavaGlobalRef<jobject> j_scoped_service_registry;
  j_scoped_service_registry.Reset(env, j_service_registry);

  ScopedJavaGlobalRef<jobject> j_scoped_manager;
  j_scoped_manager.Reset(env, j_manager);

  ScopedJavaGlobalRef<jobject> j_scoped_factory;
  j_scoped_factory.Reset(env, j_factory);

  service_registry_->AddService(name,
                                base::Bind(&CreateImplAndAttach,
                                           j_scoped_service_registry,
                                           j_scoped_manager,
                                           j_scoped_factory));
}

void ServiceRegistryAndroid::RemoveService(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_service_registry,
    const JavaParamRef<jstring>& j_name) {
  std::string name(ConvertJavaStringToUTF8(env, j_name));
  service_registry_->RemoveService(name);
}

void ServiceRegistryAndroid::ConnectToRemoteService(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_service_registry,
    const JavaParamRef<jstring>& j_name,
    jint j_handle) {
  std::string name(ConvertJavaStringToUTF8(env, j_name));
  mojo::ScopedMessagePipeHandle handle((mojo::MessagePipeHandle(j_handle)));
  service_registry_->ConnectToRemoteService(name, std::move(handle));
}

}  // namespace content
