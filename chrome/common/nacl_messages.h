// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Defines messages between the browser and NaCl process.

// Multiply-included message file, no traditional include guard.
#include "base/process.h"
#include "chrome/common/nacl_types.h"
#include "ipc/ipc_message_macros.h"

#ifndef CHROME_COMMON_NACL_MESSAGES_H_
#define CHROME_COMMON_NACL_MESSAGES_H_

#endif  // CHROME_COMMON_NACL_MESSAGES_H_

#define IPC_MESSAGE_START NaClMsgStart

//-----------------------------------------------------------------------------
// NaClProcess messages
// These are messages sent between the browser and the NaCl process.
// Tells the NaCl process to start.
IPC_MESSAGE_CONTROL4(NaClProcessMsg_Start,
                     std::vector<nacl::FileDescriptor> /* sockets */,
                     std::string /* validation_cache_key */,
                     std::string /* version */,
                     bool /* enable_exception_handling */)

// Tells the NaCl broker to launch a NaCl loader process.
IPC_MESSAGE_CONTROL1(NaClProcessMsg_LaunchLoaderThroughBroker,
                     std::string /* channel ID for the loader */)

// Notify the browser process that the loader was launched successfully.
IPC_MESSAGE_CONTROL2(NaClProcessMsg_LoaderLaunched,
                     std::string,  /* channel ID for the loader */
                     base::ProcessHandle /* loader process handle */)

// Tells the NaCl broker to attach a debug exception handler to the
// given NaCl loader process.
IPC_MESSAGE_CONTROL1(NaClProcessMsg_LaunchDebugExceptionHandler,
                     int32 /* pid */)

// Notify the browser process that the broker process finished
// attaching a debug exception handler to the given NaCl loader
// process.
IPC_MESSAGE_CONTROL1(NaClProcessMsg_DebugExceptionHandlerLaunched,
                     int32 /* pid */)

// Notify the broker that all loader processes have been terminated and it
// should shutdown.
IPC_MESSAGE_CONTROL0(NaClProcessMsg_StopBroker)

// Used by the NaCl process to query a database in the browser.  The database
// contains the signatures of previously validated code chunks.
IPC_SYNC_MESSAGE_CONTROL1_1(NaClProcessMsg_QueryKnownToValidate,
                            std::string, /* A validation signature */
                            bool /* Can validation be skipped? */)

// Used by the NaCl process to add a validation signature to the validation
// database in the browser.
IPC_MESSAGE_CONTROL1(NaClProcessMsg_SetKnownToValidate,
                     std::string /* A validation signature */)
