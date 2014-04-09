// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Message definition file, included multiple times, hence no include guard.

#include "base/strings/string16.h"
#include "content/common/service_worker/service_worker_status_code.h"
#include "content/common/service_worker/service_worker_types.h"
#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_param_traits.h"
#include "third_party/WebKit/public/platform/WebServiceWorkerError.h"
#include "third_party/WebKit/public/platform/WebServiceWorkerEventResult.h"
#include "url/gurl.h"

#undef IPC_MESSAGE_EXPORT
#define IPC_MESSAGE_EXPORT CONTENT_EXPORT

#define IPC_MESSAGE_START ServiceWorkerMsgStart

IPC_ENUM_TRAITS_MAX_VALUE(blink::WebServiceWorkerError::ErrorType,
                          blink::WebServiceWorkerError::ErrorTypeLast)

IPC_ENUM_TRAITS_MAX_VALUE(blink::WebServiceWorkerEventResult,
                          blink::WebServiceWorkerEventResultLast)

IPC_STRUCT_TRAITS_BEGIN(content::ServiceWorkerFetchRequest)
  IPC_STRUCT_TRAITS_MEMBER(url)
  IPC_STRUCT_TRAITS_MEMBER(method)
  IPC_STRUCT_TRAITS_MEMBER(headers)
IPC_STRUCT_TRAITS_END()

IPC_ENUM_TRAITS_MAX_VALUE(content::ServiceWorkerFetchEventResult,
                          content::SERVICE_WORKER_FETCH_EVENT_LAST)

IPC_STRUCT_TRAITS_BEGIN(content::ServiceWorkerResponse)
  IPC_STRUCT_TRAITS_MEMBER(status_code)
  IPC_STRUCT_TRAITS_MEMBER(status_text)
  IPC_STRUCT_TRAITS_MEMBER(method)
  IPC_STRUCT_TRAITS_MEMBER(headers)
IPC_STRUCT_TRAITS_END()

// Messages sent from the child process to the browser.

IPC_MESSAGE_CONTROL4(ServiceWorkerHostMsg_RegisterServiceWorker,
                     int32 /* thread_id */,
                     int32 /* request_id */,
                     GURL /* scope */,
                     GURL /* script_url */)

IPC_MESSAGE_CONTROL3(ServiceWorkerHostMsg_UnregisterServiceWorker,
                     int32 /* thread_id */,
                     int32 /* request_id */,
                     GURL /* scope (url pattern) */)

// Sends a 'message' event to a service worker (renderer->browser).
IPC_MESSAGE_CONTROL3(ServiceWorkerHostMsg_PostMessage,
                     int64 /* version_id */,
                     base::string16 /* message */,
                     std::vector<int> /* sent_message_port_ids */)

// Informs the browser of a new ServiceWorkerProvider in the child process,
// |provider_id| is unique within its child process.
IPC_MESSAGE_CONTROL1(ServiceWorkerHostMsg_ProviderCreated,
                     int /* provider_id */)

// Informs the browser of a ServiceWorkerProvider being destroyed.
IPC_MESSAGE_CONTROL1(ServiceWorkerHostMsg_ProviderDestroyed,
                     int /* provider_id */)

// Informs the browser of a ServiceWorker object being destroyed.
IPC_MESSAGE_CONTROL1(ServiceWorkerHostMsg_ServiceWorkerObjectDestroyed,
                     int /* handle_id */)

// Informs the browser that |provider_id| is associated
// with a service worker script running context and
// |version_id| identifies which ServcieWorkerVersion.
IPC_MESSAGE_CONTROL2(ServiceWorkerHostMsg_SetVersionId,
                     int /* provider_id */,
                     int64 /* version_id */)

// Informs the browser of a new scriptable API client in the child process.
IPC_MESSAGE_CONTROL2(ServiceWorkerHostMsg_AddScriptClient,
                     int /* thread_id */,
                     int /* provider_id */)

// Informs the browser that the scriptable API client is unregistered.
IPC_MESSAGE_CONTROL2(ServiceWorkerHostMsg_RemoveScriptClient,
                     int /* thread_id */,
                     int /* provider_id */)

// Informs the browser that install event handling has finished.
// Sent via EmbeddedWorker.
IPC_MESSAGE_CONTROL1(ServiceWorkerHostMsg_InstallEventFinished,
                     blink::WebServiceWorkerEventResult)

IPC_MESSAGE_CONTROL1(ServiceWorkerHostMsg_ActivateEventFinished,
                     blink::WebServiceWorkerEventResult);

// Informs the browser that fetch event handling has finished.
// Sent via EmbeddedWorker.
IPC_MESSAGE_CONTROL2(ServiceWorkerHostMsg_FetchEventFinished,
                     content::ServiceWorkerFetchEventResult,
                     content::ServiceWorkerResponse)

// Messages sent from the browser to the child process.

// Response to ServiceWorkerMsg_RegisterServiceWorker
IPC_MESSAGE_CONTROL3(ServiceWorkerMsg_ServiceWorkerRegistered,
                     int32 /* thread_id */,
                     int32 /* request_id */,
                     int /* handle_id */)

// Response to ServiceWorkerMsg_UnregisterServiceWorker
IPC_MESSAGE_CONTROL2(ServiceWorkerMsg_ServiceWorkerUnregistered,
                     int32 /* thread_id */,
                     int32 /* request_id */)

// Sent when any kind of registration error occurs during a
// RegisterServiceWorker / UnregisterServiceWorker handler above.
IPC_MESSAGE_CONTROL4(ServiceWorkerMsg_ServiceWorkerRegistrationError,
                     int32 /* thread_id */,
                     int32 /* request_id */,
                     blink::WebServiceWorkerError::ErrorType /* code */,
                     base::string16 /* message */)

// Sent via EmbeddedWorker to dispatch install event.
IPC_MESSAGE_CONTROL1(ServiceWorkerMsg_InstallEvent, int /* active_version_id */)

IPC_MESSAGE_CONTROL0(ServiceWorkerMsg_ActivateEvent)

// Sent via EmbeddedWorker to dispatch fetch event.
IPC_MESSAGE_CONTROL1(ServiceWorkerMsg_FetchEvent,
                     content::ServiceWorkerFetchRequest)

// Sends a 'message' event to a service worker (browser->EmbeddedWorker).
IPC_MESSAGE_CONTROL3(ServiceWorkerMsg_Message,
                     base::string16 /* message */,
                     std::vector<int> /* sent_message_port_ids */,
                     std::vector<int> /* new_routing_ids */)

// Sent via EmbeddedWorker to dispatch sync event.
IPC_MESSAGE_CONTROL0(ServiceWorkerMsg_SyncEvent)

// Informs the browser that sync event handling has finished.
IPC_MESSAGE_CONTROL0(ServiceWorkerHostMsg_SyncEventFinished)
