// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GCM_DRIVER_INSTANCE_ID_INSTANCE_ID_ANDROID_H_
#define COMPONENTS_GCM_DRIVER_INSTANCE_ID_INSTANCE_ID_ANDROID_H_

#include <jni.h>

#include <map>
#include <string>

#include "base/android/scoped_java_ref.h"
#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/id_map.h"
#include "base/macros.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "components/gcm_driver/instance_id/instance_id.h"

namespace instance_id {

// InstanceID implementation for Android.
class InstanceIDAndroid : public InstanceID {
 public:
  // Tests depending on InstanceID that run without a nested Java message loop
  // must use this. Operations that would normally be asynchronous will instead
  // block the UI thread.
  class ScopedBlockOnAsyncTasksForTesting {
   public:
    ScopedBlockOnAsyncTasksForTesting();
    ~ScopedBlockOnAsyncTasksForTesting();

   private:
    bool previous_value_;
    DISALLOW_COPY_AND_ASSIGN(ScopedBlockOnAsyncTasksForTesting);
  };

  // Register JNI methods.
  static bool RegisterJni(JNIEnv* env);

  InstanceIDAndroid(const std::string& app_id);
  ~InstanceIDAndroid() override;

  // InstanceID implementation:
  void GetID(const GetIDCallback& callback) override;
  void GetCreationTime(const GetCreationTimeCallback& callback) override;
  void GetToken(const std::string& audience,
                const std::string& scope,
                const std::map<std::string, std::string>& options,
                const GetTokenCallback& callback) override;
  void DeleteToken(const std::string& audience,
                   const std::string& scope,
                   const DeleteTokenCallback& callback) override;
  void DeleteID(const DeleteIDCallback& callback) override;

  // Methods called from Java via JNI:
  void DidGetToken(JNIEnv* env,
                   const base::android::JavaParamRef<jobject>& obj,
                   jint request_id,
                   const base::android::JavaParamRef<jstring>& jtoken);
  void DidDeleteToken(JNIEnv* env,
                      const base::android::JavaParamRef<jobject>& obj,
                      jint request_id,
                      jboolean success);
  void DidDeleteID(JNIEnv* env,
                   const base::android::JavaParamRef<jobject>& obj,
                   jint request_id,
                   jboolean success);

 private:
  base::android::ScopedJavaGlobalRef<jobject> java_ref_;

  IDMap<GetTokenCallback, IDMapOwnPointer> get_token_callbacks_;
  IDMap<DeleteTokenCallback, IDMapOwnPointer> delete_token_callbacks_;
  IDMap<DeleteIDCallback, IDMapOwnPointer> delete_id_callbacks_;

  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(InstanceIDAndroid);
};

}  // namespace instance_id

#endif  // COMPONENTS_GCM_DRIVER_INSTANCE_ID_INSTANCE_ID_ANDROID_H_
