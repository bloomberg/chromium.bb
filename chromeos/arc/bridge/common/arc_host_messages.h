// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Messages sent from the host to the ARC instance
// Multiply-included message file, hence no include guard.

#include "ipc/ipc_message_macros.h"

#define IPC_MESSAGE_START ArcInstanceHostMsgStart

IPC_MESSAGE_CONTROL0(ArcInstanceHostMsg_InstanceReady)
