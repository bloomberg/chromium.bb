// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Defines messages between the browser and worker process, as well as between
// the renderer and worker process.

// Multiply-included message file, hence no include guard.

#include <string>
#include <utility>
#include <vector>

#include "base/strings/string16.h"
#include "content/common/content_export.h"
#include "content/common/content_param_traits.h"
#include "content/common/message_port.h"
#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_message_utils.h"
#include "mojo/public/cpp/system/handle.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "url/gurl.h"

#undef IPC_MESSAGE_EXPORT
#define IPC_MESSAGE_EXPORT CONTENT_EXPORT
#define IPC_MESSAGE_START WorkerMsgStart

// Parameters structure for WorkerHostMsg_PostConsoleMessageToWorkerObject,
// which has too many data parameters to be reasonably put in a predefined
// IPC message. The data members directly correspond to parameters of
// WebWorkerClient::postConsoleMessageToWorkerObject()
IPC_STRUCT_BEGIN(WorkerHostMsg_PostConsoleMessageToWorkerObject_Params)
  IPC_STRUCT_MEMBER(int, source_identifier)
  IPC_STRUCT_MEMBER(int, message_type)
  IPC_STRUCT_MEMBER(int, message_level)
  IPC_STRUCT_MEMBER(base::string16, message)
  IPC_STRUCT_MEMBER(int, line_number)
  IPC_STRUCT_MEMBER(base::string16, source_url)
IPC_STRUCT_END()

// Parameter structure for WorkerProcessMsg_CreateWorker.
IPC_STRUCT_BEGIN(WorkerProcessMsg_CreateWorker_Params)
  IPC_STRUCT_MEMBER(GURL, url)
  IPC_STRUCT_MEMBER(base::string16, name)
  IPC_STRUCT_MEMBER(base::string16, content_security_policy)
  IPC_STRUCT_MEMBER(blink::WebContentSecurityPolicyType, security_policy_type)
  IPC_STRUCT_MEMBER(blink::WebAddressSpace, creation_address_space)
  IPC_STRUCT_MEMBER(bool, pause_on_start)
  IPC_STRUCT_MEMBER(int, route_id)
  IPC_STRUCT_MEMBER(bool, data_saver_enabled)
  // For blink::mojom::SharedWorkerContentSettingsProxy
  IPC_STRUCT_MEMBER(mojo::MessagePipeHandle, content_settings_handle)
IPC_STRUCT_END()

//-----------------------------------------------------------------------------
// WorkerProcess messages
// These are messages sent from the browser to the worker process.
IPC_MESSAGE_CONTROL1(WorkerProcessMsg_CreateWorker,
                     WorkerProcessMsg_CreateWorker_Params)

//-----------------------------------------------------------------------------
// Worker messages
// These are messages sent from the renderer process to the worker process.
IPC_MESSAGE_ROUTED0(WorkerMsg_TerminateWorkerContext)

IPC_MESSAGE_ROUTED2(WorkerMsg_Connect,
                    int /* connection_request_id */,
                    content::MessagePort /* sent_message_port */)

IPC_MESSAGE_ROUTED0(WorkerMsg_WorkerObjectDestroyed)


//-----------------------------------------------------------------------------
// WorkerHost messages
// These are messages sent from the worker (renderer process) to the worker
// host (browser process).

// Sent when the worker calls API that should be recored in UseCounter.
// |feature| must be one of the values from blink::UseCounter::Feature
// enum.
IPC_MESSAGE_CONTROL2(WorkerHostMsg_CountFeature,
                     int /* worker_route_id */,
                     uint32_t /* feature */)

IPC_MESSAGE_CONTROL1(WorkerHostMsg_WorkerContextClosed,
                     int /* worker_route_id */)

IPC_MESSAGE_CONTROL1(WorkerHostMsg_WorkerContextDestroyed,
                     int /* worker_route_id */)

// Renderer -> Browser message to indicate that the worker is ready for
// inspection.
IPC_MESSAGE_CONTROL1(WorkerHostMsg_WorkerReadyForInspection,
                     int /* worker_route_id */)

IPC_MESSAGE_CONTROL1(WorkerHostMsg_WorkerScriptLoaded,
                     int /* worker_route_id */)

IPC_MESSAGE_CONTROL1(WorkerHostMsg_WorkerScriptLoadFailed,
                     int /* worker_route_id */)

IPC_MESSAGE_CONTROL2(WorkerHostMsg_WorkerConnected,
                     int /* connection_request_id */,
                     int /* worker_route_id */)
