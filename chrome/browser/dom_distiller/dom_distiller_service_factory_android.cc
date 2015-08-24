// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dom_distiller/dom_distiller_service_factory_android.h"

#include "base/android/jni_android.h"
#include "chrome/browser/dom_distiller/dom_distiller_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_android.h"
#include "components/dom_distiller/core/dom_distiller_service_android.h"
#include "jni/DomDistillerServiceFactory_jni.h"

using base::android::ScopedJavaLocalRef;

namespace dom_distiller {
namespace android {

jobject DomDistillerServiceFactoryAndroid::GetForProfile(JNIEnv* env,
                                                         jclass clazz,
                                                         jobject j_profile) {
  dom_distiller::DomDistillerService* service =
      dom_distiller::DomDistillerServiceFactory::GetForBrowserContext(
          ProfileAndroid::FromProfileAndroid(j_profile));
  DomDistillerServiceAndroid* service_android =
      new DomDistillerServiceAndroid(service);
  return service_android->java_ref_.obj();
}

bool DomDistillerServiceFactoryAndroid::Register(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

jobject GetForProfile(JNIEnv* env, jclass clazz, jobject j_profile) {
  return DomDistillerServiceFactoryAndroid::GetForProfile(
      env, clazz, j_profile);
}

}  // namespace android
}  // namespace dom_distiller
