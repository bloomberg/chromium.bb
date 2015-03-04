// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/service_worker/service_worker_script_context.h"

#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/trace_event/trace_event.h"
#include "content/child/notifications/notification_data_conversions.h"
#include "content/child/service_worker/web_service_worker_registration_impl.h"
#include "content/child/thread_safe_sender.h"
#include "content/child/webmessageportchannel_impl.h"
#include "content/common/message_port_messages.h"
#include "content/common/service_worker/service_worker_messages.h"
#include "content/public/common/referrer.h"
#include "content/renderer/service_worker/embedded_worker_context_client.h"
#include "ipc/ipc_message.h"
#include "third_party/WebKit/public/platform/WebCrossOriginServiceWorkerClient.h"
#include "third_party/WebKit/public/platform/WebReferrerPolicy.h"
#include "third_party/WebKit/public/platform/WebServiceWorkerRequest.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/WebURL.h"
#include "third_party/WebKit/public/platform/modules/notifications/WebNotificationData.h"
#include "third_party/WebKit/public/web/WebServiceWorkerContextClient.h"
#include "third_party/WebKit/public/web/WebServiceWorkerContextProxy.h"

namespace content {

namespace {

void SendPostMessageToDocumentOnMainThread(
    ThreadSafeSender* sender,
    int routing_id,
    int client_id,
    const base::string16& message,
    scoped_ptr<blink::WebMessagePortChannelArray> channels) {
  sender->Send(new ServiceWorkerHostMsg_PostMessageToDocument(
      routing_id, client_id, message,
      WebMessagePortChannelImpl::ExtractMessagePortIDs(channels.release())));
}

void SendCrossOriginMessageToClientOnMainThread(
    ThreadSafeSender* sender,
    int message_port_id,
    const base::string16& message,
    scoped_ptr<blink::WebMessagePortChannelArray> channels) {
  sender->Send(new MessagePortHostMsg_PostMessage(
      message_port_id,
      MessagePortMessage(message),
                         WebMessagePortChannelImpl::ExtractMessagePortIDs(
                             channels.release())));
}

blink::WebURLRequest::FetchRequestMode GetBlinkFetchRequestMode(
    FetchRequestMode mode) {
  return static_cast<blink::WebURLRequest::FetchRequestMode>(mode);
}

blink::WebURLRequest::FetchCredentialsMode GetBlinkFetchCredentialsMode(
    FetchCredentialsMode credentials_mode) {
  return static_cast<blink::WebURLRequest::FetchCredentialsMode>(
      credentials_mode);
}

blink::WebURLRequest::RequestContext GetBlinkRequestContext(
    RequestContextType request_context_type) {
  return static_cast<blink::WebURLRequest::RequestContext>(
      request_context_type);
}

blink::WebURLRequest::FrameType GetBlinkFrameType(
    RequestContextFrameType frame_type) {
  return static_cast<blink::WebURLRequest::FrameType>(frame_type);
}

blink::WebServiceWorkerClientInfo
ToWebServiceWorkerClientInfo(const ServiceWorkerClientInfo& client_info) {
  DCHECK(client_info.IsValid());

  blink::WebServiceWorkerClientInfo web_client_info;

  web_client_info.clientID = client_info.client_id;
  web_client_info.pageVisibilityState = client_info.page_visibility_state;
  web_client_info.isFocused = client_info.is_focused;
  web_client_info.url = client_info.url;
  web_client_info.frameType = GetBlinkFrameType(client_info.frame_type);

  return web_client_info;
}

}  // namespace

ServiceWorkerScriptContext::ServiceWorkerScriptContext(
    EmbeddedWorkerContextClient* embedded_context,
    blink::WebServiceWorkerContextProxy* proxy)
    : cache_storage_dispatcher_(new ServiceWorkerCacheStorageDispatcher(this)),
      embedded_context_(embedded_context),
      proxy_(proxy) {
}

ServiceWorkerScriptContext::~ServiceWorkerScriptContext() {}

void ServiceWorkerScriptContext::OnMessageReceived(
    const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(ServiceWorkerScriptContext, message)
    IPC_MESSAGE_HANDLER(ServiceWorkerMsg_ActivateEvent, OnActivateEvent)
    IPC_MESSAGE_HANDLER(ServiceWorkerMsg_FetchEvent, OnFetchEvent)
    IPC_MESSAGE_HANDLER(ServiceWorkerMsg_InstallEvent, OnInstallEvent)
    IPC_MESSAGE_HANDLER(ServiceWorkerMsg_SyncEvent, OnSyncEvent)
    IPC_MESSAGE_HANDLER(ServiceWorkerMsg_NotificationClickEvent,
                        OnNotificationClickEvent)
    IPC_MESSAGE_HANDLER(ServiceWorkerMsg_PushEvent, OnPushEvent)
    IPC_MESSAGE_HANDLER(ServiceWorkerMsg_GeofencingEvent, OnGeofencingEvent)
    IPC_MESSAGE_HANDLER(ServiceWorkerMsg_CrossOriginConnectEvent,
                        OnCrossOriginConnectEvent)
    IPC_MESSAGE_HANDLER(ServiceWorkerMsg_MessageToWorker, OnPostMessage)
    IPC_MESSAGE_HANDLER(ServiceWorkerMsg_CrossOriginMessageToWorker,
                        OnCrossOriginMessageToWorker)
    IPC_MESSAGE_HANDLER(ServiceWorkerMsg_DidGetClientDocuments,
                        OnDidGetClientDocuments)
    IPC_MESSAGE_HANDLER(ServiceWorkerMsg_OpenWindowResponse,
                        OnOpenWindowResponse)
    IPC_MESSAGE_HANDLER(ServiceWorkerMsg_OpenWindowError,
                        OnOpenWindowError)
    IPC_MESSAGE_HANDLER(ServiceWorkerMsg_FocusClientResponse,
                        OnFocusClientResponse)
    IPC_MESSAGE_HANDLER(ServiceWorkerMsg_DidSkipWaiting, OnDidSkipWaiting)
    IPC_MESSAGE_HANDLER(ServiceWorkerMsg_DidClaimClients, OnDidClaimClients)
    IPC_MESSAGE_HANDLER(ServiceWorkerMsg_ClaimClientsError, OnClaimClientsError)
    IPC_MESSAGE_HANDLER(ServiceWorkerMsg_Ping, OnPing);
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  // TODO(gavinp): Would it be preferable to put an AddListener() method to
  // EmbeddedWorkerContextClient?
  if (!handled)
    handled = cache_storage_dispatcher_->OnMessageReceived(message);

  DCHECK(handled);
}

void ServiceWorkerScriptContext::SetRegistrationInServiceWorkerGlobalScope(
    scoped_ptr<WebServiceWorkerRegistrationImpl> registration) {
  proxy_->setRegistration(registration.release());
}

void ServiceWorkerScriptContext::DidHandleActivateEvent(
    int request_id,
    blink::WebServiceWorkerEventResult result) {
  UMA_HISTOGRAM_TIMES(
      "ServiceWorker.ActivateEventExecutionTime",
      base::TimeTicks::Now() - activate_start_timings_[request_id]);
  activate_start_timings_.erase(request_id);

  Send(new ServiceWorkerHostMsg_ActivateEventFinished(
      GetRoutingID(), request_id, result));
}

void ServiceWorkerScriptContext::DidHandleInstallEvent(
    int request_id,
    blink::WebServiceWorkerEventResult result) {
  UMA_HISTOGRAM_TIMES(
      "ServiceWorker.InstallEventExecutionTime",
      base::TimeTicks::Now() - install_start_timings_[request_id]);
  install_start_timings_.erase(request_id);

  Send(new ServiceWorkerHostMsg_InstallEventFinished(
      GetRoutingID(), request_id, result));
}

void ServiceWorkerScriptContext::DidHandleFetchEvent(
    int request_id,
    ServiceWorkerFetchEventResult result,
    const ServiceWorkerResponse& response) {
  UMA_HISTOGRAM_TIMES(
      "ServiceWorker.FetchEventExecutionTime",
      base::TimeTicks::Now() - fetch_start_timings_[request_id]);
  fetch_start_timings_.erase(request_id);

  Send(new ServiceWorkerHostMsg_FetchEventFinished(
      GetRoutingID(), request_id, result, response));
}

void ServiceWorkerScriptContext::DidHandleNotificationClickEvent(
    int request_id,
    blink::WebServiceWorkerEventResult result) {
  UMA_HISTOGRAM_TIMES(
      "ServiceWorker.NotificationClickEventExecutionTime",
      base::TimeTicks::Now() - notification_click_start_timings_[request_id]);
  notification_click_start_timings_.erase(request_id);

  Send(new ServiceWorkerHostMsg_NotificationClickEventFinished(
      GetRoutingID(), request_id));
}

void ServiceWorkerScriptContext::DidHandlePushEvent(
    int request_id,
    blink::WebServiceWorkerEventResult result) {
  if (result == blink::WebServiceWorkerEventResultCompleted) {
    UMA_HISTOGRAM_TIMES(
        "ServiceWorker.PushEventExecutionTime",
        base::TimeTicks::Now() - push_start_timings_[request_id]);
  }
  push_start_timings_.erase(request_id);

  Send(new ServiceWorkerHostMsg_PushEventFinished(
      GetRoutingID(), request_id, result));
}

void ServiceWorkerScriptContext::DidHandleSyncEvent(int request_id) {
  Send(new ServiceWorkerHostMsg_SyncEventFinished(
      GetRoutingID(), request_id));
}

void ServiceWorkerScriptContext::DidHandleCrossOriginConnectEvent(
    int request_id,
    bool accept_connection) {
  Send(new ServiceWorkerHostMsg_CrossOriginConnectEventFinished(
      GetRoutingID(), request_id, accept_connection));
}

void ServiceWorkerScriptContext::GetClientDocuments(
    blink::WebServiceWorkerClientsCallbacks* callbacks) {
  DCHECK(callbacks);
  int request_id = pending_clients_callbacks_.Add(callbacks);
  Send(new ServiceWorkerHostMsg_GetClientDocuments(
      GetRoutingID(), request_id));
}

void ServiceWorkerScriptContext::OpenWindow(
    const GURL& url, blink::WebServiceWorkerClientCallbacks* callbacks) {
  DCHECK(callbacks);
  int request_id = pending_client_callbacks_.Add(callbacks);
  Send(new ServiceWorkerHostMsg_OpenWindow(GetRoutingID(), request_id, url));
}

void ServiceWorkerScriptContext::SetCachedMetadata(const GURL& url,
                                                   const char* data,
                                                   size_t size) {
  std::vector<char> copy(data, data + size);
  Send(new ServiceWorkerHostMsg_SetCachedMetadata(GetRoutingID(), url, copy));
}

void ServiceWorkerScriptContext::ClearCachedMetadata(const GURL& url) {
  Send(new ServiceWorkerHostMsg_ClearCachedMetadata(GetRoutingID(), url));
}

void ServiceWorkerScriptContext::PostMessageToDocument(
    int client_id,
    const base::string16& message,
    scoped_ptr<blink::WebMessagePortChannelArray> channels) {
  // This may send channels for MessagePorts, and all internal book-keeping
  // messages for MessagePort (e.g. QueueMessages) are sent from main thread
  // (with thread hopping), so we need to do the same thread hopping here not
  // to overtake those messages.
  embedded_context_->main_thread_task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&SendPostMessageToDocumentOnMainThread,
                 make_scoped_refptr(embedded_context_->thread_safe_sender()),
                 GetRoutingID(), client_id, message, base::Passed(&channels)));
}

