// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Multiply-included message file, hence no include guard.

#include "content/public/common/password_form.h"
#include "ipc/ipc_message_macros.h"

#define IPC_MESSAGE_START OneClickSigninMsgStart

IPC_MESSAGE_ROUTED1(OneClickSigninHostMsg_FormSubmitted, content::PasswordForm)
