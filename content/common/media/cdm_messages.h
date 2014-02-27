// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// IPC messages for content decryption module (CDM) implementation.
// Multiply-included message file, hence no include guard.

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "content/common/content_export.h"
#include "content/common/media/cdm_messages_enums.h"
#include "ipc/ipc_message_macros.h"
#include "media/base/media_keys.h"
#include "url/gurl.h"

#undef IPC_MESSAGE_EXPORT
#define IPC_MESSAGE_EXPORT CONTENT_EXPORT
#define IPC_MESSAGE_START CdmMsgStart

IPC_ENUM_TRAITS(media::MediaKeys::KeyError)
IPC_ENUM_TRAITS(CdmHostMsg_CreateSession_Type)

IPC_MESSAGE_ROUTED3(CdmHostMsg_InitializeCDM,
                    int /* media_keys_id */,
                    std::vector<uint8> /* uuid */,
                    GURL /* frame url */)

IPC_MESSAGE_ROUTED4(CdmHostMsg_CreateSession,
                    int /* media_keys_id */,
                    uint32_t /* session_id */,
                    CdmHostMsg_CreateSession_Type /* type */,
                    std::vector<uint8> /* init_data */)

IPC_MESSAGE_ROUTED3(CdmHostMsg_UpdateSession,
                    int /* media_keys_id */,
                    uint32_t /* session_id */,
                    std::vector<uint8> /* response */)

IPC_MESSAGE_ROUTED2(CdmHostMsg_ReleaseSession,
                    int /* media_keys_id */,
                    uint32_t /* session_id */)

IPC_MESSAGE_ROUTED1(CdmHostMsg_DestroyCdm,
                    int /* media_keys_id */)

IPC_MESSAGE_ROUTED3(CdmMsg_SessionCreated,
                    int /* media_keys_id */,
                    uint32_t /* session_id */,
                    std::string /* web_session_id */)

IPC_MESSAGE_ROUTED4(CdmMsg_SessionMessage,
                    int /* media_keys_id */,
                    uint32_t /* session_id */,
                    std::vector<uint8> /* message */,
                    GURL /* destination_url */)

IPC_MESSAGE_ROUTED2(CdmMsg_SessionReady,
                    int /* media_keys_id */,
                    uint32_t /* session_id */)

IPC_MESSAGE_ROUTED2(CdmMsg_SessionClosed,
                    int /* media_keys_id */,
                    uint32_t /* session_id */)

IPC_MESSAGE_ROUTED4(CdmMsg_SessionError,
                    int /* media_keys_id */,
                    uint32_t /* session_id */,
                    media::MediaKeys::KeyError /* error_code */,
                    int /* system_code */)
