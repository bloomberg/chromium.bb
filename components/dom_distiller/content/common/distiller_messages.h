// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Multiply-included message file, hence no include guard.

#include <string>
#include <vector>

#include "base/strings/string16.h"
#include "content/public/common/common_param_traits.h"
#include "content/public/common/common_param_traits_macros.h"
#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_message_utils.h"

#define IPC_MESSAGE_START DistillerMsgStart

// Whether a page is suitable for DOM distiller to process.
IPC_MESSAGE_ROUTED2(FrameHostMsg_Distillability,
                    bool /* is_distillable */,
                    bool /* is_last_update */)