void ServiceWorkerScriptContext::PostCrossOriginMessageToClient(
    const blink::WebCrossOriginServiceWorkerClient& client,
    const base::string16& message,
    scoped_ptr<blink::WebMessagePortChannelArray> channels) {
  // This may send channels for MessagePorts, and all internal book-keeping
  // messages for MessagePort (e.g. QueueMessages) are sent from main thread
  // (with thread hopping), so we need to do the same thread hopping here not
  // to overtake those messages.
  embedded_context_->main_thread_task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&SendCrossOriginMessageToClientOnMainThread,
                 make_scoped_refptr(embedded_context_->thread_safe_sender()),
                 client.clientID, message, base::Passed(&channels)));
}

void ServiceWorkerScriptContext::FocusClient(
    int client_id, blink::WebServiceWorkerClientCallbacks* callback) {
  DCHECK(callback);
  int request_id = pending_client_callbacks_.Add(callback);
  Send(new ServiceWorkerHostMsg_FocusClient(
      GetRoutingID(), request_id, client_id));
}

void ServiceWorkerScriptContext::ClaimClients(
    blink::WebServiceWorkerClientsClaimCallbacks* callbacks) {
  DCHECK(callbacks);
  int request_id = pending_claim_clients_callbacks_.Add(callbacks);
  Send(new ServiceWorkerHostMsg_ClaimClients(GetRoutingID(), request_id));
}

