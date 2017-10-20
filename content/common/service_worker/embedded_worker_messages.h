// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_SERVICE_WORKER_EMBEDDED_WORKER_MESSAGES_H_
#define CONTENT_COMMON_SERVICE_WORKER_EMBEDDED_WORKER_MESSAGES_H_

#include <stdint.h>

#include <string>

#include "content/common/service_worker/embedded_worker_settings.h"
#include "content/common/service_worker/embedded_worker_start_params.h"
#include "content/public/common/console_message_level.h"
#include "content/public/common/web_preferences.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_param_traits.h"
#include "url/gurl.h"

#undef IPC_MESSAGE_EXPORT
#define IPC_MESSAGE_EXPORT CONTENT_EXPORT

#define IPC_MESSAGE_START EmbeddedWorkerMsgStart

IPC_STRUCT_TRAITS_BEGIN(content::EmbeddedWorkerSettings)
  IPC_STRUCT_TRAITS_MEMBER(v8_cache_options)
  IPC_STRUCT_TRAITS_MEMBER(data_saver_enabled)
IPC_STRUCT_TRAITS_END()

// Parameters structure for EmbeddedWorkerMsg_StartWorker.
IPC_STRUCT_TRAITS_BEGIN(content::EmbeddedWorkerStartParams)
  IPC_STRUCT_TRAITS_MEMBER(embedded_worker_id)
  IPC_STRUCT_TRAITS_MEMBER(service_worker_version_id)
  IPC_STRUCT_TRAITS_MEMBER(scope)
  IPC_STRUCT_TRAITS_MEMBER(script_url)
  IPC_STRUCT_TRAITS_MEMBER(worker_devtools_agent_route_id)
  IPC_STRUCT_TRAITS_MEMBER(devtools_worker_token)
  IPC_STRUCT_TRAITS_MEMBER(pause_after_download)
  IPC_STRUCT_TRAITS_MEMBER(wait_for_debugger)
  IPC_STRUCT_TRAITS_MEMBER(is_installed)
  IPC_STRUCT_TRAITS_MEMBER(settings)
IPC_STRUCT_TRAITS_END()

// Renderer -> Browser message to count an API use. |feature| must be one of the
// values from blink::UseCounter::Feature enum.
IPC_MESSAGE_CONTROL2(EmbeddedWorkerHostMsg_CountFeature,
                     int64_t /* service_worker_version_id */,
                     uint32_t /* feature */)

// ---------------------------------------------------------------------------
// For EmbeddedWorkerContext related messages, which are directly sent from
// browser to the worker thread in the child process. We use a new message class
// for this for easier cross-thread message dispatching.

#undef IPC_MESSAGE_START
#define IPC_MESSAGE_START EmbeddedWorkerContextMsgStart

// Browser -> Renderer message to send message.
IPC_MESSAGE_CONTROL3(EmbeddedWorkerContextMsg_MessageToWorker,
                     int /* thread_id */,
                     int /* embedded_worker_id */,
                     IPC::Message /* message */)

#endif  // CONTENT_COMMON_SERVICE_WORKER_EMBEDDED_WORKER_MESSAGES_H_
