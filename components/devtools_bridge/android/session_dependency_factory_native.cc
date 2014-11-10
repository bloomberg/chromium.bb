// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/devtools_bridge/android/session_dependency_factory_native.h"
#include "jni/SessionDependencyFactoryNative_jni.h"

namespace devtools_bridge {
namespace android {

SessionDependencyFactoryNative::SessionDependencyFactoryNative() {
}

SessionDependencyFactoryNative::~SessionDependencyFactoryNative() {
}

// static
void SessionDependencyFactoryNative::RegisterNatives(JNIEnv* env) {
  RegisterNativesImpl(env);
}

static jlong CreateFactory(JNIEnv* env, jclass jcaller) {
  return reinterpret_cast<jlong>(new SessionDependencyFactoryNative());
}

static void DestroyFactory(JNIEnv* env, jclass jcaller, jlong ptr) {
  delete SessionDependencyFactoryNative::cast(ptr);
}

}  // namespace android
}  // namespace devtools_bridge
