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

static uint32 ReadUint32(const uint8_t* data) {
  uint32 value = 0;
  for (int i = 0; i < 4; ++i)
    value = (value << 8) | data[i];
  return value;
}

static uint64 ReadUint64(const uint8_t* data) {
  uint64 value = 0;
  for (int i = 0; i < 8; ++i)
    value = (value << 8) | data[i];
  return value;
}

// The structure of an ISO CENC Protection System Specific Header (PSSH) box is
// as follows. (See ISO/IEC FDIS 23001-7:2011(E).)
// Note: ISO boxes use big-endian values.
//
// PSSH {
//   uint32 Size
//   uint32 Type
//   uint64 LargeSize  # Field is only present if value(Size) == 1.
//   uint32 VersionAndFlags
//   uint8[16] SystemId
//   uint32 DataSize
//   uint8[DataSize] Data
// }
static const int kBoxHeaderSize = 8;  // Box's header contains Size and Type.
static const int kBoxLargeSizeSize = 8;
static const int kPsshVersionFlagSize = 4;
static const int kPsshSystemIdSize = 16;
static const int kPsshDataSizeSize = 4;
static const uint32 kTencType = 0x74656e63;
static const uint32 kPsshType = 0x70737368;

// Tries to find a PSSH box whose "SystemId" is |uuid| in |data|, parses the
// "Data" of the box and put it in |pssh_data|. Returns true if such a box is
// found and successfully parsed. Returns false otherwise.
// Notes:
// 1, If multiple PSSH boxes are found,the "Data" of the first matching PSSH box
// will be set in |pssh_data|.
// 2, Only PSSH and TENC boxes are allowed in |data|. TENC boxes are skipped.
static bool GetPsshData(const uint8* data, int data_size,
                        const std::vector<uint8>& uuid,
                        std::vector<uint8>* pssh_data) {
  const uint8* cur = data;
  const uint8* data_end = data + data_size;
  int bytes_left = data_size;

  while (bytes_left > 0) {
    const uint8* box_head = cur;

    if (bytes_left < kBoxHeaderSize)
      return false;

    uint64_t box_size = ReadUint32(cur);
    uint32 type = ReadUint32(cur + 4);
    cur += kBoxHeaderSize;
    bytes_left -= kBoxHeaderSize;

    if (box_size == 1) {  // LargeSize is present.
      if (bytes_left < kBoxLargeSizeSize)
        return false;

      box_size = ReadUint64(cur);
      cur += kBoxLargeSizeSize;
      bytes_left -= kBoxLargeSizeSize;
    } else if (box_size == 0) {
      box_size = bytes_left + kBoxHeaderSize;
    }

    const uint8* box_end = box_head + box_size;
    if (data_end < box_end)
      return false;

    if (type == kTencType) {
      // Skip 'tenc' box.
      cur = box_end;
      bytes_left = data_end - cur;
      continue;
    } else if (type != kPsshType) {
      return false;
    }

    const int kPsshBoxMinimumSize =
        kPsshVersionFlagSize + kPsshSystemIdSize + kPsshDataSizeSize;
    if (box_end < cur + kPsshBoxMinimumSize)
      return false;

    uint32 version_and_flags = ReadUint32(cur);
    cur += kPsshVersionFlagSize;
    bytes_left -= kPsshVersionFlagSize;
    if (version_and_flags != 0)
      return false;

    DCHECK_GE(bytes_left, kPsshSystemIdSize);
    if (!std::equal(uuid.begin(), uuid.end(), cur)) {
      cur = box_end;
      bytes_left = data_end - cur;
      continue;
    }

    cur += kPsshSystemIdSize;
    bytes_left -= kPsshSystemIdSize;

    uint32 data_size = ReadUint32(cur);
    cur += kPsshDataSizeSize;
    bytes_left -= kPsshDataSizeSize;

    if (box_end < cur + data_size)
      return false;

    pssh_data->assign(cur, cur + data_size);
    return true;
  }

  return false;
}

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
  std::vector<uint8> pssh_data;
  if (!GetPsshData(init_data, init_data_length, uuid_, &pssh_data))
    return false;

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
  std::string destination_url = ConvertJavaStringToUTF8(env, j_destination_url);

  manager_->OnKeyMessage(media_keys_id_, session_id, message, destination_url);
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