void ServiceWorkerScriptContext::SkipWaiting(
    blink::WebServiceWorkerSkipWaitingCallbacks* callbacks) {
  DCHECK(callbacks);
  int request_id = pending_skip_waiting_callbacks_.Add(callbacks);
  Send(new ServiceWorkerHostMsg_SkipWaiting(GetRoutingID(), request_id));
}

void ServiceWorkerScriptContext::Send(IPC::Message* message) {
  embedded_context_->Send(message);
}

int ServiceWorkerScriptContext::GetRoutingID() const {
  return embedded_context_->embedded_worker_id();
}

void ServiceWorkerScriptContext::OnActivateEvent(int request_id) {
  TRACE_EVENT0("ServiceWorker",
               "ServiceWorkerScriptContext::OnActivateEvent");
  activate_start_timings_[request_id] = base::TimeTicks::Now();
  proxy_->dispatchActivateEvent(request_id);
}

void ServiceWorkerScriptContext::OnInstallEvent(int request_id) {
  TRACE_EVENT0("ServiceWorker",
               "ServiceWorkerScriptContext::OnInstallEvent");
  install_start_timings_[request_id] = base::TimeTicks::Now();
  proxy_->dispatchInstallEvent(request_id);
}

void ServiceWorkerScriptContext::OnFetchEvent(
    int request_id,
    const ServiceWorkerFetchRequest& request) {
  blink::WebServiceWorkerRequest webRequest;
  TRACE_EVENT0("ServiceWorker",
               "ServiceWorkerScriptContext::OnFetchEvent");
  webRequest.setURL(blink::WebURL(request.url));
  webRequest.setMethod(blink::WebString::fromUTF8(request.method));
  for (ServiceWorkerHeaderMap::const_iterator it = request.headers.begin();
       it != request.headers.end();
       ++it) {
    webRequest.setHeader(blink::WebString::fromUTF8(it->first),
                         blink::WebString::fromUTF8(it->second));
  }
  if (!request.blob_uuid.empty()) {
    webRequest.setBlob(blink::WebString::fromUTF8(request.blob_uuid),
                       request.blob_size);
  }
  webRequest.setReferrer(
      blink::WebString::fromUTF8(request.referrer.url.spec()),
      request.referrer.policy);
  webRequest.setMode(GetBlinkFetchRequestMode(request.mode));
  webRequest.setCredentialsMode(
      GetBlinkFetchCredentialsMode(request.credentials_mode));
  webRequest.setRequestContext(
      GetBlinkRequestContext(request.request_context_type));
  webRequest.setFrameType(GetBlinkFrameType(request.frame_type));
  webRequest.setIsReload(request.is_reload);
  fetch_start_timings_[request_id] = base::TimeTicks::Now();
  proxy_->dispatchFetchEvent(request_id, webRequest);
}

