// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_SERVICE_WORKER_SERVICE_WORKER_SCRIPT_CONTEXT_H_
#define CONTENT_RENDERER_SERVICE_WORKER_SERVICE_WORKER_SCRIPT_CONTEXT_H_

#include <map>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/id_map.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string16.h"
#include "base/time/time.h"
#include "content/child/webmessageportchannel_impl.h"
#include "content/common/service_worker/service_worker_types.h"
#include "content/renderer/service_worker/service_worker_cache_storage_dispatcher.h"
#include "third_party/WebKit/public/platform/WebGeofencingEventType.h"
#include "third_party/WebKit/public/platform/WebMessagePortChannel.h"
#include "third_party/WebKit/public/platform/WebServiceWorkerClientFocusCallback.h"
#include "third_party/WebKit/public/platform/WebServiceWorkerClientsInfo.h"
#include "third_party/WebKit/public/platform/WebServiceWorkerEventResult.h"
#include "third_party/WebKit/public/platform/WebServiceWorkerSkipWaitingCallbacks.h"

namespace blink {
struct WebCircularGeofencingRegion;
class WebServiceWorkerContextProxy;
}

namespace IPC {
class Message;
}

namespace content {

struct CrossOriginServiceWorkerClient;
class EmbeddedWorkerContextClient;
struct PlatformNotificationData;

// TODO(kinuko): This should implement WebServiceWorkerContextClient
// rather than having EmbeddedWorkerContextClient implement it.
// See the header comment in embedded_worker_context_client.h for the
// potential EW/SW layering concerns.
class ServiceWorkerScriptContext {
 public:
  ServiceWorkerScriptContext(
      EmbeddedWorkerContextClient* embedded_context,
      blink::WebServiceWorkerContextProxy* proxy);
  ~ServiceWorkerScriptContext();

  void OnMessageReceived(const IPC::Message& message);

  void DidHandleActivateEvent(int request_id,
                              blink::WebServiceWorkerEventResult);
  void DidHandleInstallEvent(int request_id,
                             blink::WebServiceWorkerEventResult result);
  void DidHandleFetchEvent(int request_id,
                           ServiceWorkerFetchEventResult result,
                           const ServiceWorkerResponse& response);
  void DidHandleNotificationClickEvent(
      int request_id,
      blink::WebServiceWorkerEventResult result);
  void DidHandlePushEvent(int request_id,
                          blink::WebServiceWorkerEventResult result);
  void DidHandleSyncEvent(int request_id);
  void DidHandleCrossOriginConnectEvent(int request_id, bool accept_connection);
  void GetClientDocuments(
      blink::WebServiceWorkerClientsCallbacks* callbacks);
  void PostMessageToDocument(
      int client_id,
      const base::string16& message,
      scoped_ptr<blink::WebMessagePortChannelArray> channels);
  void FocusClient(int client_id,
                   blink::WebServiceWorkerClientFocusCallback* callback);
  void SkipWaiting(blink::WebServiceWorkerSkipWaitingCallbacks* callbacks);

  // Send a message to the browser. Takes ownership of |message|.
  void Send(IPC::Message* message);

  // Get routing_id for sending message to the ServiceWorkerVersion
  // in the browser process.
  int GetRoutingID() const;

  blink::WebServiceWorkerCacheStorage* cache_storage() {
    return cache_storage_dispatcher_.get();
  }

 private:
  typedef IDMap<blink::WebServiceWorkerClientsCallbacks, IDMapOwnPointer>
      ClientsCallbacksMap;
  typedef IDMap<blink::WebServiceWorkerClientFocusCallback, IDMapOwnPointer>
      FocusClientCallbacksMap;
  typedef IDMap<blink::WebServiceWorkerSkipWaitingCallbacks, IDMapOwnPointer>
      SkipWaitingCallbacksMap;

  void OnActivateEvent(int request_id);
  void OnInstallEvent(int request_id, int active_version_id);
  void OnFetchEvent(int request_id, const ServiceWorkerFetchRequest& request);
  void OnSyncEvent(int request_id);
  void OnNotificationClickEvent(
      int request_id,
      const std::string& notification_id,
      const PlatformNotificationData& notification_data);
  void OnPushEvent(int request_id, const std::string& data);
  void OnGeofencingEvent(int request_id,
                         blink::WebGeofencingEventType event_type,
                         const std::string& region_id,
                         const blink::WebCircularGeofencingRegion& region);
  void OnCrossOriginConnectEvent(int request_id,
                                 const CrossOriginServiceWorkerClient& client);
  void OnPostMessage(const base::string16& message,
                     const std::vector<int>& sent_message_port_ids,
                     const std::vector<int>& new_routing_ids);
  void OnDidGetClientDocuments(
      int request_id, const std::vector<ServiceWorkerClientInfo>& clients);
  void OnFocusClientResponse(int request_id, bool result);
  void OnDidSkipWaiting(int request_id);

  scoped_ptr<ServiceWorkerCacheStorageDispatcher> cache_storage_dispatcher_;

  // Not owned; embedded_context_ owns this.
  EmbeddedWorkerContextClient* embedded_context_;

  // Not owned; this object is destroyed when proxy_ becomes invalid.
  blink::WebServiceWorkerContextProxy* proxy_;

  // Used for incoming messages from the browser for which an outgoing response
  // back to the browser is expected, the id must be sent back with the
  // response.
  int current_request_id_;

  // Pending callbacks for GetClientDocuments().
  ClientsCallbacksMap pending_clients_callbacks_;

  // Pending callbacks for FocusClient().
  FocusClientCallbacksMap pending_focus_client_callbacks_;

  // Pending callbacks for SkipWaiting().
  SkipWaitingCallbacksMap pending_skip_waiting_callbacks_;

  // Capture timestamps for UMA
  std::map<int, base::TimeTicks> activate_start_timings_;
  std::map<int, base::TimeTicks> fetch_start_timings_;
  std::map<int, base::TimeTicks> install_start_timings_;
  std::map<int, base::TimeTicks> notification_click_start_timings_;
  std::map<int, base::TimeTicks> push_start_timings_;

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerScriptContext);
};

}  // namespace content

#endif  // CONTENT_RENDERER_SERVICE_WORKER_SERVICE_WORKER_SCRIPT_CONTEXT_H_
