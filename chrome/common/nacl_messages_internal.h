// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/nacl_types.h"
#include "ipc/ipc_message_macros.h"

//-----------------------------------------------------------------------------
// NaClProcess messages
// These are messages sent from the browser to the NaCl process.
IPC_BEGIN_MESSAGES(NaClProcess)
  // Tells the NaCl process to start.
  IPC_MESSAGE_CONTROL2(NaClProcessMsg_Start,
                       int /* descriptor id */,
                       nacl::FileDescriptor /* handle value */)

IPC_END_MESSAGES(NaClProcess)

