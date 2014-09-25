// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Message definition file, included multiple times, hence no include guard.

#include <string>
#include <vector>

#include "base/strings/string16.h"
#include "content/common/service_worker/service_worker_status_code.h"
#include "content/common/service_worker/service_worker_types.h"
#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_param_traits.h"
#include "third_party/WebKit/public/platform/WebServiceWorkerCacheError.h"
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

IPC_ENUM_TRAITS_MAX_VALUE(blink::WebServiceWorkerState,
                          blink::WebServiceWorkerStateLast)

IPC_STRUCT_TRAITS_BEGIN(content::ServiceWorkerFetchRequest)
  IPC_STRUCT_TRAITS_MEMBER(url)
  IPC_STRUCT_TRAITS_MEMBER(method)
  IPC_STRUCT_TRAITS_MEMBER(headers)
  IPC_STRUCT_TRAITS_MEMBER(blob_uuid)
  IPC_STRUCT_TRAITS_MEMBER(blob_size)
  IPC_STRUCT_TRAITS_MEMBER(referrer)
  IPC_STRUCT_TRAITS_MEMBER(is_reload)
IPC_STRUCT_TRAITS_END()

IPC_ENUM_TRAITS_MAX_VALUE(content::ServiceWorkerFetchEventResult,
                          content::SERVICE_WORKER_FETCH_EVENT_LAST)

IPC_STRUCT_TRAITS_BEGIN(content::ServiceWorkerResponse)
  IPC_STRUCT_TRAITS_MEMBER(url)
  IPC_STRUCT_TRAITS_MEMBER(status_code)
  IPC_STRUCT_TRAITS_MEMBER(status_text)
  IPC_STRUCT_TRAITS_MEMBER(headers)
  IPC_STRUCT_TRAITS_MEMBER(blob_uuid)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(content::ServiceWorkerCacheQueryParams)
  IPC_STRUCT_TRAITS_MEMBER(ignore_search)
  IPC_STRUCT_TRAITS_MEMBER(ignore_method)
  IPC_STRUCT_TRAITS_MEMBER(ignore_vary)
  IPC_STRUCT_TRAITS_MEMBER(prefix_match)
IPC_STRUCT_TRAITS_END()

IPC_ENUM_TRAITS_MAX_VALUE(content::ServiceWorkerCacheOperationType,
                          content::SERVICE_WORKER_CACHE_OPERATION_TYPE_LAST)

IPC_STRUCT_TRAITS_BEGIN(content::ServiceWorkerBatchOperation)
  IPC_STRUCT_TRAITS_MEMBER(operation_type)
  IPC_STRUCT_TRAITS_MEMBER(request)
  IPC_STRUCT_TRAITS_MEMBER(response)
  IPC_STRUCT_TRAITS_MEMBER(match_params)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(content::ServiceWorkerObjectInfo)
  IPC_STRUCT_TRAITS_MEMBER(handle_id)
  IPC_STRUCT_TRAITS_MEMBER(scope)
  IPC_STRUCT_TRAITS_MEMBER(url)
  IPC_STRUCT_TRAITS_MEMBER(state)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(content::ServiceWorkerRegistrationObjectInfo)
  IPC_STRUCT_TRAITS_MEMBER(handle_id)
  IPC_STRUCT_TRAITS_MEMBER(scope)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(content::ServiceWorkerVersionAttributes)
  IPC_STRUCT_TRAITS_MEMBER(installing)
  IPC_STRUCT_TRAITS_MEMBER(waiting)
  IPC_STRUCT_TRAITS_MEMBER(active)
IPC_STRUCT_TRAITS_END()

IPC_ENUM_TRAITS_MAX_VALUE(
    blink::WebServiceWorkerCacheError,
    blink::WebServiceWorkerCacheErrorLast)

//---------------------------------------------------------------------------
// Messages sent from the child process to the browser.

