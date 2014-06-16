// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/importer/profile_import_process_messages.h"
#include "ipc/ipc_message_macros.h"

#define IPC_MESSAGE_START FirefoxImporterUnittestMsgStart

// Messages definitions for messages sent between the unit test binary and
// a child process by FFUnitTestDecryptorProxy.

// Server->Child: Initialize the decrytor with the following paramters.
IPC_MESSAGE_CONTROL2(Msg_Decryptor_Init,
                     base::FilePath /* dll_path */,
                     base::FilePath /* db_path */)
// Child->Server: Return paramter from init call.
IPC_MESSAGE_CONTROL1(Msg_Decryptor_InitReturnCode,
                     bool /* ret */)

// Server->Child: Decrypt a given string.
IPC_MESSAGE_CONTROL1(Msg_Decrypt,
                     std::string /* crypt */)
// Child->Server: Decrypted String.
IPC_MESSAGE_CONTROL1(Msg_Decryptor_Response,
                     base::string16 /* unencrypted_str */)
// Server->Child: Parse firefox signons db from a given path
IPC_MESSAGE_CONTROL1(Msg_ParseSignons,
                     base::FilePath /* path to firefox signons db */)
// Child->Server: Vector of parsed password forms
IPC_MESSAGE_CONTROL1(Msg_ParseSignons_Response,
                     std::vector<autofill::PasswordForm> /* parsed signons */)

// Server->Child: Die.
IPC_MESSAGE_CONTROL0(Msg_Decryptor_Quit)
