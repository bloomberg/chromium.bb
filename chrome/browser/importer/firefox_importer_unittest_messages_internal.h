// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This header is meant to be included in multiple passes, hence no traditional
// header guard.
// See ipc_message_macros.h for explanation of the macros and passes.

#include "ipc/ipc_message_macros.h"

// Messages definitions for messages sent between the unit test binary and
// a child process by FFUnitTestDecryptorProxy.
IPC_BEGIN_MESSAGES(Test)

// Server->Child: Initialize the decrytor with the following paramters.
IPC_MESSAGE_CONTROL2(Msg_Decryptor_Init,
                     std::wstring /* dll_path */,
                     std::wstring /* db_path */)
// Child->Server: Return paramter from init call.
IPC_MESSAGE_CONTROL1(Msg_Decryptor_InitReturnCode,
                     bool /* ret */)

// Server->Child: Decrypt a given string.
IPC_MESSAGE_CONTROL1(Msg_Decrypt,
                     std::string /* crypt */)
// Child->Server: Decrypted String.
IPC_MESSAGE_CONTROL1(Msg_Decryptor_Response,
                     string16 /* unencrypted_str */)

// Server->Child: Die.
IPC_MESSAGE_CONTROL0(Msg_Decryptor_Quit)

IPC_END_MESSAGES(Test)
