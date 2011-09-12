// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Multiply-included message file, hence no include guard

#include "ipc/ipc_message_macros.h"
#include "ui/base/range/range.h"
#include "ui/gfx/rect.h"

#if defined(OS_MACOSX)
#include "content/common/mac/attributed_string_coder.h"
#endif

#define IPC_MESSAGE_START TextInputClientMsgStart

// Browser -> Renderer Messages ////////////////////////////////////////////////
// These messages are sent from the browser to the renderer. Each one has a
// corresponding reply message.
////////////////////////////////////////////////////////////////////////////////

// Tells the renderer to send back the character index for a point.
IPC_MESSAGE_ROUTED1(TextInputClientMsg_CharacterIndexForPoint,
                    gfx::Point)

// Tells the renderer to send back the rectangle for a given character range.
IPC_MESSAGE_ROUTED1(TextInputClientMsg_FirstRectForCharacterRange,
                    ui::Range)

// Tells the renderer to send back the text fragment in a given range.
IPC_MESSAGE_ROUTED1(TextInputClientMsg_StringForRange,
                    ui::Range)

////////////////////////////////////////////////////////////////////////////////

// Renderer -> Browser Replies /////////////////////////////////////////////////
// These messages are sent in reply to the above messages.
////////////////////////////////////////////////////////////////////////////////

// Reply message for TextInputClientMsg_CharacterIndexForPoint.
IPC_MESSAGE_ROUTED1(TextInputClientReplyMsg_GotCharacterIndexForPoint,
                    size_t /* character index */)

// Reply message for TextInputClientMsg_FirstRectForCharacterRange.
IPC_MESSAGE_ROUTED1(TextInputClientReplyMsg_GotFirstRectForRange,
                    gfx::Rect /* frame rectangle */)

#if defined(OS_MACOSX)
// Reply message for TextInputClientMsg_StringForRange.
IPC_MESSAGE_ROUTED1(TextInputClientReplyMsg_GotStringForRange,
                    mac::AttributedStringCoder::EncodedString)
#endif  // defined(OS_MACOSX)

