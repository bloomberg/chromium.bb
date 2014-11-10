// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DEVTOOLS_BRIDGE_ANDROID_SESSION_DEPENDENCY_FACTORY_NATIVE_H_
#define COMPONENTS_DEVTOOLS_BRIDGE_ANDROID_SESSION_DEPENDENCY_FACTORY_NATIVE_H_

#include "jni.h"

namespace devtools_bridge {
namespace android {

class SessionDependencyFactoryNative {
 public:
  SessionDependencyFactoryNative();
  ~SessionDependencyFactoryNative();

  static void RegisterNatives(JNIEnv* env);
  inline static SessionDependencyFactoryNative* cast(jlong ptr);
};

SessionDependencyFactoryNative*
SessionDependencyFactoryNative::cast(jlong ptr) {
  return reinterpret_cast<SessionDependencyFactoryNative*>(ptr);
}

}  // namespace android
}  // namespace devtools_bridge

#endif  // COMPONENTS_DEVTOOLS_BRIDGE_ANDROID_SESSION_DEPENDENCY_FACTORY_NATIVE_H_
