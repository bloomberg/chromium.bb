// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gcm_driver/gcm_driver_android.h"

#include "base/compiler_specific.h"
#include "base/logging.h"

namespace gcm {
static void Java_GCMDriver_doNothing(JNIEnv* env) ALLOW_UNUSED;
}  // namespace gcm

// Must come after the ALLOW_UNUSED declaration.
#include "jni/GCMDriver_jni.h"

namespace gcm {

GCMDriverAndroid::GCMDriverAndroid() {
}

GCMDriverAndroid::~GCMDriverAndroid() {
}

void GCMDriverAndroid::Enable() {
}

void GCMDriverAndroid::Disable() {
}

void GCMDriverAndroid::Register(const std::string& app_id,
                                const std::vector<std::string>& sender_ids,
                                const RegisterCallback& callback) {
  // TODO(johnme): Hook up to Android GCM API via JNI.
  NOTIMPLEMENTED();
}

void GCMDriverAndroid::Unregister(const std::string& app_id,
                                  const UnregisterCallback& callback) {
  // TODO(johnme): Hook up to Android GCM API via JNI.
  NOTIMPLEMENTED();
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

// static
bool GCMDriverAndroid::RegisterBindings(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

}  // namespace gcm
