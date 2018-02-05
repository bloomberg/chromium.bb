// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_SERVICE_WORKER_SERVICE_WORKER_MESSAGES_H_
#define CONTENT_COMMON_SERVICE_WORKER_SERVICE_WORKER_MESSAGES_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "base/strings/string16.h"
#include "base/time/time.h"
#include "content/common/service_worker/service_worker_status_code.h"
#include "content/common/service_worker/service_worker_types.h"
#include "content/public/common/platform_notification_data.h"
#include "content/public/common/push_event_payload.h"
#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_param_traits.h"
#include "services/network/public/interfaces/fetch_api.mojom.h"
#include "third_party/WebKit/common/message_port/message_port_channel.h"
#include "third_party/WebKit/common/message_port/transferable_message.h"
#include "third_party/WebKit/public/platform/modules/serviceworker/WebServiceWorkerError.h"
#include "url/gurl.h"
#include "url/origin.h"

#undef IPC_MESSAGE_EXPORT
#define IPC_MESSAGE_EXPORT CONTENT_EXPORT

#define IPC_MESSAGE_START ServiceWorkerMsgStart

IPC_ENUM_TRAITS_MAX_VALUE(blink::mojom::ServiceWorkerErrorType,
                          blink::mojom::ServiceWorkerErrorType::kLast)

IPC_ENUM_TRAITS_MAX_VALUE(blink::mojom::ServiceWorkerState,
                          blink::mojom::ServiceWorkerState::kLast)

IPC_ENUM_TRAITS_MAX_VALUE(blink::mojom::ServiceWorkerResponseError,
                          blink::mojom::ServiceWorkerResponseError::kLast)

IPC_ENUM_TRAITS_MAX_VALUE(blink::mojom::ServiceWorkerClientType,
                          blink::mojom::ServiceWorkerClientType::kLast)

IPC_ENUM_TRAITS_MAX_VALUE(content::ServiceWorkerFetchType,
                          content::ServiceWorkerFetchType::LAST)

IPC_STRUCT_TRAITS_BEGIN(content::ServiceWorkerFetchRequest)
  IPC_STRUCT_TRAITS_MEMBER(mode)
  IPC_STRUCT_TRAITS_MEMBER(is_main_resource_load)
  IPC_STRUCT_TRAITS_MEMBER(request_context_type)
  IPC_STRUCT_TRAITS_MEMBER(frame_type)
  IPC_STRUCT_TRAITS_MEMBER(url)
  IPC_STRUCT_TRAITS_MEMBER(method)
  IPC_STRUCT_TRAITS_MEMBER(headers)
  IPC_STRUCT_TRAITS_MEMBER(blob_uuid)
  IPC_STRUCT_TRAITS_MEMBER(blob_size)
  IPC_STRUCT_TRAITS_MEMBER(blob)
  IPC_STRUCT_TRAITS_MEMBER(referrer)
  IPC_STRUCT_TRAITS_MEMBER(credentials_mode)
  IPC_STRUCT_TRAITS_MEMBER(redirect_mode)
  IPC_STRUCT_TRAITS_MEMBER(integrity)
  IPC_STRUCT_TRAITS_MEMBER(keepalive)
  IPC_STRUCT_TRAITS_MEMBER(client_id)
  IPC_STRUCT_TRAITS_MEMBER(is_reload)
  IPC_STRUCT_TRAITS_MEMBER(fetch_type)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(content::ServiceWorkerResponse)
  IPC_STRUCT_TRAITS_MEMBER(url_list)
  IPC_STRUCT_TRAITS_MEMBER(status_code)
  IPC_STRUCT_TRAITS_MEMBER(status_text)
  IPC_STRUCT_TRAITS_MEMBER(response_type)
  IPC_STRUCT_TRAITS_MEMBER(headers)
  IPC_STRUCT_TRAITS_MEMBER(blob_uuid)
  IPC_STRUCT_TRAITS_MEMBER(blob_size)
  IPC_STRUCT_TRAITS_MEMBER(blob)
  IPC_STRUCT_TRAITS_MEMBER(error)
  IPC_STRUCT_TRAITS_MEMBER(response_time)
  IPC_STRUCT_TRAITS_MEMBER(is_in_cache_storage)
  IPC_STRUCT_TRAITS_MEMBER(cache_storage_cache_name)
  IPC_STRUCT_TRAITS_MEMBER(cors_exposed_header_names)
  IPC_STRUCT_TRAITS_MEMBER(side_data_blob_uuid)
  IPC_STRUCT_TRAITS_MEMBER(side_data_blob_size)
  IPC_STRUCT_TRAITS_MEMBER(side_data_blob)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(blink::mojom::ServiceWorkerClientInfo)
  IPC_STRUCT_TRAITS_MEMBER(client_uuid)
  IPC_STRUCT_TRAITS_MEMBER(page_visibility_state)
  IPC_STRUCT_TRAITS_MEMBER(is_focused)
  IPC_STRUCT_TRAITS_MEMBER(url)
  IPC_STRUCT_TRAITS_MEMBER(frame_type)
  IPC_STRUCT_TRAITS_MEMBER(client_type)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(content::PushEventPayload)
  IPC_STRUCT_TRAITS_MEMBER(data)
  IPC_STRUCT_TRAITS_MEMBER(is_null)
