// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Message definition file, included multiple times, hence no include guard.

#include <stdint.h>

#include <string>
#include <vector>

#include "base/strings/string16.h"
#include "base/time/time.h"
#include "content/common/message_port.h"
#include "content/common/service_worker/service_worker_client_info.h"
#include "content/common/service_worker/service_worker_status_code.h"
#include "content/common/service_worker/service_worker_types.h"
#include "content/public/common/platform_notification_data.h"
#include "content/public/common/push_event_payload.h"
#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_param_traits.h"
#include "third_party/WebKit/public/platform/modules/serviceworker/WebServiceWorkerError.h"
#include "third_party/WebKit/public/platform/modules/serviceworker/WebServiceWorkerEventResult.h"
#include "url/gurl.h"
#include "url/origin.h"

#undef IPC_MESSAGE_EXPORT
#define IPC_MESSAGE_EXPORT CONTENT_EXPORT

#define IPC_MESSAGE_START ServiceWorkerMsgStart

IPC_ENUM_TRAITS_MAX_VALUE(blink::WebServiceWorkerError::ErrorType,
                          blink::WebServiceWorkerError::kErrorTypeLast)

IPC_ENUM_TRAITS_MAX_VALUE(blink::WebServiceWorkerEventResult,
                          blink::kWebServiceWorkerEventResultLast)

IPC_ENUM_TRAITS_MAX_VALUE(blink::WebServiceWorkerState,
                          blink::kWebServiceWorkerStateLast)

IPC_ENUM_TRAITS_MAX_VALUE(blink::WebServiceWorkerResponseType,
                          blink::kWebServiceWorkerResponseTypeLast)

IPC_ENUM_TRAITS_MAX_VALUE(blink::WebServiceWorkerResponseError,
                          blink::kWebServiceWorkerResponseErrorLast)

IPC_ENUM_TRAITS_MAX_VALUE(blink::WebServiceWorkerClientType,
                          blink::kWebServiceWorkerClientTypeLast)

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
  IPC_STRUCT_TRAITS_MEMBER(referrer)
  IPC_STRUCT_TRAITS_MEMBER(credentials_mode)
  IPC_STRUCT_TRAITS_MEMBER(redirect_mode)
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

IPC_STRUCT_TRAITS_BEGIN(content::ServiceWorkerRegistrationObjectInfo)
  IPC_STRUCT_TRAITS_MEMBER(handle_id)
  IPC_STRUCT_TRAITS_MEMBER(scope)
  IPC_STRUCT_TRAITS_MEMBER(registration_id)
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
  IPC_STRUCT_MEMBER(std::vector<content::MessagePort>, message_ports)
IPC_STRUCT_END()

IPC_STRUCT_TRAITS_BEGIN(content::PushEventPayload)
  IPC_STRUCT_TRAITS_MEMBER(data)
  IPC_STRUCT_TRAITS_MEMBER(is_null)
IPC_STRUCT_TRAITS_END()

//---------------------------------------------------------------------------
// Messages sent from the child process to the browser.

IPC_MESSAGE_CONTROL5(ServiceWorkerHostMsg_RegisterServiceWorker,
                     int /* thread_id */,
                     int /* request_id */,
                     int /* provider_id */,
                     GURL /* scope */,
                     GURL /* script_url */)

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

IPC_MESSAGE_CONTROL4(ServiceWorkerHostMsg_GetRegistration,
                     int /* thread_id */,
                     int /* request_id */,
                     int /* provider_id */,
                     GURL /* document_url */)

IPC_MESSAGE_CONTROL3(ServiceWorkerHostMsg_GetRegistrations,
                     int /* thread_id */,
                     int /* request_id */,
                     int /* provider_id */)

IPC_MESSAGE_CONTROL3(ServiceWorkerHostMsg_GetRegistrationForReady,
                     int /* thread_id */,
                     int /* request_id */,
                     int /* provider_id */)

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
    std::vector<content::MessagePort> /* sent_message_ports */)

// Increments and decrements the ServiceWorker object's reference
// counting in the browser side. The ServiceWorker object is created
// with ref-count==1 initially.
IPC_MESSAGE_CONTROL1(ServiceWorkerHostMsg_IncrementServiceWorkerRefCount,
                     int /* handle_id */)
IPC_MESSAGE_CONTROL1(ServiceWorkerHostMsg_DecrementServiceWorkerRefCount,
                     int /* handle_id */)

