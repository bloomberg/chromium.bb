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
#include "content/common/service_worker/service_worker_client_info.h"
#include "content/common/service_worker/service_worker_status_code.h"
#include "content/common/service_worker/service_worker_types.h"
#include "content/public/common/platform_notification_data.h"
#include "content/public/common/push_event_payload.h"
#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_param_traits.h"
#include "services/network/public/interfaces/fetch_api.mojom.h"
#include "third_party/WebKit/common/message_port/message_port_channel.h"
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

IPC_ENUM_TRAITS_MAX_VALUE(blink::WebServiceWorkerResponseError,
                          blink::kWebServiceWorkerResponseErrorLast)

IPC_ENUM_TRAITS_MAX_VALUE(blink::WebServiceWorkerClientType,
                          blink::kWebServiceWorkerClientTypeLast)

IPC_ENUM_TRAITS_MAX_VALUE(network::mojom::FetchResponseType,
                          network::mojom::FetchResponseType::kLast)

IPC_ENUM_TRAITS_MAX_VALUE(content::ServiceWorkerProviderType,
                          content::SERVICE_WORKER_PROVIDER_TYPE_LAST)

IPC_ENUM_TRAITS_MAX_VALUE(content::ServiceWorkerFetchType,
                          content::ServiceWorkerFetchType::LAST)

IPC_STRUCT_TRAITS_BEGIN(content::ExtendableMessageEventSource)
  IPC_STRUCT_TRAITS_MEMBER(client_info)
  IPC_STRUCT_TRAITS_MEMBER(service_worker_info)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(content::NavigationPreloadState)
  IPC_STRUCT_TRAITS_MEMBER(enabled)
  IPC_STRUCT_TRAITS_MEMBER(header)
IPC_STRUCT_TRAITS_END()

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
  IPC_STRUCT_TRAITS_MEMBER(client_id)
  IPC_STRUCT_TRAITS_MEMBER(is_reload)
  IPC_STRUCT_TRAITS_MEMBER(fetch_type)
IPC_STRUCT_TRAITS_END()

IPC_ENUM_TRAITS_MAX_VALUE(content::ServiceWorkerFetchEventResult,
                          content::SERVICE_WORKER_FETCH_EVENT_LAST)

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
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(content::ServiceWorkerObjectInfo)
  IPC_STRUCT_TRAITS_MEMBER(handle_id)
  IPC_STRUCT_TRAITS_MEMBER(url)
  IPC_STRUCT_TRAITS_MEMBER(state)
  IPC_STRUCT_TRAITS_MEMBER(version_id)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(content::ServiceWorkerVersionAttributes)
  IPC_STRUCT_TRAITS_MEMBER(installing)
  IPC_STRUCT_TRAITS_MEMBER(waiting)
  IPC_STRUCT_TRAITS_MEMBER(active)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(content::ServiceWorkerClientInfo)
  IPC_STRUCT_TRAITS_MEMBER(client_uuid)
  IPC_STRUCT_TRAITS_MEMBER(page_visibility_state)
  IPC_STRUCT_TRAITS_MEMBER(is_focused)
  IPC_STRUCT_TRAITS_MEMBER(url)
  IPC_STRUCT_TRAITS_MEMBER(frame_type)
  IPC_STRUCT_TRAITS_MEMBER(client_type)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(content::ServiceWorkerClientQueryOptions)
  IPC_STRUCT_TRAITS_MEMBER(client_type)
  IPC_STRUCT_TRAITS_MEMBER(include_uncontrolled)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_BEGIN(ServiceWorkerMsg_MessageToDocument_Params)
  IPC_STRUCT_MEMBER(int, thread_id)
  IPC_STRUCT_MEMBER(int, provider_id)
  IPC_STRUCT_MEMBER(content::ServiceWorkerObjectInfo, service_worker_info)
  IPC_STRUCT_MEMBER(base::string16, message)
  IPC_STRUCT_MEMBER(std::vector<blink::MessagePortChannel>, message_ports)
IPC_STRUCT_END()

