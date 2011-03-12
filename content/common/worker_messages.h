// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Defines messages between the browser and worker process, as well as between
// the renderer and worker process.

// Multiply-included message file, hence no include guard.

#include <string>
#include <utility>
#include <vector>

#include "base/basictypes.h"
#include "base/string16.h"
#include "googleurl/src/gurl.h"
#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_message_utils.h"

typedef std::pair<string16, std::vector<int> > QueuedMessage;

#define IPC_MESSAGE_START WorkerMsgStart

// Parameters structure for WorkerHostMsg_PostConsoleMessageToWorkerObject,
// which has too many data parameters to be reasonably put in a predefined
// IPC message. The data members directly correspond to parameters of
// WebWorkerClient::postConsoleMessageToWorkerObject()
IPC_STRUCT_BEGIN(WorkerHostMsg_PostConsoleMessageToWorkerObject_Params)
  IPC_STRUCT_MEMBER(int, source_identifier)
  IPC_STRUCT_MEMBER(int, message_type)
  IPC_STRUCT_MEMBER(int, message_level)
  IPC_STRUCT_MEMBER(string16, message)
  IPC_STRUCT_MEMBER(int, line_number)
  IPC_STRUCT_MEMBER(string16, source_url)
IPC_STRUCT_END()

// Parameter structure for WorkerProcessMsg_CreateWorker.
IPC_STRUCT_BEGIN(WorkerProcessMsg_CreateWorker_Params)
  IPC_STRUCT_MEMBER(GURL, url)
  IPC_STRUCT_MEMBER(bool, is_shared)
  IPC_STRUCT_MEMBER(string16, name)
  IPC_STRUCT_MEMBER(int, route_id)
  IPC_STRUCT_MEMBER(int, creator_process_id)
  // Only valid for dedicated workers.
  IPC_STRUCT_MEMBER(int, creator_appcache_host_id)
  // Only valid for shared workers.
  IPC_STRUCT_MEMBER(int64, shared_worker_appcache_id)
IPC_STRUCT_END()

//-----------------------------------------------------------------------------
// WorkerProcess messages
// These are messages sent from the browser to the worker process.
IPC_MESSAGE_CONTROL1(WorkerProcessMsg_CreateWorker,
                     WorkerProcessMsg_CreateWorker_Params)

// Note: these Message Port related messages can also be sent to the
// renderer process.  Putting them here since we don't have a shared place
// like common_messages_internal.h
IPC_MESSAGE_ROUTED3(WorkerProcessMsg_Message,
                    string16 /* message */,
                    std::vector<int> /* sent_message_port_ids */,
                    std::vector<int> /* new_routing_ids */)

// Tells the Message Port Channel object that there are no more in-flight
// messages arriving.
IPC_MESSAGE_ROUTED0(WorkerProcessMsg_MessagesQueued)


//-----------------------------------------------------------------------------
// WorkerProcessHost messages
// These are messages sent from the worker process to the browser process.

// Note: these Message Port related messages can also be sent out from the
// renderer process.  Putting them here since we don't have a shared place
// like common_messages_internal.h

// Creates a new Message Port Channel object.  The first paramaeter is the
// message port channel's routing id in this process.  The second parameter
// is the process-wide-unique identifier for that port.
IPC_SYNC_MESSAGE_CONTROL0_2(WorkerProcessHostMsg_CreateMessagePort,
                            int /* route_id */,
                            int /* message_port_id */)

// Sent when a Message Port Channel object is destroyed.
IPC_MESSAGE_CONTROL1(WorkerProcessHostMsg_DestroyMessagePort,
                     int /* message_port_id */)

// Sends a message to a message port.  Optionally sends a message port as
// as well if sent_message_port_id != MSG_ROUTING_NONE.
IPC_MESSAGE_CONTROL3(WorkerProcessHostMsg_PostMessage,
                     int /* sender_message_port_id */,
                     string16 /* message */,
                     std::vector<int> /* sent_message_port_ids */)

// Causes messages sent to the remote port to be delivered to this local port.
IPC_MESSAGE_CONTROL2(WorkerProcessHostMsg_Entangle,
                     int /* local_message_port_id */,
                     int /* remote_message_port_id */)

// Causes the browser to queue messages sent to this port until the the port
// has made sure that all in-flight messages were routed to the new
// destination.
IPC_MESSAGE_CONTROL1(WorkerProcessHostMsg_QueueMessages,
                     int /* message_port_id */)

// Sends the browser all the queued messages that arrived at this message port
// after it was sent in a postMessage call.
// NOTE: MSVS can't compile the macro if std::vector<std::pair<string16, int> >
// is used, so we typedef it in worker_messages.h.
IPC_MESSAGE_CONTROL2(WorkerProcessHostMsg_SendQueuedMessages,
                     int /* message_port_id */,
                     std::vector<QueuedMessage> /* queued_messages */)

// Sent by the worker process to check whether access to web databases is
// granted by content settings.
IPC_SYNC_MESSAGE_CONTROL5_1(WorkerProcessHostMsg_AllowDatabase,
                            int /* worker_route_id */,
                            GURL /* origin url */,
                            string16 /* database name */,
                            string16 /* database display name */,
                            unsigned long /* estimated size */,
                            bool /* result */)

//-----------------------------------------------------------------------------
// Worker messages
// These are messages sent from the renderer process to the worker process.
IPC_MESSAGE_ROUTED3(WorkerMsg_StartWorkerContext,
                    GURL /* url */,
                    string16  /* user_agent */,
                    string16  /* source_code */)

IPC_MESSAGE_ROUTED0(WorkerMsg_TerminateWorkerContext)

IPC_MESSAGE_ROUTED3(WorkerMsg_PostMessage,
                    string16  /* message */,
                    std::vector<int>  /* sent_message_port_ids */,
                    std::vector<int>  /* new_routing_ids */)

IPC_MESSAGE_ROUTED2(WorkerMsg_Connect,
                    int /* sent_message_port_id */,
                    int /* routing_id */)

IPC_MESSAGE_ROUTED0(WorkerMsg_WorkerObjectDestroyed)


//-----------------------------------------------------------------------------
// WorkerHost messages
// These are messages sent from the worker process to the renderer process.
// WorkerMsg_PostMessage is also sent here.
IPC_MESSAGE_ROUTED3(WorkerHostMsg_PostExceptionToWorkerObject,
                    string16  /* error_message */,
                    int  /* line_number */,
                    string16  /* source_url*/)

IPC_MESSAGE_ROUTED1(WorkerHostMsg_PostConsoleMessageToWorkerObject,
                    WorkerHostMsg_PostConsoleMessageToWorkerObject_Params)

IPC_MESSAGE_ROUTED1(WorkerHostMsg_ConfirmMessageFromWorkerObject,
                    bool /* bool has_pending_activity */)

IPC_MESSAGE_ROUTED1(WorkerHostMsg_ReportPendingActivity,
                    bool /* bool has_pending_activity */)

IPC_MESSAGE_CONTROL1(WorkerHostMsg_WorkerContextClosed,
                     int /* worker_route_id */)
IPC_MESSAGE_ROUTED0(WorkerHostMsg_WorkerContextDestroyed)