IPC_MESSAGE_CONTROL5(ServiceWorkerHostMsg_RegisterServiceWorker,
                     int /* thread_id */,
                     int /* request_id */,
                     int /* provider_id */,
                     GURL /* scope */,
                     GURL /* script_url */)

IPC_MESSAGE_CONTROL4(ServiceWorkerHostMsg_UnregisterServiceWorker,
                     int /* thread_id */,
                     int /* request_id */,
                     int /* provider_id */,
                     GURL /* scope (url pattern) */)

IPC_MESSAGE_CONTROL4(ServiceWorkerHostMsg_GetRegistration,
                     int /* thread_id */,
                     int /* request_id */,
                     int /* provider_id */,
                     GURL /* document_url */)

// Sends a 'message' event to a service worker (renderer->browser).
IPC_MESSAGE_CONTROL3(ServiceWorkerHostMsg_PostMessageToWorker,
                     int /* handle_id */,
                     base::string16 /* message */,
                     std::vector<int> /* sent_message_port_ids */)

// Informs the browser of a new ServiceWorkerProvider in the child process,
// |provider_id| is unique within its child process.
IPC_MESSAGE_CONTROL1(ServiceWorkerHostMsg_ProviderCreated,
                     int /* provider_id */)

// Informs the browser of a ServiceWorkerProvider being destroyed.
IPC_MESSAGE_CONTROL1(ServiceWorkerHostMsg_ProviderDestroyed,
                     int /* provider_id */)

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

// Informs the browser that |provider_id| is associated
// with a service worker script running context and
// |version_id| identifies which ServiceWorkerVersion.
IPC_MESSAGE_CONTROL2(ServiceWorkerHostMsg_SetVersionId,
                     int /* provider_id */,
                     int64 /* version_id */)

// Informs the browser that event handling has finished.
// Routed to the target ServiceWorkerVersion.
IPC_MESSAGE_ROUTED2(ServiceWorkerHostMsg_InstallEventFinished,
                    int /* request_id */,
                    blink::WebServiceWorkerEventResult)
IPC_MESSAGE_ROUTED2(ServiceWorkerHostMsg_ActivateEventFinished,
                    int /* request_id */,
                    blink::WebServiceWorkerEventResult)
IPC_MESSAGE_ROUTED3(ServiceWorkerHostMsg_FetchEventFinished,
                    int /* request_id */,
                    content::ServiceWorkerFetchEventResult,
                    content::ServiceWorkerResponse)
IPC_MESSAGE_ROUTED1(ServiceWorkerHostMsg_SyncEventFinished,
                    int /* request_id */)
IPC_MESSAGE_ROUTED1(ServiceWorkerHostMsg_PushEventFinished,
                    int /* request_id */)

// Asks the browser to retrieve documents controlled by the sender
// ServiceWorker.
IPC_MESSAGE_ROUTED1(ServiceWorkerHostMsg_GetClientDocuments,
                    int /* request_id */)

// Sends a 'message' event to a client document (renderer->browser).
IPC_MESSAGE_ROUTED3(ServiceWorkerHostMsg_PostMessageToDocument,
                    int /* client_id */,
                    base::string16 /* message */,
                    std::vector<int> /* sent_message_port_ids */)

// CacheStorage operations in the browser.
IPC_MESSAGE_ROUTED2(ServiceWorkerHostMsg_CacheStorageGet,
                    int /* request_id */,
                    base::string16 /* fetch_store_name */)

IPC_MESSAGE_ROUTED2(ServiceWorkerHostMsg_CacheStorageHas,
                    int /* request_id */,
                    base::string16 /* fetch_store_name */)

IPC_MESSAGE_ROUTED2(ServiceWorkerHostMsg_CacheStorageCreate,
                    int /* request_id */,
                    base::string16 /* fetch_store_name */)

IPC_MESSAGE_ROUTED2(ServiceWorkerHostMsg_CacheStorageDelete,
                    int /* request_id */,
                    base::string16 /* fetch_store_name */)

