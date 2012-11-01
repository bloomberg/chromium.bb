// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Multiply-included message file, so no include guard.

#include <string>
#include <vector>

#include "base/shared_memory.h"
#include "content/public/common/common_param_traits.h"
#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_param_traits.h"
#include "ui/base/clipboard/clipboard.h"

#define IPC_MESSAGE_START ClipboardMsgStart

#ifndef CONTENT_COMMON_CLIPBOARD_MESSAGES_H_
#define CONTENT_COMMON_CLIPBOARD_MESSAGES_H_

namespace IPC {

template<>
struct ParamTraits<ui::Clipboard::FormatType> {
  typedef ui::Clipboard::FormatType param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, PickleIterator* iter, param_type* r);
  static void Log(const param_type& p, std::string* l);
};

}  // namespace IPC

#endif  // CONTENT_COMMON_CLIPBOARD_MESSAGES_H_

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
IPC_SYNC_MESSAGE_CONTROL1_1(ClipboardHostMsg_GetSequenceNumber,
                            ui::Clipboard::Buffer /* buffer */,
                            uint64 /* result */)
IPC_SYNC_MESSAGE_CONTROL2_1(ClipboardHostMsg_IsFormatAvailable,
                            ui::Clipboard::FormatType /* format */,
                            ui::Clipboard::Buffer /* buffer */,
                            bool /* result */)
IPC_MESSAGE_CONTROL1(ClipboardHostMsg_Clear,
                     ui::Clipboard::Buffer /* buffer */)
IPC_SYNC_MESSAGE_CONTROL1_2(ClipboardHostMsg_ReadAvailableTypes,
                            ui::Clipboard::Buffer /* buffer */,
                            std::vector<string16> /* types */,
                            bool /* contains filenames */)
IPC_SYNC_MESSAGE_CONTROL1_1(ClipboardHostMsg_ReadText,
                            ui::Clipboard::Buffer /* buffer */,
                            string16 /* result */)
IPC_SYNC_MESSAGE_CONTROL1_1(ClipboardHostMsg_ReadAsciiText,
                            ui::Clipboard::Buffer  /* buffer */,
                            std::string /* result */)
IPC_SYNC_MESSAGE_CONTROL1_4(ClipboardHostMsg_ReadHTML,
                            ui::Clipboard::Buffer  /* buffer */,
                            string16 /* markup */,
                            GURL /* url */,
                            uint32 /* fragment start */,
                            uint32 /* fragment end */)
IPC_SYNC_MESSAGE_CONTROL1_1(ClipboardHostMsg_ReadRTF,
                            ui::Clipboard::Buffer /* buffer */,
                            std::string /* result */)
IPC_SYNC_MESSAGE_CONTROL1_2(ClipboardHostMsg_ReadImage,
                            ui::Clipboard::Buffer /* buffer */,
                            base::SharedMemoryHandle /* PNG-encoded image */,
                            uint32 /* image size */)
IPC_SYNC_MESSAGE_CONTROL2_1(ClipboardHostMsg_ReadCustomData,
                            ui::Clipboard::Buffer /* buffer */,
                            string16 /* type */,
                            string16 /* result */)
IPC_SYNC_MESSAGE_CONTROL1_1(ClipboardHostMsg_ReadData,
                            ui::Clipboard::FormatType /* format */,
                            std::string /* result */)

#if defined(OS_MACOSX)
IPC_MESSAGE_CONTROL1(ClipboardHostMsg_FindPboardWriteStringAsync,
                     string16 /* text */)
#endif