void ServiceWorkerScriptContext::OnSyncEvent(int request_id) {
  TRACE_EVENT0("ServiceWorker",
               "ServiceWorkerScriptContext::OnSyncEvent");
  proxy_->dispatchSyncEvent(request_id);
}

void ServiceWorkerScriptContext::OnNotificationClickEvent(
    int request_id,
    const std::string& notification_id,
    const PlatformNotificationData& notification_data) {
  TRACE_EVENT0("ServiceWorker",
               "ServiceWorkerScriptContext::OnNotificationClickEvent");
  notification_click_start_timings_[request_id] = base::TimeTicks::Now();
  proxy_->dispatchNotificationClickEvent(
      request_id,
      blink::WebString::fromUTF8(notification_id),
      ToWebNotificationData(notification_data));
}

void ServiceWorkerScriptContext::OnPushEvent(int request_id,
                                             const std::string& data) {
  TRACE_EVENT0("ServiceWorker",
               "ServiceWorkerScriptContext::OnPushEvent");
  push_start_timings_[request_id] = base::TimeTicks::Now();
  proxy_->dispatchPushEvent(request_id, blink::WebString::fromUTF8(data));
}

void ServiceWorkerScriptContext::OnGeofencingEvent(
    int request_id,
    blink::WebGeofencingEventType event_type,
    const std::string& region_id,
    const blink::WebCircularGeofencingRegion& region) {
  TRACE_EVENT0("ServiceWorker",
               "ServiceWorkerScriptContext::OnGeofencingEvent");
  proxy_->dispatchGeofencingEvent(
      request_id, event_type, blink::WebString::fromUTF8(region_id), region);
  Send(new ServiceWorkerHostMsg_GeofencingEventFinished(GetRoutingID(),
                                                        request_id));
}