IPC_STRUCT_TRAITS_END()

//---------------------------------------------------------------------------
// Messages sent from the child process to the browser.

// Sends ExtendableMessageEvent to a service worker (renderer->browser).
IPC_MESSAGE_CONTROL4(
    ServiceWorkerHostMsg_PostMessageToWorker,
    int /* handle_id */,
    int /* provider_id */,
    scoped_refptr<
        base::RefCountedData<blink::TransferableMessage>> /* message */,
    url::Origin /* source_origin */)

// Tells the browser to terminate a service worker. Used in layout tests to
// verify behavior when a service worker isn't running.
IPC_MESSAGE_CONTROL1(ServiceWorkerHostMsg_TerminateWorker,
                     int /* handle_id */)

// Sends MessageEvent to a client (renderer->browser).
IPC_MESSAGE_ROUTED2(
    ServiceWorkerHostMsg_PostMessageToClient,
    std::string /* uuid */,
    scoped_refptr<
        base::RefCountedData<blink::TransferableMessage>> /* message */)

// Ask the browser to focus a client (renderer->browser).
IPC_MESSAGE_ROUTED2(ServiceWorkerHostMsg_FocusClient,
                    int /* request_id */,
                    std::string /* uuid */)

// Ask the browser to navigate a client (renderer->browser).
IPC_MESSAGE_ROUTED3(ServiceWorkerHostMsg_NavigateClient,
                    int /* request_id */,
                    std::string /* uuid */,
                    GURL /* url */)

//---------------------------------------------------------------------------
// Messages sent from the browser to the child process.
//
// NOTE: All ServiceWorkerMsg messages not sent via EmbeddedWorker must have
// a thread_id as their first field so that ServiceWorkerMessageFilter can
// extract it and dispatch the message to the correct ServiceWorkerDispatcher
// on the correct thread.

// Informs the child process that the ServiceWorker's state has changed.
IPC_MESSAGE_CONTROL3(ServiceWorkerMsg_ServiceWorkerStateChanged,
                     int /* thread_id */,
                     int /* handle_id */,
                     blink::mojom::ServiceWorkerState)

// Sent via EmbeddedWorker as a response of FocusClient.
IPC_MESSAGE_CONTROL2(ServiceWorkerMsg_FocusClientResponse,
                     int /* request_id */,
                     blink::mojom::ServiceWorkerClientInfo /* client */)

// Sent via EmbeddedWorker as a response of NavigateClient.
IPC_MESSAGE_CONTROL2(ServiceWorkerMsg_NavigateClientResponse,
                     int /* request_id */,
                     blink::mojom::ServiceWorkerClientInfo /* client */)

// Sent via EmbeddedWorker as an error response of NavigateClient.
IPC_MESSAGE_CONTROL2(ServiceWorkerMsg_NavigateClientError,
                     int /* request_id */,
                     GURL /* url */)

#endif  // CONTENT_COMMON_SERVICE_WORKER_SERVICE_WORKER_MESSAGES_H_
