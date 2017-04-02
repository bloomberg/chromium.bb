// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/android/media_drm_storage_bridge.h"

#include <memory>

#include "base/android/callback_android.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "jni/MediaDrmStorageBridge_jni.h"

using base::android::JavaParamRef;
using base::android::RunCallbackAndroid;
using base::android::ScopedJavaLocalRef;

namespace media {

// static
bool MediaDrmStorageBridge::RegisterMediaDrmStorageBridge(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

MediaDrmStorageBridge::MediaDrmStorageBridge()
    : task_runner_(base::ThreadTaskRunnerHandle::Get()), weak_factory_(this) {}

MediaDrmStorageBridge::~MediaDrmStorageBridge() = default;

// TODO(yucliu): Implement these methods with MediaDrmStorage

void MediaDrmStorageBridge::OnProvisioned(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_storage,
    // Callback<Boolean>
    const JavaParamRef<jobject>& j_callback) {
  NOTIMPLEMENTED();

  RunCallbackAndroid(j_callback, true);
}

void MediaDrmStorageBridge::OnLoadInfo(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_storage,
    const JavaParamRef<jbyteArray>& j_session_id,
    // Callback<PersistentInfo>
    const JavaParamRef<jobject>& j_callback) {
  NOTIMPLEMENTED();

  RunCallbackAndroid(j_callback, ScopedJavaLocalRef<jobject>());
}

void MediaDrmStorageBridge::OnSaveInfo(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_storage,
    const JavaParamRef<jobject>& j_persist_info,
    // Callback<Boolean>
    const JavaParamRef<jobject>& j_callback) {
  NOTIMPLEMENTED();

  RunCallbackAndroid(j_callback, false);
}

void MediaDrmStorageBridge::OnClearInfo(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_storage,
    const JavaParamRef<jbyteArray>& j_session_id,
    // Callback<Boolean>
    const JavaParamRef<jobject>& j_callback) {
  NOTIMPLEMENTED();

  RunCallbackAndroid(j_callback, false);
}
}  // namespace media