IPC_MESSAGE_ROUTED1(ServiceWorkerHostMsg_CacheStorageKeys,
                    int /* request_id */)

// Cache operations in the browser.
IPC_MESSAGE_ROUTED4(ServiceWorkerHostMsg_CacheMatch,
                    int /* request_id */,
                    int /* cache_id */,
                    content::ServiceWorkerFetchRequest,
                    content::ServiceWorkerCacheQueryParams)

IPC_MESSAGE_ROUTED4(ServiceWorkerHostMsg_CacheMatchAll,
                    int /* request_id */,
                    int /* cache_id */,
                    content::ServiceWorkerFetchRequest,
                    content::ServiceWorkerCacheQueryParams)

IPC_MESSAGE_ROUTED4(ServiceWorkerHostMsg_CacheKeys,
                    int /* request_id */,
                    int /* cache_id */,
                    content::ServiceWorkerFetchRequest,
                    content::ServiceWorkerCacheQueryParams);

IPC_MESSAGE_ROUTED3(ServiceWorkerHostMsg_CacheBatch,
                    int /* request_id */,
                    int /* cache_id */,
                    std::vector<content::ServiceWorkerBatchOperation>);

IPC_MESSAGE_ROUTED1(ServiceWorkerHostMsg_CacheClosed,
                    int /* cache_id */);

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

