// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/x509_util_android.h"

#include "base/android/jni_android.h"
#include "jni/X509Util_jni.h"
#include "net/cert/cert_database.h"

namespace net {

void NotifyKeyChainChanged(JNIEnv* env, jclass clazz) {
  CertDatabase::GetInstance()->OnAndroidKeyChainChanged();
}

jobject GetApplicationContext(JNIEnv* env, jclass clazz) {
  return base::android::GetApplicationContext();
}

bool RegisterX509Util(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

}  // net namespace
