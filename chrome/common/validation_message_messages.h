// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/string16.h"
#include "ipc/ipc_message_macros.h"
#include "ui/gfx/rect.h"

#define IPC_MESSAGE_START ValidationMessageMsgStart

IPC_MESSAGE_CONTROL4(ValidationMessageMsg_ShowValidationMessage,
                     int /* route id */,
                     gfx::Rect /* anchor rectangle in root view coordinate */,
                     string16 /* validation message */,
                     string16 /* supplemental text */)

IPC_MESSAGE_CONTROL0(ValidationMessageMsg_HideValidationMessage)

IPC_MESSAGE_CONTROL2(ValidationMessageMsg_MoveValidationMessage,
                     int /* route id */,
                     gfx::Rect /* anchor rectangle in root view coordinate */)

