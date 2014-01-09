// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Message definition file, included multiple times, hence no include guard.

#include <string>

#include "content/common/service_worker/service_worker_types.h"
#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_param_traits.h"
#include "url/gurl.h"

#undef IPC_MESSAGE_EXPORT
#define IPC_MESSAGE_EXPORT CONTENT_EXPORT

IPC_STRUCT_TRAITS_BEGIN(content::ServiceWorkerFetchRequest)
  IPC_STRUCT_TRAITS_MEMBER(url)
  IPC_STRUCT_TRAITS_MEMBER(method)
  IPC_STRUCT_TRAITS_MEMBER(headers)
IPC_STRUCT_TRAITS_END()

#define IPC_MESSAGE_START EmbeddedWorkerMsgStart

// Browser -> Renderer message to create a new embedded worker context.
IPC_MESSAGE_CONTROL3(EmbeddedWorkerMsg_StartWorker,
                     int /* embedded_worker_id */,
                     int64 /* service_worker_version_id */,
                     GURL /* script_url */)

// Browser -> Renderer message to stop (terminate) the embedded worker.
IPC_MESSAGE_CONTROL1(EmbeddedWorkerMsg_StopWorker,
                     int /* embedded_worker_id */)

// Renderer -> Browser message to indicate that the worker is started.
IPC_MESSAGE_CONTROL2(EmbeddedWorkerHostMsg_WorkerStarted,
                     int /* thread_id */,
                     int /* embedded_worker_id */)

// Renderer -> Browser message to indicate that the worker is stopped.
IPC_MESSAGE_CONTROL1(EmbeddedWorkerHostMsg_WorkerStopped,
                     int /* embedded_worker_id */)

// ---------------------------------------------------------------------------
// For EmbeddedWorkerContext related messages, which are directly sent from
// browser to the worker thread in the child process. We use a new message class
// for this for easier cross-thread message dispatching.

#undef IPC_MESSAGE_START
#define IPC_MESSAGE_START EmbeddedWorkerContextMsgStart

IPC_MESSAGE_CONTROL3(EmbeddedWorkerContextMsg_FetchEvent,
                     int /* thread_id */,
                     int /* embedded_worker_id */,
                     content::ServiceWorkerFetchRequest)
