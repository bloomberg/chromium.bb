// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/client/app/android/blimp_client_session_android.h"

#include "blimp/client/feature/tab_control_feature.h"
#include "jni/BlimpClientSession_jni.h"

namespace blimp {
namespace client {
namespace {
const int kDummyTabId = 0;
}  // namespace

static jlong Init(JNIEnv* env, const JavaParamRef<jobject>& jobj) {
  return reinterpret_cast<intptr_t>(new BlimpClientSessionAndroid(env, jobj));
}

// static
bool BlimpClientSessionAndroid::RegisterJni(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

// static
BlimpClientSessionAndroid* BlimpClientSessionAndroid::FromJavaObject(
    JNIEnv* env,
    jobject jobj) {
  return reinterpret_cast<BlimpClientSessionAndroid*>(
      Java_BlimpClientSession_getNativePtr(env, jobj));
}

BlimpClientSessionAndroid::BlimpClientSessionAndroid(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jobj)
    : BlimpClientSession() {
  java_obj_.Reset(env, jobj);

  // Create a single tab's WebContents.
  // TODO(kmarshall): Remove this once we add tab-literacy to Blimp.
  GetTabControlFeature()->CreateTab(kDummyTabId);
}

BlimpClientSessionAndroid::~BlimpClientSessionAndroid() {}

void BlimpClientSessionAndroid::Destroy(JNIEnv* env,
                                        const JavaParamRef<jobject>& jobj) {
  delete this;
}

}  // namespace client
}  // namespace blimp
