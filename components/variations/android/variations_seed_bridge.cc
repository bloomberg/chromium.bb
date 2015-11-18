// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/android/variations_seed_bridge.h"

#include <jni.h>
#include <vector>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/jni_weak_ref.h"
#include "jni/VariationsSeedBridge_jni.h"

using base::android::AttachCurrentThread;
using base::android::ConvertJavaStringToUTF8;
using base::android::GetApplicationContext;
using base::android::ScopedJavaLocalRef;

namespace {

std::string JavaByteArrayToString(JNIEnv* env, jbyteArray byte_array) {
  if (!byte_array)
    return std::string();
  std::vector<uint8> array_data;
  base::android::JavaByteArrayToByteVector(env, byte_array, &array_data);
  return std::string(array_data.begin(), array_data.end());
}

}  // namespace

namespace variations {
namespace android {

bool RegisterVariationsSeedBridge(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

void GetVariationsFirstRunSeed(std::string* seed_data,
                               std::string* seed_signature,
                               std::string* seed_country) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jbyteArray> j_seed_data =
      Java_VariationsSeedBridge_getVariationsFirstRunSeedData(
          env, GetApplicationContext());
  ScopedJavaLocalRef<jstring> j_seed_signature =
      Java_VariationsSeedBridge_getVariationsFirstRunSeedSignature(
          env, GetApplicationContext());
  ScopedJavaLocalRef<jstring> j_seed_country =
      Java_VariationsSeedBridge_getVariationsFirstRunSeedCountry(
          env, GetApplicationContext());
  *seed_data = JavaByteArrayToString(env, j_seed_data.obj());
  *seed_signature = ConvertJavaStringToUTF8(j_seed_signature);
  *seed_country = ConvertJavaStringToUTF8(j_seed_country);
}

void ClearJavaFirstRunPrefs() {
  JNIEnv* env = AttachCurrentThread();
  Java_VariationsSeedBridge_clearFirstRunPrefs(env, GetApplicationContext());
}

void MarkVariationsSeedAsStored() {
  JNIEnv* env = AttachCurrentThread();
  Java_VariationsSeedBridge_markVariationsSeedAsStored(env,
                                                       GetApplicationContext());
}

}  // namespace android
}  // namespace variations