IPC_STRUCT_TRAITS_BEGIN(content::PushEventPayload)
  IPC_STRUCT_TRAITS_MEMBER(data)
  IPC_STRUCT_TRAITS_MEMBER(is_null)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_BEGIN(ServiceWorkerMsg_SetControllerServiceWorker_Params)
  IPC_STRUCT_MEMBER(int, thread_id)
  IPC_STRUCT_MEMBER(int, provider_id)
  IPC_STRUCT_MEMBER(content::ServiceWorkerObjectInfo, object_info)
  IPC_STRUCT_MEMBER(bool, should_notify_controllerchange)

  // |used_features| is the set of features that the worker has used.
  // The values must be from blink::UseCounter::Feature enum.
  IPC_STRUCT_MEMBER(std::set<uint32_t>, used_features)
IPC_STRUCT_END()

//---------------------------------------------------------------------------
// Messages sent from the child process to the browser.

IPC_MESSAGE_CONTROL4(ServiceWorkerHostMsg_UpdateServiceWorker,
                     int /* thread_id */,
                     int /* request_id */,
                     int /* provider_id */,
                     int64_t /* registration_id */)

IPC_MESSAGE_CONTROL4(ServiceWorkerHostMsg_UnregisterServiceWorker,
                     int /* thread_id */,
                     int /* request_id */,
                     int /* provider_id */,
                     int64_t /* registration_id */)

// Asks the browser to enable/disable navigation preload for a registration.
IPC_MESSAGE_CONTROL5(ServiceWorkerHostMsg_EnableNavigationPreload,
                     int /* thread_id */,
                     int /* request_id */,
                     int /* provider_id */,
                     int64_t /* registration_id */,
                     bool /* enable */)

// Asks the browser to get navigation preload state for a registration.
IPC_MESSAGE_CONTROL4(ServiceWorkerHostMsg_GetNavigationPreloadState,
                     int /* thread_id */,
                     int /* request_id */,
                     int /* provider_id */,
                     int64_t /* registration_id */)

// Asks the browser to set navigation preload header value for a registration.
IPC_MESSAGE_CONTROL5(ServiceWorkerHostMsg_SetNavigationPreloadHeader,
                     int /* thread_id */,
                     int /* request_id */,
                     int /* provider_id */,
                     int64_t /* registration_id */,
                     std::string /* header_value */)

// Sends ExtendableMessageEvent to a service worker (renderer->browser).
IPC_MESSAGE_CONTROL5(
    ServiceWorkerHostMsg_PostMessageToWorker,
    int /* handle_id */,
    int /* provider_id */,
    base::string16 /* message */,
    url::Origin /* source_origin */,
    std::vector<blink::MessagePortChannel> /* sent_message_ports */)

// Increments and decrements the ServiceWorker object's reference
// counting in the browser side. The ServiceWorker object is created
// with ref-count==1 initially.
IPC_MESSAGE_CONTROL1(ServiceWorkerHostMsg_IncrementServiceWorkerRefCount,
                     int /* handle_id */)
IPC_MESSAGE_CONTROL1(ServiceWorkerHostMsg_DecrementServiceWorkerRefCount,
                     int /* handle_id */)

// Tells the browser to terminate a service worker. Used in layout tests to
// verify behavior when a service worker isn't running.
IPC_MESSAGE_CONTROL1(ServiceWorkerHostMsg_TerminateWorker,
                     int /* handle_id */)

// Asks the browser to retrieve client of the sender ServiceWorker.
IPC_MESSAGE_ROUTED2(ServiceWorkerHostMsg_GetClient,
                    int /* request_id */,
                    std::string /* client_uuid */)

// Asks the browser to retrieve clients of the sender ServiceWorker.
IPC_MESSAGE_ROUTED2(ServiceWorkerHostMsg_GetClients,
                    int /* request_id */,
                    content::ServiceWorkerClientQueryOptions)

// Sends MessageEvent to a client (renderer->browser).
IPC_MESSAGE_ROUTED3(
    ServiceWorkerHostMsg_PostMessageToClient,
    std::string /* uuid */,
    base::string16 /* message */,
    std::vector<blink::MessagePortChannel> /* sent_message_ports */)

// ServiceWorker -> Browser message to request that the ServiceWorkerStorage
// cache |data| associated with |url|.
IPC_MESSAGE_ROUTED2(ServiceWorkerHostMsg_SetCachedMetadata,
                    GURL /* url */,
                    std::vector<char> /* data */)

