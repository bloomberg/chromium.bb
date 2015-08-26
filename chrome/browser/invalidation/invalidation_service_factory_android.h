#// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_INVALIDATION_INVALIDATION_SERVICE_FACTORY_ANDROID_H_
#define CHROME_BROWSER_INVALIDATION_INVALIDATION_SERVICE_FACTORY_ANDROID_H_

#include <jni.h>
#include "base/android/scoped_java_ref.h"

namespace invalidation {

// This class should not be used except from the Java class
// InvalidationServiceFactory.
class InvalidationServiceFactoryAndroid {
 public:
  static base::android::ScopedJavaLocalRef<jobject>
  GetForProfile(JNIEnv* env, jclass clazz, jobject j_profile);

  static base::android::ScopedJavaLocalRef<jobject>
  GetForTest(JNIEnv* env, jclass clazz, jobject j_context);

  static bool Register(JNIEnv* env);
};

}  // namespace invalidation

#endif  // CHROME_BROWSER_INVALIDATION_INVALIDATION_SERVICE_FACTORY_ANDROID_H_