void ServiceWorkerScriptContext::OnCrossOriginConnectEvent(
    int request_id,
    const NavigatorConnectClient& client) {
  TRACE_EVENT0("ServiceWorker",
               "ServiceWorkerScriptContext::OnCrossOriginConnectEvent");
  blink::WebCrossOriginServiceWorkerClient web_client;
  web_client.origin = client.origin;
  web_client.targetURL = client.target_url;
  web_client.clientID = client.message_port_id;
  proxy_->dispatchCrossOriginConnectEvent(request_id, web_client);
}

void ServiceWorkerScriptContext::OnPostMessage(
    const base::string16& message,
    const std::vector<TransferredMessagePort>& sent_message_ports,
    const std::vector<int>& new_routing_ids) {
  TRACE_EVENT0("ServiceWorker",
               "ServiceWorkerScriptContext::OnPostEvent");
  blink::WebMessagePortChannelArray ports =
      WebMessagePortChannelImpl::CreatePorts(
          sent_message_ports, new_routing_ids,
          embedded_context_->main_thread_task_runner());

  // dispatchMessageEvent is expected to execute onmessage function
  // synchronously.
  base::TimeTicks before = base::TimeTicks::Now();
  proxy_->dispatchMessageEvent(message, ports);
  UMA_HISTOGRAM_TIMES(
      "ServiceWorker.MessageEventExecutionTime",
      base::TimeTicks::Now() - before);
}

void ServiceWorkerScriptContext::OnCrossOriginMessageToWorker(
    const NavigatorConnectClient& client,
    const base::string16& message,
    const std::vector<TransferredMessagePort>& sent_message_ports,
    const std::vector<int>& new_routing_ids) {
  TRACE_EVENT0("ServiceWorker",
               "ServiceWorkerScriptContext::OnCrossOriginMessageToWorker");
  blink::WebMessagePortChannelArray ports =
      WebMessagePortChannelImpl::CreatePorts(
          sent_message_ports, new_routing_ids,
          embedded_context_->main_thread_task_runner());

  blink::WebCrossOriginServiceWorkerClient web_client;
  web_client.origin = client.origin;
  web_client.targetURL = client.target_url;
  web_client.clientID = client.message_port_id;
  proxy_->dispatchCrossOriginMessageEvent(web_client, message, ports);
}

void ServiceWorkerScriptContext::OnDidGetClientDocuments(
    int request_id, const std::vector<ServiceWorkerClientInfo>& clients) {
  TRACE_EVENT0("ServiceWorker",
               "ServiceWorkerScriptContext::OnDidGetClientDocuments");
  blink::WebServiceWorkerClientsCallbacks* callbacks =
      pending_clients_callbacks_.Lookup(request_id);
  if (!callbacks) {
    NOTREACHED() << "Got stray response: " << request_id;
    return;
  }
  scoped_ptr<blink::WebServiceWorkerClientsInfo> info(
      new blink::WebServiceWorkerClientsInfo);
  blink::WebVector<blink::WebServiceWorkerClientInfo> convertedClients(
      clients.size());
  for (size_t i = 0; i < clients.size(); ++i)
    convertedClients[i] = ToWebServiceWorkerClientInfo(clients[i]);
  info->clients.swap(convertedClients);
  callbacks->onSuccess(info.release());
  pending_clients_callbacks_.Remove(request_id);
}

void ServiceWorkerScriptContext::OnOpenWindowResponse(
    int request_id,
    const ServiceWorkerClientInfo& client) {
  TRACE_EVENT0("ServiceWorker",
               "ServiceWorkerScriptContext::OnOpenWindowResponse");
  blink::WebServiceWorkerClientCallbacks* callbacks =
      pending_client_callbacks_.Lookup(request_id);
  if (!callbacks) {
    NOTREACHED() << "Got stray response: " << request_id;
    return;
  }
  scoped_ptr<blink::WebServiceWorkerClientInfo> web_client;
  if (!client.IsEmpty()) {
    DCHECK(client.IsValid());
    web_client.reset(new blink::WebServiceWorkerClientInfo(
        ToWebServiceWorkerClientInfo(client)));
  }
  callbacks->onSuccess(web_client.release());
  pending_client_callbacks_.Remove(request_id);
}

