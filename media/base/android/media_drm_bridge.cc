// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/android/media_drm_bridge.h"

#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/logging.h"
#include "media/base/android/media_player_manager.h"

using base::android::ConvertJavaStringToUTF8;
using base::android::JavaByteArrayToByteVector;
using base::android::ScopedJavaLocalRef;

namespace media {

// static
bool MediaDrmBridge::IsAvailable() {
  return false;
}

MediaDrmBridge* MediaDrmBridge::Create(int media_keys_id,
                                       const std::vector<uint8>& uuid,
                                       MediaPlayerManager* manager) {
  if (!IsAvailable())
    return NULL;

  // TODO(qinmin): check whether the uuid is valid.
  return new MediaDrmBridge(media_keys_id, uuid, manager);
}

MediaDrmBridge::MediaDrmBridge(int media_keys_id,
                               const std::vector<uint8>& uuid,
                               MediaPlayerManager* manager)
    : media_keys_id_(media_keys_id), uuid_(uuid), manager_(manager) {
  // TODO(qinmin): pass the uuid to DRM engine.
}

MediaDrmBridge::~MediaDrmBridge() {}

bool MediaDrmBridge::GenerateKeyRequest(const std::string& type,
                                        const uint8* init_data,
                                        int init_data_length) {
  NOTIMPLEMENTED();
  return false;
}

void MediaDrmBridge::CancelKeyRequest(const std::string& session_id) {
  NOTIMPLEMENTED();
}

void MediaDrmBridge::AddKey(const uint8* key, int key_length,
                            const uint8* init_data, int init_data_length,
                            const std::string& session_id) {
  NOTIMPLEMENTED();
}

ScopedJavaLocalRef<jobject> MediaDrmBridge::GetMediaCrypto() {
  NOTIMPLEMENTED();
  return ScopedJavaLocalRef<jobject>();
}

void MediaDrmBridge::OnKeyMessage(JNIEnv* env,
                                  jobject j_media_drm,
                                  jstring j_session_id,
                                  jbyteArray j_message,
                                  jstring j_destination_url) {
  std::string session_id = ConvertJavaStringToUTF8(env, j_session_id);
  std::vector<uint8> message;
  JavaByteArrayToByteVector(env, j_message, &message);
  std::string message_string(message.begin(), message.end());
  std::string destination_url = ConvertJavaStringToUTF8(env, j_destination_url);

  manager_->OnKeyMessage(
      media_keys_id_, session_id, message_string, destination_url);
}

void MediaDrmBridge::OnDrmEvent(JNIEnv* env,
                                jobject j_media_drm,
                                jstring session_id,
                                jint event,
                                jint extra,
                                jstring data) {
  NOTIMPLEMENTED();
}

}  // namespace media
