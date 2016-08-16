// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/interface_registry_android_impl.h"

#include <utility>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/callback.h"
#include "base/memory/ptr_util.h"
#include "content/public/browser/browser_thread.h"
#include "jni/InterfaceRegistry_jni.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "services/shell/public/cpp/interface_provider.h"
#include "services/shell/public/cpp/interface_registry.h"

using base::android::AttachCurrentThread;
using base::android::ConvertJavaStringToUTF8;
using base::android::JavaParamRef;
using base::android::ScopedJavaGlobalRef;

namespace content {

namespace {

// Callback passed to the wrapped InterfaceRegistry upon AddInterface(). The
// InterfaceRegistry will call it to create a registered Java impl.
void CreateImplAndAttach(
    const ScopedJavaGlobalRef<jobject>& j_scoped_interface_registry,
    const ScopedJavaGlobalRef<jobject>& j_scoped_manager,
    const ScopedJavaGlobalRef<jobject>& j_scoped_factory,
    mojo::ScopedMessagePipeHandle handle) {
  JNIEnv* env = AttachCurrentThread();
  Java_InterfaceRegistry_createImplAndAttach(
      env, j_scoped_interface_registry, handle.release().value(),
      j_scoped_manager, j_scoped_factory);
}

}  // namespace

// static
std::unique_ptr<InterfaceRegistryAndroid> InterfaceRegistryAndroid::Create(
    shell::InterfaceRegistry* interface_registry) {
  return base::WrapUnique(new InterfaceRegistryAndroidImpl(interface_registry));
}

// static
bool InterfaceRegistryAndroidImpl::Register(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

InterfaceRegistryAndroidImpl::~InterfaceRegistryAndroidImpl() {
  Java_InterfaceRegistry_destroy(AttachCurrentThread(), obj_);
}

// Constructor and destructor call into Java.
InterfaceRegistryAndroidImpl::InterfaceRegistryAndroidImpl(
    shell::InterfaceRegistry* interface_registry)
    : interface_registry_(interface_registry) {
  JNIEnv* env = AttachCurrentThread();
  obj_.Reset(
      env,
      Java_InterfaceRegistry_create(
          env, reinterpret_cast<intptr_t>(this)).obj());
}

const base::android::ScopedJavaGlobalRef<jobject>&
InterfaceRegistryAndroidImpl::GetObj() {
  return obj_;
}

// Methods called from Java.
void InterfaceRegistryAndroidImpl::AddInterface(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_interface_registry,
    const JavaParamRef<jobject>& j_manager,
    const JavaParamRef<jobject>& j_factory,
    const JavaParamRef<jstring>& j_name) {
  std::string name(ConvertJavaStringToUTF8(env, j_name));

  ScopedJavaGlobalRef<jobject> j_scoped_interface_registry;
  j_scoped_interface_registry.Reset(env, j_interface_registry);

  ScopedJavaGlobalRef<jobject> j_scoped_manager;
  j_scoped_manager.Reset(env, j_manager);

  ScopedJavaGlobalRef<jobject> j_scoped_factory;
  j_scoped_factory.Reset(env, j_factory);

  // All Java interfaces must be bound on the main thread.
  scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner =
      BrowserThread::GetTaskRunnerForThread(BrowserThread::UI);
  interface_registry_->AddInterface(
      name,
      base::Bind(&CreateImplAndAttach, j_scoped_interface_registry,
                 j_scoped_manager, j_scoped_factory),
      ui_task_runner);
}

void InterfaceRegistryAndroidImpl::RemoveInterface(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_interface_registry,
    const JavaParamRef<jstring>& j_name) {
  std::string name(ConvertJavaStringToUTF8(env, j_name));
  interface_registry_->RemoveInterface(name);
}

}  // namespace content