void ServiceWorkerScriptContext::OnOpenWindowError(int request_id) {
  TRACE_EVENT0("ServiceWorker",
               "ServiceWorkerScriptContext::OnOpenWindowError");
  blink::WebServiceWorkerClientCallbacks* callbacks =
      pending_client_callbacks_.Lookup(request_id);
  if (!callbacks) {
    NOTREACHED() << "Got stray response: " << request_id;
    return;
  }
  scoped_ptr<blink::WebServiceWorkerError> error(
      new blink::WebServiceWorkerError(
          blink::WebServiceWorkerError::ErrorTypeUnknown,
          "Something went wrong while trying to open the window."));
  callbacks->onError(error.release());
  pending_client_callbacks_.Remove(request_id);
}

void ServiceWorkerScriptContext::OnFocusClientResponse(
    int request_id, const ServiceWorkerClientInfo& client) {
  TRACE_EVENT0("ServiceWorker",
               "ServiceWorkerScriptContext::OnFocusClientResponse");
  blink::WebServiceWorkerClientCallbacks* callback =
      pending_client_callbacks_.Lookup(request_id);
  if (!callback) {
    NOTREACHED() << "Got stray response: " << request_id;
    return;
  }
  if (!client.IsEmpty()) {
    DCHECK(client.IsValid());
    scoped_ptr<blink::WebServiceWorkerClientInfo> web_client (
        new blink::WebServiceWorkerClientInfo(
            ToWebServiceWorkerClientInfo(client)));
    callback->onSuccess(web_client.release());
  } else {
    scoped_ptr<blink::WebServiceWorkerError> error(
        new blink::WebServiceWorkerError(
            blink::WebServiceWorkerError::ErrorTypeNotFound,
            "The WindowClient was not found."));
    callback->onError(error.release());
  }

  pending_client_callbacks_.Remove(request_id);
}

void ServiceWorkerScriptContext::OnDidSkipWaiting(int request_id) {
  TRACE_EVENT0("ServiceWorker",
               "ServiceWorkerScriptContext::OnDidSkipWaiting");
  blink::WebServiceWorkerSkipWaitingCallbacks* callbacks =
      pending_skip_waiting_callbacks_.Lookup(request_id);
  if (!callbacks) {
    NOTREACHED() << "Got stray response: " << request_id;
    return;
  }
  callbacks->onSuccess();
  pending_skip_waiting_callbacks_.Remove(request_id);
}

void ServiceWorkerScriptContext::OnDidClaimClients(int request_id) {
  TRACE_EVENT0("ServiceWorker",
               "ServiceWorkerScriptContext::OnDidClaimClients");
  blink::WebServiceWorkerClientsClaimCallbacks* callbacks =
      pending_claim_clients_callbacks_.Lookup(request_id);
  if (!callbacks) {
    NOTREACHED() << "Got stray response: " << request_id;
    return;
  }
  callbacks->onSuccess();
  pending_claim_clients_callbacks_.Remove(request_id);
}

void ServiceWorkerScriptContext::OnClaimClientsError(
    int request_id,
    blink::WebServiceWorkerError::ErrorType error_type,
    const base::string16& message) {
  TRACE_EVENT0("ServiceWorker",
               "ServiceWorkerScriptContext::OnClaimClientsError");
  blink::WebServiceWorkerClientsClaimCallbacks* callbacks =
      pending_claim_clients_callbacks_.Lookup(request_id);
  if (!callbacks) {
    NOTREACHED() << "Got stray response: " << request_id;
    return;
  }
  scoped_ptr<blink::WebServiceWorkerError> error(
      new blink::WebServiceWorkerError(error_type, message));
  callbacks->onError(error.release());
  pending_claim_clients_callbacks_.Remove(request_id);
}

void ServiceWorkerScriptContext::OnPing() {
  Send(new ServiceWorkerHostMsg_Pong(GetRoutingID()));
}

}  // namespace content
