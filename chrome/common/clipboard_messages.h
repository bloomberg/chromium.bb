// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Multiply-included message file, so no include guard.

#include <string>
#include <vector>

// #include "chrome/common/common_param_traits.h"
#include "content/common/common_param_traits.h"
#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_param_traits.h"
#include "ui/base/clipboard/clipboard.h"

#define IPC_MESSAGE_START ClipboardMsgStart

IPC_ENUM_TRAITS(ui::Clipboard::Buffer)

// Clipboard IPC messages sent from the renderer to the browser.

// This message is used when the object list does not contain a bitmap.
IPC_MESSAGE_CONTROL1(ClipboardHostMsg_WriteObjectsAsync,
                     ui::Clipboard::ObjectMap /* objects */)
// This message is used when the object list contains a bitmap.
// It is synchronized so that the renderer knows when it is safe to
// free the shared memory used to transfer the bitmap.
IPC_SYNC_MESSAGE_CONTROL2_0(ClipboardHostMsg_WriteObjectsSync,
                            ui::Clipboard::ObjectMap /* objects */,
                            base::SharedMemoryHandle /* bitmap handle */)
IPC_SYNC_MESSAGE_CONTROL2_1(ClipboardHostMsg_IsFormatAvailable,
                            std::string /* format */,
                            ui::Clipboard::Buffer /* buffer */,
                            bool /* result */)
IPC_SYNC_MESSAGE_CONTROL1_1(ClipboardHostMsg_ReadText,
                            ui::Clipboard::Buffer /* buffer */,
                            string16 /* result */)
IPC_SYNC_MESSAGE_CONTROL1_1(ClipboardHostMsg_ReadAsciiText,
                            ui::Clipboard::Buffer  /* buffer */,
                            std::string /* result */)
IPC_SYNC_MESSAGE_CONTROL1_2(ClipboardHostMsg_ReadHTML,
                            ui::Clipboard::Buffer  /* buffer */,
                            string16 /* markup */,
                            GURL /* url */)
#if defined(OS_MACOSX)
IPC_MESSAGE_CONTROL1(ClipboardHostMsg_FindPboardWriteStringAsync,
                     string16 /* text */)
#endif
IPC_SYNC_MESSAGE_CONTROL1_3(ClipboardHostMsg_ReadAvailableTypes,
                            ui::Clipboard::Buffer /* buffer */,
                            bool /* result */,
                            std::vector<string16> /* types */,
                            bool /* contains filenames */)
IPC_SYNC_MESSAGE_CONTROL2_3(ClipboardHostMsg_ReadData,
                            ui::Clipboard::Buffer /* buffer */,
                            string16 /* type */,
                            bool /* succeeded */,
                            string16 /* data */,
                            string16 /* metadata */)
IPC_SYNC_MESSAGE_CONTROL1_2(ClipboardHostMsg_ReadFilenames,
                            ui::Clipboard::Buffer /* buffer */,
                            bool /* result */,
                            std::vector<string16> /* filenames */)
