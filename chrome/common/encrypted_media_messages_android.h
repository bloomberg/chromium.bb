// Copyright 2013 The Chromium Authors. All rights reserved.
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

// Synchronously get a list of supported EME key systems.
IPC_SYNC_MESSAGE_CONTROL1_1(
    ChromeViewHostMsg_GetSupportedKeySystems,
    SupportedKeySystemRequest /* key system information request */,
    SupportedKeySystemResponse /* key system information response */)
