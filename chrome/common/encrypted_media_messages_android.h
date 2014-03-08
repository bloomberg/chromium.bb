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

}  // namespace android

#endif  // CHROME_COMMON_ENCRYPTED_MEDIA_MESSAGES_ANDROID_H


#define IPC_MESSAGE_START EncryptedMediaMsgStart

IPC_ENUM_TRAITS(android::SupportedCodecs)

IPC_STRUCT_BEGIN(SupportedKeySystemRequest)
  IPC_STRUCT_MEMBER(std::string, key_system)
  IPC_STRUCT_MEMBER(android::SupportedCodecs, codecs,
                    android::NO_SUPPORTED_CODECS)
IPC_STRUCT_END()

IPC_STRUCT_BEGIN(SupportedKeySystemResponse)
  IPC_STRUCT_MEMBER(std::string, key_system)
  IPC_STRUCT_MEMBER(android::SupportedCodecs, compositing_codecs,
                    android::NO_SUPPORTED_CODECS)
  IPC_STRUCT_MEMBER(android::SupportedCodecs, non_compositing_codecs,
                    android::NO_SUPPORTED_CODECS)
IPC_STRUCT_END()

// Messages sent from the renderer to the browser.

// Synchronously get a list of supported EME key systems.
IPC_SYNC_MESSAGE_CONTROL1_1(
    ChromeViewHostMsg_GetSupportedKeySystems,
    SupportedKeySystemRequest /* key system information request */,
    SupportedKeySystemResponse /* key system information response */)
