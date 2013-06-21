// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/android/media_drm_bridge.h"

#include "base/logging.h"

using base::android::ScopedJavaLocalRef;

namespace media {

// static
bool MediaDrmBridge::IsAvailable() {
  return false;
}

MediaDrmBridge* MediaDrmBridge::Create(int media_keys_id,
                                       const std::vector<uint8>& uuid) {
  if (!IsAvailable())
    return NULL;

  // TODO(qinmin): check whether the uuid is valid.
  return new MediaDrmBridge(media_keys_id, uuid);
}

MediaDrmBridge::MediaDrmBridge(
    int media_keys_id, const std::vector<uint8>& uuid)
    : media_keys_id_(media_keys_id),
      uuid_(uuid) {
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

ScopedJavaLocalRef<jobject> MediaDrmBridge::CreateMediaCrypto(
    const std::string& session_id) {
  NOTIMPLEMENTED();
  return ScopedJavaLocalRef<jobject>();
}

void MediaDrmBridge::ReleaseMediaCrypto(const std::string& session_id) {
  NOTIMPLEMENTED();
}

void MediaDrmBridge::OnKeyMessage(
    JNIEnv* env, jobject j_media_drm, jstring session_id, jbyteArray message,
    jstring destination_url) {
  NOTIMPLEMENTED();
}

void MediaDrmBridge::OnDrmEvent(
    JNIEnv* env, jobject j_media_drm, jstring session_id, jint event,
    jint extra, jstring data) {
  NOTIMPLEMENTED();
}

}  // namespace media
