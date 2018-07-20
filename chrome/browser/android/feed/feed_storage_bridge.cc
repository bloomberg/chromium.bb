// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/feed/feed_storage_bridge.h"

#include <jni.h>

#include <string>
#include <vector>

#include "base/android/callback_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/bind.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/android/feed/feed_host_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_android.h"
#include "components/feed/core/feed_host_service.h"
#include "components/feed/core/feed_storage_database.h"
#include "jni/FeedStorageBridge_jni.h"
#include "ui/gfx/android/java_bitmap.h"
#include "ui/gfx/image/image.h"

namespace feed {

using base::android::AppendJavaStringArrayToStringVector;
using base::android::AttachCurrentThread;
using base::android::ConvertJavaStringToUTF8;
using base::android::JavaArrayOfByteArrayToStringVector;
using base::android::JavaIntArrayToIntVector;
using base::android::JavaRef;
using base::android::JavaParamRef;
using base::android::ScopedJavaGlobalRef;
using base::android::ScopedJavaLocalRef;
using base::android::ToJavaArrayOfByteArray;
using base::android::ToJavaArrayOfStrings;

static jlong JNI_FeedStorageBridge_Init(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_this,
    const JavaParamRef<jobject>& j_profile) {
  Profile* profile = ProfileAndroid::FromProfileAndroid(j_profile);
  FeedHostService* host_service =
      FeedHostServiceFactory::GetForBrowserContext(profile);
  DCHECK(host_service);
  FeedStorageDatabase* feed_storage_database =
      host_service->GetStorageDatabase();
  DCHECK(feed_storage_database);
  FeedStorageBridge* native_storage_bridge =
      new FeedStorageBridge(feed_storage_database);
  return reinterpret_cast<intptr_t>(native_storage_bridge);
}

FeedStorageBridge::FeedStorageBridge(FeedStorageDatabase* feed_storage_database)
    : feed_storage_database_(feed_storage_database), weak_ptr_factory_(this) {}

FeedStorageBridge::~FeedStorageBridge() = default;

void FeedStorageBridge::Destroy(JNIEnv* env, const JavaRef<jobject>& j_this) {
  delete this;
}

void FeedStorageBridge::LoadContent(JNIEnv* j_env,
                                    const JavaRef<jobject>& j_this,
                                    const JavaRef<jobjectArray>& j_keys,
                                    const JavaRef<jobject>& j_callback) {
  std::vector<std::string> keys;
  AppendJavaStringArrayToStringVector(j_env, j_keys.obj(), &keys);
  ScopedJavaGlobalRef<jobject> callback(j_callback);

  feed_storage_database_->LoadContent(
      keys, base::BindOnce(&FeedStorageBridge::OnLoadContentDone,
                           weak_ptr_factory_.GetWeakPtr(), callback));
}

void FeedStorageBridge::LoadContentByPrefix(
    JNIEnv* j_env,
    const JavaRef<jobject>& j_this,
    const JavaRef<jstring>& j_prefix,
    const JavaRef<jobject>& j_callback) {
  std::string prefix = ConvertJavaStringToUTF8(j_env, j_prefix);
  ScopedJavaGlobalRef<jobject> callback(j_callback);

  feed_storage_database_->LoadContentByPrefix(
      prefix, base::BindOnce(&FeedStorageBridge::OnLoadContentDone,
                             weak_ptr_factory_.GetWeakPtr(), callback));
}

void FeedStorageBridge::LoadAllContentKeys(JNIEnv* j_env,
                                           const JavaRef<jobject>& j_this,
                                           const JavaRef<jobject>& j_callback) {
  ScopedJavaGlobalRef<jobject> callback(j_callback);

  feed_storage_database_->LoadAllContentKeys(
      base::BindOnce(&FeedStorageBridge::OnLoadAllContentKeysDone,
                     weak_ptr_factory_.GetWeakPtr(), callback));
}

void FeedStorageBridge::SaveContent(JNIEnv* j_env,
                                    const JavaRef<jobject>& j_this,
                                    const JavaRef<jobjectArray>& j_keys,
                                    const JavaRef<jobjectArray>& j_data,
                                    const JavaRef<jobject>& j_callback) {
  std::vector<std::string> keys;
  std::vector<std::string> data;
  AppendJavaStringArrayToStringVector(j_env, j_keys.obj(), &keys);
  JavaArrayOfByteArrayToStringVector(j_env, j_data.obj(), &data);
  ScopedJavaGlobalRef<jobject> callback(j_callback);

  DCHECK_EQ(keys.size(), data.size());
  std::vector<FeedStorageDatabase::KeyAndData> pairs;
  for (size_t i = 0; i < keys.size() && i < data.size(); ++i) {
    pairs.emplace_back(keys[i], data[i]);
  }

  feed_storage_database_->SaveContent(
      std::move(pairs),
      base::BindOnce(&FeedStorageBridge::OnStorageCommitDone,
                     weak_ptr_factory_.GetWeakPtr(), callback));
}

void FeedStorageBridge::DeleteContent(JNIEnv* j_env,
                                      const JavaRef<jobject>& j_this,
                                      const JavaRef<jobjectArray>& j_keys,
                                      const JavaRef<jobject>& j_callback) {
  std::vector<std::string> keys;
  AppendJavaStringArrayToStringVector(j_env, j_keys.obj(), &keys);
  ScopedJavaGlobalRef<jobject> callback(j_callback);

  feed_storage_database_->DeleteContent(
      keys, base::BindOnce(&FeedStorageBridge::OnStorageCommitDone,
                           weak_ptr_factory_.GetWeakPtr(), callback));
}

void FeedStorageBridge::DeleteContentByPrefix(
    JNIEnv* j_env,
    const JavaRef<jobject>& j_this,
    const JavaRef<jstring>& j_prefix,
    const JavaRef<jobject>& j_callback) {
  std::string prefix = ConvertJavaStringToUTF8(j_env, j_prefix);
  ScopedJavaGlobalRef<jobject> callback(j_callback);

  feed_storage_database_->DeleteContentByPrefix(
      prefix, base::BindOnce(&FeedStorageBridge::OnStorageCommitDone,
                             weak_ptr_factory_.GetWeakPtr(), callback));
}

void FeedStorageBridge::DeleteAllContent(JNIEnv* j_env,
                                         const JavaRef<jobject>& j_this,
                                         const JavaRef<jobject>& j_callback) {
  ScopedJavaGlobalRef<jobject> callback(j_callback);

  feed_storage_database_->DeleteAllContent(
      base::BindOnce(&FeedStorageBridge::OnStorageCommitDone,
                     weak_ptr_factory_.GetWeakPtr(), callback));
}

void FeedStorageBridge::OnLoadContentDone(
    ScopedJavaGlobalRef<jobject> callback,
    std::vector<FeedStorageDatabase::KeyAndData> pairs) {
  std::vector<std::string> keys;
  std::vector<std::string> data;
  for (auto pair : pairs) {
    keys.push_back(std::move(pair.first));
    data.push_back(std::move(pair.second));
  }

  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobjectArray> j_keys = ToJavaArrayOfStrings(env, keys);
  ScopedJavaLocalRef<jobjectArray> j_data = ToJavaArrayOfByteArray(env, data);

  // Ceate Java Map by JNI call.
  ScopedJavaLocalRef<jobject> j_pairs =
      Java_FeedStorageBridge_createKeyAndDataMap(env, j_keys, j_data);
  RunObjectCallbackAndroid(callback, j_pairs);
}

void FeedStorageBridge::OnLoadAllContentKeysDone(
    ScopedJavaGlobalRef<jobject> callback,
    std::vector<std::string> keys) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobjectArray> j_keys = ToJavaArrayOfStrings(env, keys);

  // Ceate Java List by JNI call.
  ScopedJavaLocalRef<jobject> j_keys_list =
      Java_FeedStorageBridge_createJavaList(env, j_keys);
  RunObjectCallbackAndroid(callback, j_keys_list);
}

void FeedStorageBridge::OnStorageCommitDone(
    ScopedJavaGlobalRef<jobject> callback,
    bool success) {
  RunBooleanCallbackAndroid(callback, success);
}

}  // namespace feed
