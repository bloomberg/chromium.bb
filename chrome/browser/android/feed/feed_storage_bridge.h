// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_FEED_FEED_STORAGE_BRIDGE_H_
#define CHROME_BROWSER_ANDROID_FEED_FEED_STORAGE_BRIDGE_H_

#include "base/android/scoped_java_ref.h"
#include "base/memory/weak_ptr.h"
#include "components/feed/core/feed_storage_database.h"

namespace feed {

class FeedStorageDatabase;

// Native counterpart of FeedStorageBridge.java. Holds non-owning pointers
// to native implementation, to which operations are delegated. Results are
// passed back by a single argument callback so
// base::android::RunBooleanCallbackAndroid() and
// base::android::RunObjectCallbackAndroid() can be used. This bridge is
// instantiated, owned, and destroyed from Java.
class FeedStorageBridge {
 public:
  explicit FeedStorageBridge(FeedStorageDatabase* feed_Storage_database);
  ~FeedStorageBridge();

  void Destroy(JNIEnv* j_env, const base::android::JavaRef<jobject>& j_this);

  void LoadContent(JNIEnv* j_env,
                   const base::android::JavaRef<jobject>& j_this,
                   const base::android::JavaRef<jobjectArray>& j_keys,
                   const base::android::JavaRef<jobject>& j_callback);
  void LoadContentByPrefix(JNIEnv* j_env,
                           const base::android::JavaRef<jobject>& j_this,
                           const base::android::JavaRef<jstring>& j_prefix,
                           const base::android::JavaRef<jobject>& j_callback);
  void LoadAllContentKeys(JNIEnv* j_env,
                          const base::android::JavaRef<jobject>& j_this,
                          const base::android::JavaRef<jobject>& j_callback);
  void SaveContent(JNIEnv* j_env,
                   const base::android::JavaRef<jobject>& j_this,
                   const base::android::JavaRef<jobjectArray>& j_keys,
                   const base::android::JavaRef<jobjectArray>& j_data,
                   const base::android::JavaRef<jobject>& j_callback);
  void DeleteContent(JNIEnv* j_env,
                     const base::android::JavaRef<jobject>& j_this,
                     const base::android::JavaRef<jobjectArray>& j_keys,
                     const base::android::JavaRef<jobject>& j_callback);
  void DeleteContentByPrefix(JNIEnv* j_env,
                             const base::android::JavaRef<jobject>& j_this,
                             const base::android::JavaRef<jstring>& j_prefix,
                             const base::android::JavaRef<jobject>& j_callback);
  void DeleteAllContent(JNIEnv* j_env,
                        const base::android::JavaRef<jobject>& j_this,
                        const base::android::JavaRef<jobject>& j_callback);

 private:
  void OnLoadContentDone(base::android::ScopedJavaGlobalRef<jobject> callback,
                         std::vector<FeedStorageDatabase::KeyAndData> pairs);
  void OnLoadAllContentKeysDone(
      base::android::ScopedJavaGlobalRef<jobject> callback,
      std::vector<std::string> keys);
  void OnStorageCommitDone(base::android::ScopedJavaGlobalRef<jobject> callback,
                           bool success);

  FeedStorageDatabase* feed_storage_database_;

  base::WeakPtrFactory<FeedStorageBridge> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(FeedStorageBridge);
};

}  // namespace feed

#endif  // CHROME_BROWSER_ANDROID_FEED_FEED_STORAGE_BRIDGE_H_
