// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Multiply-included message file, so no include guard.

#include "base/file_path.h"
#include "chrome/common/common_param_traits.h"
#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_param_traits.h"

#define IPC_MESSAGE_START MimeRegistryMsgStart

// Mime registry messages sent from the renderer to the browser.

// Sent to query MIME information.
IPC_SYNC_MESSAGE_CONTROL1_1(MimeRegistryMsg_GetMimeTypeFromExtension,
                            FilePath::StringType /* extension */,
                            std::string /* mime_type */)
IPC_SYNC_MESSAGE_CONTROL1_1(MimeRegistryMsg_GetMimeTypeFromFile,
                            FilePath /* file_path */,
                            std::string /* mime_type */)
IPC_SYNC_MESSAGE_CONTROL1_1(MimeRegistryMsg_GetPreferredExtensionForMimeType,
                            std::string /* mime_type */,
                            FilePath::StringType /* extension */)

