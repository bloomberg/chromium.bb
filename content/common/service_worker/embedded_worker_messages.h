// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Message definition file, included multiple times, hence no include guard.

#include <string>

#include "ipc/ipc_message.h"
#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_param_traits.h"
#include "url/gurl.h"

#undef IPC_MESSAGE_EXPORT
#define IPC_MESSAGE_EXPORT CONTENT_EXPORT

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

// Renderer -> Browser message to send message.
// |request_id| might be used for bi-directional messaging.
IPC_MESSAGE_CONTROL3(EmbeddedWorkerHostMsg_SendMessageToBrowser,
                     int /* embedded_worker_id */,
                     int /* request_id */,
                     IPC::Message /* message */)

// Renderer -> Browser message to report an exception.
IPC_MESSAGE_CONTROL5(EmbeddedWorkerHostMsg_ReportException,
                     int /* embedded_worker_id */,
                     base::string16 /* error_message */,
                     int /* line_number */,
                     int /* column_number */,
                     GURL /* source_url */)

// ---------------------------------------------------------------------------
// For EmbeddedWorkerContext related messages, which are directly sent from
// browser to the worker thread in the child process. We use a new message class
// for this for easier cross-thread message dispatching.

#undef IPC_MESSAGE_START
#define IPC_MESSAGE_START EmbeddedWorkerContextMsgStart

// Browser -> Renderer message to send message.
// |request_id| might be used for bi-directional messaging.
IPC_MESSAGE_CONTROL4(EmbeddedWorkerContextMsg_SendMessageToWorker,
                     int /* thread_id */,
                     int /* embedded_worker_id */,
                     int /* request_id */,
                     IPC::Message /* message */)
