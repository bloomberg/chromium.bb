// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Messages sent from the ARC instance to the host.
// Multiply-included message file, hence no include guard.

#include "ipc/ipc_message_macros.h"

#include "components/arc/common/arc_message_types.h"

#define IPC_MESSAGE_START ArcInstanceHostMsgStart

IPC_ENUM_TRAITS_MAX_VALUE(arc::InstanceBootPhase, arc::InstanceBootPhase::LAST)

IPC_MESSAGE_CONTROL1(ArcInstanceHostMsg_InstanceBootPhase,
                     arc::InstanceBootPhase)
