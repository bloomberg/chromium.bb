// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Multiply-included message file, so no include guard.

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "content/common/common_param_traits.h"
#include "content/common/indexed_db_key.h"
#include "content/common/indexed_db_param_traits.h"
#include "content/common/serialized_script_value.h"
#include "ipc/ipc_message_macros.h"

#define IPC_MESSAGE_START UtilityMsgStart

//------------------------------------------------------------------------------
// Utility process messages:
// These are messages from the browser to the utility process.

// Tell the utility process to extract the given IDBKeyPath from the
// SerializedScriptValue vector and reply with the corresponding IDBKeys.
IPC_MESSAGE_CONTROL3(UtilityMsg_IDBKeysFromValuesAndKeyPath,
                     int,     // id
                     std::vector<SerializedScriptValue>,
                     string16)  // IDBKeyPath

IPC_MESSAGE_CONTROL3(UtilityMsg_InjectIDBKey,
                     IndexedDBKey /* key */,
                     SerializedScriptValue /* value */,
                     string16 /* key path*/)

// Tells the utility process that it's running in batch mode.
IPC_MESSAGE_CONTROL0(UtilityMsg_BatchMode_Started)

// Tells the utility process that it can shutdown.
IPC_MESSAGE_CONTROL0(UtilityMsg_BatchMode_Finished)


//------------------------------------------------------------------------------
// Utility process host messages:
// These are messages from the utility process to the browser.

// Reply when the utility process has succeeded in obtaining the value for
// IDBKeyPath.
IPC_MESSAGE_CONTROL2(UtilityHostMsg_IDBKeysFromValuesAndKeyPath_Succeeded,
                     int /* id */,
                     std::vector<IndexedDBKey> /* value */)

// Reply when the utility process has failed in obtaining the value for
// IDBKeyPath.
IPC_MESSAGE_CONTROL1(UtilityHostMsg_IDBKeysFromValuesAndKeyPath_Failed,
                     int /* id */)

// Reply when the utility process has finished injecting an IDBKey into
// a SerializedScriptValue.
IPC_MESSAGE_CONTROL1(UtilityHostMsg_InjectIDBKey_Finished,
                     SerializedScriptValue /* new value */)
