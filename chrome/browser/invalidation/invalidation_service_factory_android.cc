// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/invalidation/invalidation_service_factory_android.h"

#include "base/android/jni_android.h"
#include "chrome/browser/invalidation/profile_invalidation_provider_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_android.h"
#include "components/invalidation/invalidation_service_android.h"
#include "components/invalidation/profile_invalidation_provider.h"
#include "jni/InvalidationServiceFactory_jni.h"

using base::android::ScopedJavaLocalRef;

namespace invalidation {

jobject InvalidationServiceFactoryAndroid::GetForProfile(JNIEnv* env,
                                                         jclass clazz,
                                                         jobject j_profile) {
  Profile* profile = ProfileAndroid::FromProfileAndroid(j_profile);
  invalidation::ProfileInvalidationProvider* provider =
      ProfileInvalidationProviderFactory::GetForProfile(profile);
  InvalidationServiceAndroid* service_android =
      static_cast<InvalidationServiceAndroid*>(
          provider->GetInvalidationService());
  return service_android->java_ref_.obj();
}

jobject InvalidationServiceFactoryAndroid::GetForTest(JNIEnv* env,
                                                      jclass clazz,
                                                      jobject j_context) {
  InvalidationServiceAndroid* service_android =
      new InvalidationServiceAndroid(j_context);
  return service_android->java_ref_.obj();
}

jobject GetForProfile(JNIEnv* env, jclass clazz, jobject j_profile) {
  return InvalidationServiceFactoryAndroid::GetForProfile(
      env, clazz, j_profile);
}

jobject GetForTest(JNIEnv* env, jclass clazz, jobject j_context) {
  return InvalidationServiceFactoryAndroid::GetForTest(
      env, clazz, j_context);
}

bool InvalidationServiceFactoryAndroid::Register(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

}  // namespace invalidation