// Sent when any kind of registration error occurs during a
// RegisterServiceWorker handler above.
IPC_MESSAGE_CONTROL4(ServiceWorkerMsg_ServiceWorkerRegistrationError,
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

// Informs the child process that the ServiceWorker's state has changed.
IPC_MESSAGE_CONTROL3(ServiceWorkerMsg_ServiceWorkerStateChanged,
                     int /* thread_id */,
                     int /* handle_id */,
                     blink::WebServiceWorkerState)

// Tells the child process to set service workers for the given provider.
IPC_MESSAGE_CONTROL5(ServiceWorkerMsg_SetVersionAttributes,
                     int /* thread_id */,
                     int /* provider_id */,
                     int /* registration_handle_id */,
                     int /* changed_mask */,
                     content::ServiceWorkerVersionAttributes)

// Informs the child process that new ServiceWorker enters the installation
// phase.
IPC_MESSAGE_CONTROL2(ServiceWorkerMsg_UpdateFound,
                     int /* thread_id */,
                     content::ServiceWorkerRegistrationObjectInfo)

// Tells the child process to set the controller ServiceWorker for the given
// provider.
IPC_MESSAGE_CONTROL3(ServiceWorkerMsg_SetControllerServiceWorker,
                     int /* thread_id */,
                     int /* provider_id */,
                     content::ServiceWorkerObjectInfo)

// Sends a 'message' event to a client document (browser->renderer).
IPC_MESSAGE_CONTROL5(ServiceWorkerMsg_MessageToDocument,
                     int /* thread_id */,
                     int /* provider_id */,
                     base::string16 /* message */,
                     std::vector<int> /* sent_message_port_ids */,
                     std::vector<int> /* new_routing_ids */)

// Sent via EmbeddedWorker to dispatch events.
IPC_MESSAGE_CONTROL2(ServiceWorkerMsg_InstallEvent,
                     int /* request_id */,
                     int /* active_version_id */)
IPC_MESSAGE_CONTROL1(ServiceWorkerMsg_ActivateEvent,
                     int /* request_id */)
IPC_MESSAGE_CONTROL2(ServiceWorkerMsg_FetchEvent,
                     int /* request_id */,
                     content::ServiceWorkerFetchRequest)
IPC_MESSAGE_CONTROL1(ServiceWorkerMsg_SyncEvent,
                     int /* request_id */)
IPC_MESSAGE_CONTROL2(ServiceWorkerMsg_PushEvent,
                     int /* request_id */,
                     std::string /* data */)
IPC_MESSAGE_CONTROL3(ServiceWorkerMsg_MessageToWorker,
                     base::string16 /* message */,
                     std::vector<int> /* sent_message_port_ids */,
                     std::vector<int> /* new_routing_ids */)

// Sent via EmbeddedWorker as a response of GetClientDocuments.
IPC_MESSAGE_CONTROL2(ServiceWorkerMsg_DidGetClientDocuments,
                     int /* request_id */,
                     std::vector<int> /* client_ids */)

// Sent via EmbeddedWorker at successful completion of CacheStorage operations.
IPC_MESSAGE_CONTROL2(ServiceWorkerMsg_CacheStorageGetSuccess,
                     int /* request_id */,
                     int /* fetch_store_id */)
IPC_MESSAGE_CONTROL1(ServiceWorkerMsg_CacheStorageHasSuccess,
                     int /* request_id */)
IPC_MESSAGE_CONTROL2(ServiceWorkerMsg_CacheStorageCreateSuccess,
                     int /* request_id */,
                     int /* fetch_store_id */)
IPC_MESSAGE_CONTROL1(ServiceWorkerMsg_CacheStorageDeleteSuccess,
                     int /* request_id */)
IPC_MESSAGE_CONTROL2(ServiceWorkerMsg_CacheStorageKeysSuccess,
                     int /* request_id */,
                     std::vector<base::string16> /* keys */)

// Sent via EmbeddedWorker at erroneous completion of CacheStorage operations.
IPC_MESSAGE_CONTROL2(ServiceWorkerMsg_CacheStorageGetError,
                     int /* request_id */,
                     blink::WebServiceWorkerCacheError /* reason */)
IPC_MESSAGE_CONTROL2(ServiceWorkerMsg_CacheStorageHasError,
                     int /* request_id */,
                     blink::WebServiceWorkerCacheError /* reason */)
IPC_MESSAGE_CONTROL2(ServiceWorkerMsg_CacheStorageCreateError,
                     int /* request_id */,
                     blink::WebServiceWorkerCacheError /* reason */)
IPC_MESSAGE_CONTROL2(ServiceWorkerMsg_CacheStorageDeleteError,
                     int /* request_id */,
                     blink::WebServiceWorkerCacheError /* reason */)
IPC_MESSAGE_CONTROL2(ServiceWorkerMsg_CacheStorageKeysError,
                     int /* request_id */,
                     blink::WebServiceWorkerCacheError /* reason */)

// Sent via EmbeddedWorker at successful completion of Cache operations.
IPC_MESSAGE_CONTROL2(ServiceWorkerMsg_CacheMatchSuccess,
                     int /* request_id */,
                     content::ServiceWorkerResponse)
IPC_MESSAGE_CONTROL2(ServiceWorkerMsg_CacheMatchAllSuccess,
                     int /* request_id */,
                     std::vector<content::ServiceWorkerResponse>)
IPC_MESSAGE_CONTROL2(ServiceWorkerMsg_CacheKeysSuccess,
                     int /* request_id */,
                     std::vector<content::ServiceWorkerFetchRequest>)
IPC_MESSAGE_CONTROL2(ServiceWorkerMsg_CacheBatchSuccess,
                     int /* request_id */,
                     std::vector<content::ServiceWorkerResponse>)

// Sent via EmbeddedWorker at erroneous completion of CacheStorage operations.
IPC_MESSAGE_CONTROL2(ServiceWorkerMsg_CacheMatchError,
                     int /* request_id */,
                     blink::WebServiceWorkerCacheError)
IPC_MESSAGE_CONTROL2(ServiceWorkerMsg_CacheMatchAllError,
                     int /* request_id */,
                     blink::WebServiceWorkerCacheError)
IPC_MESSAGE_CONTROL2(ServiceWorkerMsg_CacheKeysError,
                     int /* request_id */,
                     blink::WebServiceWorkerCacheError)
IPC_MESSAGE_CONTROL2(ServiceWorkerMsg_CacheBatchError,
                     int /* request_id */,
                     blink::WebServiceWorkerCacheError)
