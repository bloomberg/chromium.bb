// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// IPC messages for EME on android.
// Multiply-included message file, hence no include guard.

#include <vector>

#include "content/public/common/eme_codec.h"
#include "ipc/ipc_message_macros.h"

#define IPC_MESSAGE_START EncryptedMediaMsgStart

IPC_STRUCT_BEGIN(SupportedKeySystemRequest)
  IPC_STRUCT_MEMBER(std::string, key_system)
  IPC_STRUCT_MEMBER(content::SupportedCodecs, codecs, content::EME_CODEC_NONE)
IPC_STRUCT_END()

IPC_STRUCT_BEGIN(SupportedKeySystemResponse)
  IPC_STRUCT_MEMBER(std::string, key_system)
  IPC_STRUCT_MEMBER(content::SupportedCodecs,
                    compositing_codecs,
                    content::EME_CODEC_NONE)
  IPC_STRUCT_MEMBER(content::SupportedCodecs,
                    non_compositing_codecs,
                    content::EME_CODEC_NONE)
IPC_STRUCT_END()

// Messages sent from the renderer to the browser.

// Synchronously query key system information. If the key system is supported,
// the response will be populated.
IPC_SYNC_MESSAGE_CONTROL1_1(
    ChromeViewHostMsg_QueryKeySystemSupport,
    SupportedKeySystemRequest /* key system information request */,
    SupportedKeySystemResponse /* key system information response */)

// Synchronously get a list of platform-supported EME key system names that
// are not explicitly handled by Chrome.
IPC_SYNC_MESSAGE_CONTROL0_1(
    ChromeViewHostMsg_GetPlatformKeySystemNames,
    std::vector<std::string> /* key system names */)
