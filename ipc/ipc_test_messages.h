// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"

#if defined(OS_WIN)
#include "ipc/handle_win.h"
#include "ipc/ipc_message_macros.h"
#define IPC_MESSAGE_START IPCTestMsgStart

IPC_MESSAGE_CONTROL3(TestHandleWinMsg, int, IPC::HandleWin, int)
IPC_MESSAGE_CONTROL2(TestTwoHandleWinMsg, IPC::HandleWin, IPC::HandleWin)
#endif  // defined(OS_WIN)
