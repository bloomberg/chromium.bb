// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GCM_DRIVER_GCM_DRIVER_ANDROID_H
#define COMPONENTS_GCM_DRIVER_GCM_DRIVER_ANDROID_H

#include <jni.h>

#include "base/android/scoped_java_ref.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "components/gcm_driver/gcm_driver.h"

namespace gcm {

// GCMDriver implementation for Android, using Android GCM APIs.
class GCMDriverAndroid : public GCMDriver {
 public:
  GCMDriverAndroid();
  virtual ~GCMDriverAndroid();

  // Methods called from Java via JNI:
  void OnRegisterFinished(JNIEnv* env,
                          jobject obj,
                          jstring app_id,
                          jstring registration_id,
                          jboolean success);
  void OnUnregisterFinished(JNIEnv* env,
                           jobject obj,
                           jstring app_id,
                           jboolean success);
  void OnMessageReceived(JNIEnv* env,
                         jobject obj,
                         jstring app_id,
                         jstring sender_id,
                         jstring collapse_key,
                         jobjectArray data_keys_and_values);
  void OnMessagesDeleted(JNIEnv* env,
                         jobject obj,
                         jstring app_id);

  // Register JNI methods.
  static bool RegisterBindings(JNIEnv* env);

  // GCMDriver implementation:
  virtual void OnSignedIn() OVERRIDE;
  virtual void Purge() OVERRIDE;
  virtual void Enable() OVERRIDE;
  virtual void Disable() OVERRIDE;
  virtual GCMClient* GetGCMClientForTesting() const OVERRIDE;
  virtual bool IsStarted() const OVERRIDE;
  virtual bool IsConnected() const OVERRIDE;
  virtual void GetGCMStatistics(const GetGCMStatisticsCallback& callback,
                                bool clear_logs) OVERRIDE;
  virtual void SetGCMRecording(const GetGCMStatisticsCallback& callback,
                               bool recording) OVERRIDE;
  virtual void UpdateAccountMapping(
      const AccountMapping& account_mapping) OVERRIDE;
  virtual void RemoveAccountMapping(const std::string& account_id) OVERRIDE;

 protected:
  // GCMDriver implementation:
  virtual GCMClient::Result EnsureStarted() OVERRIDE;
  virtual void RegisterImpl(
      const std::string& app_id,
      const std::vector<std::string>& sender_ids) OVERRIDE;
  virtual void UnregisterImpl(const std::string& app_id) OVERRIDE;
  virtual void SendImpl(const std::string& app_id,
                        const std::string& receiver_id,
                        const GCMClient::OutgoingMessage& message) OVERRIDE;

 private:
  base::android::ScopedJavaGlobalRef<jobject> java_ref_;

  DISALLOW_COPY_AND_ASSIGN(GCMDriverAndroid);
};

}  // namespace gcm

#endif  // COMPONENTS_GCM_DRIVER_GCM_DRIVER_ANDROID_H