// ServiceWorker -> Browser message to request that the ServiceWorkerStorage
// clear the cache associated with |url|.
IPC_MESSAGE_ROUTED1(ServiceWorkerHostMsg_ClearCachedMetadata, GURL /* url */)

// Ask the browser to open a tab/window (renderer->browser).
IPC_MESSAGE_ROUTED2(ServiceWorkerHostMsg_OpenNewTab,
                    int /* request_id */,
                    GURL /* url */)

// Ask the browser to open a popup tab/window (renderer->browser).
IPC_MESSAGE_ROUTED2(ServiceWorkerHostMsg_OpenNewPopup,
                    int /* request_id */,
                    GURL /* url */)

// Ask the browser to focus a client (renderer->browser).
IPC_MESSAGE_ROUTED2(ServiceWorkerHostMsg_FocusClient,
                    int /* request_id */,
                    std::string /* uuid */)

// Ask the browser to navigate a client (renderer->browser).
IPC_MESSAGE_ROUTED3(ServiceWorkerHostMsg_NavigateClient,
                    int /* request_id */,
                    std::string /* uuid */,
                    GURL /* url */)

// Asks the browser to force this worker to become activated.
IPC_MESSAGE_ROUTED1(ServiceWorkerHostMsg_SkipWaiting,
                    int /* request_id */)

// Asks the browser to have this worker take control of pages that match
// its scope.
IPC_MESSAGE_ROUTED1(ServiceWorkerHostMsg_ClaimClients,
                    int /* request_id */)

//---------------------------------------------------------------------------
// Messages sent from the browser to the child process.
//
// NOTE: All ServiceWorkerMsg messages not sent via EmbeddedWorker must have
// a thread_id as their first field so that ServiceWorkerMessageFilter can
// extract it and dispatch the message to the correct ServiceWorkerDispatcher
// on the correct thread.

// Response to ServiceWorkerHostMsg_UpdateServiceWorker.
IPC_MESSAGE_CONTROL2(ServiceWorkerMsg_ServiceWorkerUpdated,
                     int /* thread_id */,
                     int /* request_id */)

// Response to ServiceWorkerHostMsg_UnregisterServiceWorker.
IPC_MESSAGE_CONTROL3(ServiceWorkerMsg_ServiceWorkerUnregistered,
                     int /* thread_id */,
                     int /* request_id */,
                     bool /* is_success */)

// Sent when any kind of update error occurs during a
// UpdateServiceWorker handler above.
IPC_MESSAGE_CONTROL4(ServiceWorkerMsg_ServiceWorkerUpdateError,
                     int /* thread_id */,
                     int /* request_id */,
                     blink::mojom::ServiceWorkerErrorType,
                     base::string16 /* message */)

// Sent when any kind of registration error occurs during a
// UnregisterServiceWorker handler above.
IPC_MESSAGE_CONTROL4(ServiceWorkerMsg_ServiceWorkerUnregistrationError,
                     int /* thread_id */,
                     int /* request_id */,
                     blink::mojom::ServiceWorkerErrorType,
                     base::string16 /* message */)

// Informs the child process that the ServiceWorker's state has changed.
IPC_MESSAGE_CONTROL3(ServiceWorkerMsg_ServiceWorkerStateChanged,
                     int /* thread_id */,
                     int /* handle_id */,
                     blink::mojom::ServiceWorkerState)

// Tells the child process to set service workers for the given registration.
IPC_MESSAGE_CONTROL4(ServiceWorkerMsg_SetVersionAttributes,
                     int /* thread_id */,
                     int /* registration_handle_id */,
                     int /* changed_mask */,
                     content::ServiceWorkerVersionAttributes)

// Informs the child process that new ServiceWorker enters the installation
// phase.
IPC_MESSAGE_CONTROL2(ServiceWorkerMsg_UpdateFound,
                     int /* thread_id */,
                     int /* registration_handle_id */)

// Tells the child process to set the controller ServiceWorker for the given
// provider.
IPC_MESSAGE_CONTROL1(ServiceWorkerMsg_SetControllerServiceWorker,
                     ServiceWorkerMsg_SetControllerServiceWorker_Params)

