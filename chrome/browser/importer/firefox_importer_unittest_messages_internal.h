// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipc/ipc_message_macros.h"

#define IPC_MESSAGE_START FirefoxImporterUnittestMsgStart

// Messages definitions for messages sent between the unit test binary and
// a child process by FFUnitTestDecryptorProxy.

// Server->Child: Initialize the decrytor with the following paramters.
IPC_MESSAGE_CONTROL2(Msg_Decryptor_Init,
                     FilePath /* dll_path */,
                     FilePath /* db_path */)
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
