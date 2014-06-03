// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gcm_driver/gcm_driver_android.h"

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "jni/GCMDriver_jni.h"

using base::android::AppendJavaStringArrayToStringVector;
using base::android::AttachCurrentThread;
using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertUTF8ToJavaString;
using base::android::ToJavaArrayOfStrings;

namespace gcm {

 GCMDriverAndroid::GCMDriverAndroid() {
  JNIEnv* env = AttachCurrentThread();
  java_ref_.Reset(
      Java_GCMDriver_create(env,
                            reinterpret_cast<intptr_t>(this),
                            base::android::GetApplicationContext()));
}

GCMDriverAndroid::~GCMDriverAndroid() {
  JNIEnv* env = AttachCurrentThread();
  Java_GCMDriver_destroy(env, java_ref_.obj());
}

void GCMDriverAndroid::Enable() {
}

void GCMDriverAndroid::Disable() {
}

void GCMDriverAndroid::Register(const std::string& app_id,
                                const std::vector<std::string>& sender_ids,
                                const RegisterCallback& callback) {
  // TODO(johnme): Store callback.

  JNIEnv* env = AttachCurrentThread();
  Java_GCMDriver_register(
      env, java_ref_.obj(),
      ConvertUTF8ToJavaString(env, app_id).Release(),
      ToJavaArrayOfStrings(env, sender_ids).obj());
}

void GCMDriverAndroid::Unregister(const std::string& app_id,
                                  const UnregisterCallback& callback) {
  // TODO(johnme): Store callback.

  JNIEnv* env = AttachCurrentThread();
  Java_GCMDriver_unregister(
      env, java_ref_.obj(),
      ConvertUTF8ToJavaString(env, app_id).Release());
}

void GCMDriverAndroid::Send(const std::string& app_id,
                            const std::string& receiver_id,
                            const GCMClient::OutgoingMessage& message,
                            const SendCallback& callback) {
  NOTIMPLEMENTED();
}

GCMClient* GCMDriverAndroid::GetGCMClientForTesting() const {
  NOTIMPLEMENTED();
  return NULL;
}

bool GCMDriverAndroid::IsStarted() const {
  return true;
}

bool GCMDriverAndroid::IsGCMClientReady() const {
  return true;
}

void GCMDriverAndroid::GetGCMStatistics(
    const GetGCMStatisticsCallback& callback,
    bool clear_logs) {
  NOTIMPLEMENTED();
}

void GCMDriverAndroid::SetGCMRecording(const GetGCMStatisticsCallback& callback,
                                       bool recording) {
  NOTIMPLEMENTED();
}

std::string GCMDriverAndroid::SignedInUserName() const {
  return std::string();
}

void GCMDriverAndroid::OnRegisterFinished(JNIEnv* env,
                                          jobject obj,
                                          jstring j_app_id,
                                          jstring j_registration_id,
                                          jboolean success) {
  std::string app_id = ConvertJavaStringToUTF8(env, j_app_id);
  std::string registration_id = ConvertJavaStringToUTF8(env, j_registration_id);
  GCMClient::Result result ALLOW_UNUSED = success ? GCMClient::SUCCESS
                                                  : GCMClient::UNKNOWN_ERROR;

  NOTIMPLEMENTED(); // TODO(johnme): Fire callback.
}

void GCMDriverAndroid::OnUnregisterFinished(JNIEnv* env,
                                            jobject obj,
                                            jstring j_app_id,
                                            jboolean success) {
  std::string app_id = ConvertJavaStringToUTF8(env, j_app_id);
  GCMClient::Result result ALLOW_UNUSED = success ? GCMClient::SUCCESS
                                                  : GCMClient::UNKNOWN_ERROR;

  NOTIMPLEMENTED(); // TODO(johnme): Fire callback.
}

void GCMDriverAndroid::OnMessageReceived(JNIEnv* env,
                                         jobject obj,
                                         jstring j_app_id,
                                         jstring j_sender_id,
                                         jstring j_collapse_key,
                                         jobjectArray j_data_keys_and_values) {
  std::string app_id = ConvertJavaStringToUTF8(env, j_app_id);
  GCMClient::IncomingMessage message;
  message.sender_id = ConvertJavaStringToUTF8(env, j_sender_id);
  message.collapse_key = ConvertJavaStringToUTF8(env, j_collapse_key);
  // Expand j_data_keys_and_values from array to map.
  std::vector<std::string> data_keys_and_values;
  AppendJavaStringArrayToStringVector(env,
                                      j_data_keys_and_values,
                                      &data_keys_and_values);
  for (size_t i = 0; i + 1 < data_keys_and_values.size(); i += 2) {
    message.data[data_keys_and_values[i]] = data_keys_and_values[i+1];
  }

  NOTIMPLEMENTED(); // TODO(johnme): Notify app handler.
}

void GCMDriverAndroid::OnMessagesDeleted(JNIEnv* env,
                                         jobject obj,
                                         jstring j_app_id) {
  std::string app_id = ConvertJavaStringToUTF8(env, j_app_id);

  NOTIMPLEMENTED(); // TODO(johnme): Notify app handler.
}

// static
bool GCMDriverAndroid::RegisterBindings(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

}  // namespace gcm