// Increments and decrements the ServiceWorkerRegistration object's reference
// counting in the browser side. The registration object is created with
// ref-count==1 initially.
IPC_MESSAGE_CONTROL1(ServiceWorkerHostMsg_IncrementRegistrationRefCount,
                     int /* registration_handle_id */)
IPC_MESSAGE_CONTROL1(ServiceWorkerHostMsg_DecrementRegistrationRefCount,
                     int /* registration_handle_id */)

// Tells the browser to terminate a service worker. Used in layout tests to
// verify behavior when a service worker isn't running.
IPC_MESSAGE_CONTROL1(ServiceWorkerHostMsg_TerminateWorker,
                     int /* handle_id */)

// Returns the response as the result of fetch event. This is used only for blob
// to keep the IPC ordering. Mojo IPC is used when the response body is a stream
// or is empty, and for the fallback-to-network response.
IPC_MESSAGE_ROUTED3(ServiceWorkerHostMsg_FetchEventResponse,
                    int /* fetch_event_id */,
                    content::ServiceWorkerResponse,
                    base::Time /* dispatch_event_time */)

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
    std::vector<content::MessagePort> /* sent_message_ports */)

// ServiceWorker -> Browser message to request that the ServiceWorkerStorage
// cache |data| associated with |url|.
IPC_MESSAGE_ROUTED2(ServiceWorkerHostMsg_SetCachedMetadata,
                    GURL /* url */,
                    std::vector<char> /* data */)

// ServiceWorker -> Browser message to request that the ServiceWorkerStorage
// clear the cache associated with |url|.
IPC_MESSAGE_ROUTED1(ServiceWorkerHostMsg_ClearCachedMetadata, GURL /* url */)

