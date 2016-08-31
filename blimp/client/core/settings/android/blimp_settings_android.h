// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_CLIENT_CORE_SETTINGS_ANDROID_BLIMP_SETTINGS_ANDROID_H_
#define BLIMP_CLIENT_CORE_SETTINGS_ANDROID_BLIMP_SETTINGS_ANDROID_H_

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/macros.h"
#include "blimp/client/core/session/identity_source.h"

namespace blimp {
namespace client {

class BlimpClientContextImplAndroid;

// JNI bridge between AboutBlimpPreferences.java and native code.
class BlimpSettingsAndroid : public IdentityProvider::Observer {
 public:
  static bool RegisterJni(JNIEnv* env);
  static BlimpSettingsAndroid* FromJavaObject(JNIEnv* env, jobject jobj);
  BlimpSettingsAndroid(JNIEnv* env, jobject jobj);
  ~BlimpSettingsAndroid() override;

  void Destroy(JNIEnv* env, const base::android::JavaParamRef<jobject>& jobj);

  void SetIdentitySource(IdentitySource* identity_source);

  // Notify Java layer for user sign in state change.
  void OnSignedOut();
  void OnSignedIn();

  // IdentityProvider::Observer implementation.
  void OnActiveAccountLogout() override;
  void OnActiveAccountLogin() override;

 private:
  base::android::ScopedJavaGlobalRef<jobject> java_obj_;

  // IdentitySource that broadcasts sign in state change.
  IdentitySource* identity_source_;

  DISALLOW_COPY_AND_ASSIGN(BlimpSettingsAndroid);
};

}  // namespace client
}  // namespace blimp

#endif  //  BLIMP_CLIENT_CORE_SETTINGS_ANDROID_BLIMP_SETTINGS_ANDROID_H_
