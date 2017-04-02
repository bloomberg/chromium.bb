// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_ANDROID_MEDIA_DRM_STORAGE_BRIDGE_H_
#define MEDIA_BASE_ANDROID_MEDIA_DRM_STORAGE_BRIDGE_H_

#include <jni.h>
#include <memory>

#include "base/android/scoped_java_ref.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace media {

// This class is the native version of MediaDrmStorageBridge in Java. It's used
// to talk to the concrete implementation for persistent data management.
class MediaDrmStorageBridge {
 public:
  static bool RegisterMediaDrmStorageBridge(JNIEnv* env);

  MediaDrmStorageBridge();
  ~MediaDrmStorageBridge();

  // The following OnXXX functions are called by Java. The functions will post
  // task on message loop immediately to avoid reentrancy issues.

  // Called by the java object when device provision is finished. Implementation
  // will record the time as provisioning time.
  void OnProvisioned(JNIEnv* env,
                     const base::android::JavaParamRef<jobject>& j_storage,
                     // Callback<Boolean>
                     const base::android::JavaParamRef<jobject>& j_callback);

  // Called by the java object to load session data into memory. |j_callback|
  // will return a null object if load fails.
  void OnLoadInfo(JNIEnv* env,
                  const base::android::JavaParamRef<jobject>& j_storage,
                  const base::android::JavaParamRef<jbyteArray>& j_session_id,
                  // Callback<PersistentInfo>
                  const base::android::JavaParamRef<jobject>& j_callback);

  // Called by the java object to persistent session data.
  void OnSaveInfo(JNIEnv* env,
                  const base::android::JavaParamRef<jobject>& j_storage,
                  // PersistentInfo
                  const base::android::JavaParamRef<jobject>& j_persist_info,
                  // Callback<Boolean>
                  const base::android::JavaParamRef<jobject>& j_callback);

  // Called by the java object to remove persistent session data.
  void OnClearInfo(JNIEnv* env,
                   const base::android::JavaParamRef<jobject>& j_storage,
                   const base::android::JavaParamRef<jbyteArray>& j_session_id,
                   // Callback<Boolean>
                   const base::android::JavaParamRef<jobject>& j_callback);

 private:
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  // NOTE: Weak pointers must be invalidated before all other member variables.
  base::WeakPtrFactory<MediaDrmStorageBridge> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(MediaDrmStorageBridge);
};

}  // namespace media
#endif  // MEDIA_BASE_ANDROID_MEDIA_DRM_STORAGE_BRIDGE_H_