IPC_MESSAGE_CONTROL2(ServiceWorkerMsg_DidEnableNavigationPreload,
                     int /* thread_id */,
                     int /* request_id */)
IPC_MESSAGE_CONTROL4(ServiceWorkerMsg_EnableNavigationPreloadError,
                     int /* thread_id */,
                     int /* request_id */,
                     blink::mojom::ServiceWorkerErrorType,
                     std::string /* message */)
IPC_MESSAGE_CONTROL3(ServiceWorkerMsg_DidGetNavigationPreloadState,
                     int /* thread_id */,
                     int /* request_id */,
                     content::NavigationPreloadState /* state */)
IPC_MESSAGE_CONTROL4(ServiceWorkerMsg_GetNavigationPreloadStateError,
                     int /* thread_id */,
                     int /* request_id */,
                     blink::mojom::ServiceWorkerErrorType,
                     std::string /* message */)
IPC_MESSAGE_CONTROL2(ServiceWorkerMsg_DidSetNavigationPreloadHeader,
                     int /* thread_id */,
                     int /* request_id */)
IPC_MESSAGE_CONTROL4(ServiceWorkerMsg_SetNavigationPreloadHeaderError,
                     int /* thread_id */,
                     int /* request_id */,
                     blink::mojom::ServiceWorkerErrorType,
                     std::string /* message */)

// Sends MessageEvent to a client document (browser->renderer).
IPC_MESSAGE_CONTROL1(ServiceWorkerMsg_MessageToDocument,
                     ServiceWorkerMsg_MessageToDocument_Params)

// Notifies a client that its controller used a feature, for UseCounter
// purposes (browser->renderer). |feature| must be one of the values from
// blink::UseCounter::Feature enum.
IPC_MESSAGE_CONTROL3(ServiceWorkerMsg_CountFeature,
                     int /* thread_id */,
                     int /* provider_id */,
                     uint32_t /* feature */)

// Sent via EmbeddedWorker to dispatch events.
IPC_MESSAGE_CONTROL1(ServiceWorkerMsg_DidSkipWaiting,
                     int /* request_id */)
IPC_MESSAGE_CONTROL1(ServiceWorkerMsg_DidClaimClients,
                     int /* request_id */)
IPC_MESSAGE_CONTROL3(ServiceWorkerMsg_ClaimClientsError,
                     int /* request_id */,
                     blink::mojom::ServiceWorkerErrorType,
                     base::string16 /* message */)

// Sent via EmbeddedWorker as a response of GetClient.
IPC_MESSAGE_CONTROL2(ServiceWorkerMsg_DidGetClient,
                     int /* request_id */,
                     content::ServiceWorkerClientInfo)

// Sent via EmbeddedWorker as a response of GetClients.
IPC_MESSAGE_CONTROL2(ServiceWorkerMsg_DidGetClients,
                     int /* request_id */,
                     std::vector<content::ServiceWorkerClientInfo>)

// Sent via EmbeddedWorker as a response of OpenWindow.
IPC_MESSAGE_CONTROL2(ServiceWorkerMsg_OpenWindowResponse,
                     int /* request_id */,
                     content::ServiceWorkerClientInfo /* client */)

// Sent via EmbeddedWorker as an error response of OpenWindow.
IPC_MESSAGE_CONTROL2(ServiceWorkerMsg_OpenWindowError,
                     int /* request_id */,
                     std::string /* message */ )

// Sent via EmbeddedWorker as a response of FocusClient.
IPC_MESSAGE_CONTROL2(ServiceWorkerMsg_FocusClientResponse,
                     int /* request_id */,
                     content::ServiceWorkerClientInfo /* client */)

// Sent via EmbeddedWorker as a response of NavigateClient.
IPC_MESSAGE_CONTROL2(ServiceWorkerMsg_NavigateClientResponse,
                     int /* request_id */,
                     content::ServiceWorkerClientInfo /* client */)

// Sent via EmbeddedWorker as an error response of NavigateClient.
IPC_MESSAGE_CONTROL2(ServiceWorkerMsg_NavigateClientError,
                     int /* request_id */,
                     GURL /* url */)

#endif  // CONTENT_COMMON_SERVICE_WORKER_SERVICE_WORKER_MESSAGES_H_