// Ask the browser to open a tab/window (renderer->browser).
IPC_MESSAGE_ROUTED2(ServiceWorkerHostMsg_OpenWindow,
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

// Informs the browser of new foreign fetch subscopes this worker wants to
// handle. Should only be sent while an install event is being handled.
IPC_MESSAGE_ROUTED2(ServiceWorkerHostMsg_RegisterForeignFetchScopes,
                    std::vector<GURL> /* sub_scopes */,
                    std::vector<url::Origin> /* origins */)

//---------------------------------------------------------------------------
// Messages sent from the browser to the child process.
//
// NOTE: All ServiceWorkerMsg messages not sent via EmbeddedWorker must have
// a thread_id as their first field so that ServiceWorkerMessageFilter can
// extract it and dispatch the message to the correct ServiceWorkerDispatcher
// on the correct thread.

// Informs the child process that the given provider gets associated or
// disassociated with the registration.
IPC_MESSAGE_CONTROL4(ServiceWorkerMsg_AssociateRegistration,
                     int /* thread_id */,
                     int /* provider_id */,
                     content::ServiceWorkerRegistrationObjectInfo,
                     content::ServiceWorkerVersionAttributes)
IPC_MESSAGE_CONTROL2(ServiceWorkerMsg_DisassociateRegistration,
                     int /* thread_id */,
                     int /* provider_id */)

// Response to ServiceWorkerHostMsg_RegisterServiceWorker.
IPC_MESSAGE_CONTROL4(ServiceWorkerMsg_ServiceWorkerRegistered,
                     int /* thread_id */,
                     int /* request_id */,
                     content::ServiceWorkerRegistrationObjectInfo,
                     content::ServiceWorkerVersionAttributes)

// Response to ServiceWorkerHostMsg_UpdateServiceWorker.
IPC_MESSAGE_CONTROL2(ServiceWorkerMsg_ServiceWorkerUpdated,
                     int /* thread_id */,
                     int /* request_id */)

// Response to ServiceWorkerHostMsg_UnregisterServiceWorker.
IPC_MESSAGE_CONTROL3(ServiceWorkerMsg_ServiceWorkerUnregistered,
                     int /* thread_id */,
                     int /* request_id */,
                     bool /* is_success */)

// Response to ServiceWorkerHostMsg_GetRegistration.
IPC_MESSAGE_CONTROL4(ServiceWorkerMsg_DidGetRegistration,
                     int /* thread_id */,
                     int /* request_id */,
                     content::ServiceWorkerRegistrationObjectInfo,
                     content::ServiceWorkerVersionAttributes)

// Response to ServiceWorkerHostMsg_GetRegistrations.
IPC_MESSAGE_CONTROL4(ServiceWorkerMsg_DidGetRegistrations,
                     int /* thread_id */,
                     int /* request_id */,
                     std::vector<content::ServiceWorkerRegistrationObjectInfo>,
                     std::vector<content::ServiceWorkerVersionAttributes>)

// Response to ServiceWorkerHostMsg_GetRegistrationForReady.
IPC_MESSAGE_CONTROL4(ServiceWorkerMsg_DidGetRegistrationForReady,
                     int /* thread_id */,
                     int /* request_id */,
                     content::ServiceWorkerRegistrationObjectInfo,
                     content::ServiceWorkerVersionAttributes)

// Sent when any kind of registration error occurs during a
// RegisterServiceWorker handler above.
IPC_MESSAGE_CONTROL4(ServiceWorkerMsg_ServiceWorkerRegistrationError,
                     int /* thread_id */,
                     int /* request_id */,
                     blink::WebServiceWorkerError::ErrorType /* code */,
                     base::string16 /* message */)

// Sent when any kind of update error occurs during a
// UpdateServiceWorker handler above.
IPC_MESSAGE_CONTROL4(ServiceWorkerMsg_ServiceWorkerUpdateError,
                     int /* thread_id */,
                     int /* request_id */,
                     blink::WebServiceWorkerError::ErrorType /* code */,
                     base::string16 /* message */)

// Sent when any kind of registration error occurs during a
// UnregisterServiceWorker handler above.
IPC_MESSAGE_CONTROL4(ServiceWorkerMsg_ServiceWorkerUnregistrationError,
                     int /* thread_id */,
                     int /* request_id */,
                     blink::WebServiceWorkerError::ErrorType /* code */,
                     base::string16 /* message */)

// Sent when any kind of registration error occurs during a
// GetRegistration handler above.
IPC_MESSAGE_CONTROL4(ServiceWorkerMsg_ServiceWorkerGetRegistrationError,
                     int /* thread_id */,
                     int /* request_id */,
                     blink::WebServiceWorkerError::ErrorType /* code */,
                     base::string16 /* message */)

// Sent when any kind of registration error occurs during a
// GetRegistrations handler above.
IPC_MESSAGE_CONTROL4(ServiceWorkerMsg_ServiceWorkerGetRegistrationsError,
                     int /* thread_id */,
                     int /* request_id */,
                     blink::WebServiceWorkerError::ErrorType /* code */,
                     base::string16 /* message */)

// Informs the child process that the ServiceWorker's state has changed.
IPC_MESSAGE_CONTROL3(ServiceWorkerMsg_ServiceWorkerStateChanged,
                     int /* thread_id */,
                     int /* handle_id */,
                     blink::WebServiceWorkerState)

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
// provider. |used_features| is the set of features that the worker has used.
// The values must be from blink::UseCounter::Feature enum.
IPC_MESSAGE_CONTROL5(ServiceWorkerMsg_SetControllerServiceWorker,
                     int /* thread_id */,
                     int /* provider_id */,
                     content::ServiceWorkerObjectInfo,
                     bool /* should_notify_controllerchange */,
                     std::set<uint32_t> /* used_features */)

IPC_MESSAGE_CONTROL2(ServiceWorkerMsg_DidEnableNavigationPreload,
                     int /* thread_id */,
                     int /* request_id */)
IPC_MESSAGE_CONTROL4(ServiceWorkerMsg_EnableNavigationPreloadError,
                     int /* thread_id */,
                     int /* request_id */,
                     blink::WebServiceWorkerError::ErrorType /* code */,
                     std::string /* message */)
IPC_MESSAGE_CONTROL3(ServiceWorkerMsg_DidGetNavigationPreloadState,
                     int /* thread_id */,
                     int /* request_id */,
                     content::NavigationPreloadState /* state */)
IPC_MESSAGE_CONTROL4(ServiceWorkerMsg_GetNavigationPreloadStateError,
                     int /* thread_id */,
                     int /* request_id */,
                     blink::WebServiceWorkerError::ErrorType /* code */,
                     std::string /* message */)
IPC_MESSAGE_CONTROL2(ServiceWorkerMsg_DidSetNavigationPreloadHeader,
                     int /* thread_id */,
                     int /* request_id */)
IPC_MESSAGE_CONTROL4(ServiceWorkerMsg_SetNavigationPreloadHeaderError,
                     int /* thread_id */,
                     int /* request_id */,
                     blink::WebServiceWorkerError::ErrorType /* code */,
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
                     blink::WebServiceWorkerError::ErrorType /* code */,
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
