// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/java_system.h"

#include "base/android/jni_string.h"
#include "jni/System_jni.h"

namespace base {
namespace android {

bool JavaSystem::Register(JNIEnv* env) {
  return JNI_System::RegisterNativesImpl(env);
}

// static
std::string JavaSystem::GetProperty(const base::StringPiece& property_name) {
  DCHECK(!property_name.empty());
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jstring> jproperty_name =
      ConvertUTF8ToJavaString(env, property_name);
  ScopedJavaLocalRef<jstring> jproperty =
      JNI_System::Java_System_getPropertyJLS_JLS(env, jproperty_name.obj());
  return jproperty.is_null() ? std::string()
                             : ConvertJavaStringToUTF8(env, jproperty.obj());
}

}  // namespace android
}  // namespace base
