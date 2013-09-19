// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// IPC messages for EME on android.
// Multiply-included message file, hence no include guard.

#include <vector>

#include "ipc/ipc_message_macros.h"

// Singly-included section for enums and custom IPC traits.
#ifndef CHROME_COMMON_ENCRYPTED_MEDIA_MESSAGES_ANDROID_H
#define CHROME_COMMON_ENCRYPTED_MEDIA_MESSAGES_ANDROID_H

namespace android {

// Defines bitmask values used to specify supported codecs.
// Each value represents a codec within a specific container.
enum SupportedCodecs {
  NO_SUPPORTED_CODECS = 0,
  WEBM_VP8_AND_VORBIS = 1 << 0,
  MP4_AAC = 1 << 1,
  MP4_AVC1 = 1 << 2,
};

struct SupportedKeySystemRequest {
  SupportedKeySystemRequest();
  ~SupportedKeySystemRequest();

  // Key system UUID.
  std::vector<uint8> uuid;
  // Bitmask of requested codecs.
  SupportedCodecs codecs;
};

struct SupportedKeySystemResponse {
  SupportedKeySystemResponse();
  ~SupportedKeySystemResponse();

  // Key system UUID.
  std::vector<uint8> uuid;
  // Bitmask of supported compositing codecs.
  SupportedCodecs compositing_codecs;
  // Bitmask of supported non-compositing codecs.
  SupportedCodecs non_compositing_codecs;
};

}  // namespace android

#endif  // CHROME_COMMON_ENCRYPTED_MEDIA_MESSAGES_ANDROID_H


#define IPC_MESSAGE_START EncryptedMediaMsgStart

IPC_ENUM_TRAITS(android::SupportedCodecs)

IPC_STRUCT_TRAITS_BEGIN(android::SupportedKeySystemRequest)
  IPC_STRUCT_TRAITS_MEMBER(uuid)
  IPC_STRUCT_TRAITS_MEMBER(codecs)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(android::SupportedKeySystemResponse)
  IPC_STRUCT_TRAITS_MEMBER(uuid)
  IPC_STRUCT_TRAITS_MEMBER(compositing_codecs)
  IPC_STRUCT_TRAITS_MEMBER(non_compositing_codecs)
IPC_STRUCT_TRAITS_END()


// Messages sent from the renderer to the browser.

// Synchronously get a list of supported EME key systems.
IPC_SYNC_MESSAGE_CONTROL1_1(
    ChromeViewHostMsg_GetSupportedKeySystems,
    android::SupportedKeySystemRequest /* key system information request */,
    android::SupportedKeySystemResponse /* key system information response */)
