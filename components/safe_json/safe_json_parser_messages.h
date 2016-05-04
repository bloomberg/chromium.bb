// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Message definition file, included multiple times, hence no include guard.

#include <string>
#include <vector>

#include "base/strings/string16.h"
#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_message_utils.h"

#define IPC_MESSAGE_START SafeJsonParserMsgStart

//------------------------------------------------------------------------------
// Process messages:
// This is a message from the browser to the utility process telling it to parse
// a JSON string into a Value object.
IPC_MESSAGE_CONTROL1(SafeJsonParserMsg_ParseJSON,
                     std::string /* JSON to parse */)

//------------------------------------------------------------------------------
// Host messages: Reply when the utility process successfully parsed a JSON
// string.
//
// WARNING: The result can be of any Value subclass type, but we can't easily
// pass indeterminate value types by const object reference with our IPC macros,
// so we put the result Value into a ListValue. Handlers should examine the
// first (and only) element of the ListValue for the actual result.
IPC_MESSAGE_CONTROL1(SafeJsonParserHostMsg_ParseJSON_Succeeded, base::ListValue)

// Reply when the utility process failed in parsing a JSON string.
IPC_MESSAGE_CONTROL1(SafeJsonParserHostMsg_ParseJSON_Failed,
                     std::string /* error message, if any*/)
