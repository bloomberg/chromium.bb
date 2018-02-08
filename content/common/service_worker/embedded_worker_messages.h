// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_SERVICE_WORKER_EMBEDDED_WORKER_MESSAGES_H_
#define CONTENT_COMMON_SERVICE_WORKER_EMBEDDED_WORKER_MESSAGES_H_

#include <stdint.h>

#include "ipc/ipc_message.h"
#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_param_traits.h"

#undef IPC_MESSAGE_EXPORT
#define IPC_MESSAGE_EXPORT CONTENT_EXPORT

#define IPC_MESSAGE_START EmbeddedWorkerMsgStart

// Renderer -> Browser message to count an API use. |feature| must be one of the
// values from blink::UseCounter::Feature enum.
IPC_MESSAGE_CONTROL2(EmbeddedWorkerHostMsg_CountFeature,
                     int64_t /* service_worker_version_id */,
                     uint32_t /* feature */)

#endif  // CONTENT_COMMON_SERVICE_WORKER_EMBEDDED_WORKER_MESSAGES_H_
