// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_SERVICE_WORKER_SERVICE_WORKER_CONTEXT_CLIENT_H_
#define CONTENT_RENDERER_SERVICE_WORKER_SERVICE_WORKER_CONTEXT_CLIENT_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/id_map.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/strings/string16.h"
#include "base/time/time.h"
#include "components/payments/content/payment_app.mojom.h"
#include "content/child/webmessageportchannel_impl.h"
#include "content/common/service_worker/service_worker_event_dispatcher.mojom.h"
#include "content/common/service_worker/service_worker_status_code.h"
#include "content/common/service_worker/service_worker_types.h"
#include "ipc/ipc_listener.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "third_party/WebKit/public/platform/WebMessagePortChannel.h"
#include "third_party/WebKit/public/platform/modules/serviceworker/WebServiceWorkerError.h"
#include "third_party/WebKit/public/platform/modules/serviceworker/service_worker_event_status.mojom.h"
#include "third_party/WebKit/public/web/modules/serviceworker/WebServiceWorkerContextClient.h"
#include "third_party/WebKit/public/web/modules/serviceworker/WebServiceWorkerContextProxy.h"
#include "v8/include/v8.h"

namespace base {
class SingleThreadTaskRunner;
class TaskRunner;
}

namespace blink {
class WebDataConsumerHandle;
class WebDataSource;
struct WebServiceWorkerClientQueryOptions;
class WebServiceWorkerContextProxy;
class WebServiceWorkerProvider;
class WebServiceWorkerResponse;
class WebURLResponse;
}

namespace IPC {
class Message;
}

namespace content {

struct PlatformNotificationData;
struct PushEventPayload;
struct ServiceWorkerClientInfo;
class ServiceWorkerProviderContext;
class ServiceWorkerContextClient;
class ThreadSafeSender;
class EmbeddedWorkerInstanceClientImpl;

// This class provides access to/from an ServiceWorker's WorkerGlobalScope.
// Unless otherwise noted, all methods are called on the worker thread.
class ServiceWorkerContextClient : public blink::WebServiceWorkerContextClient,
                                   public mojom::ServiceWorkerEventDispatcher {
 public:
  using SyncCallback =
      base::Callback<void(ServiceWorkerStatusCode,
                          base::Time /* dispatch_event_time */)>;
  using PaymentRequestEventCallback =
      base::Callback<void(ServiceWorkerStatusCode,
                          base::Time /* dispatch_event_time */)>;
  using FetchCallback =
      base::Callback<void(ServiceWorkerStatusCode,
                          base::Time /* dispatch_event_time */)>;

  // Returns a thread-specific client instance.  This does NOT create a
  // new instance.
  static ServiceWorkerContextClient* ThreadSpecificInstance();

  // Called on the main thread.
  ServiceWorkerContextClient(
      int embedded_worker_id,
      int64_t service_worker_version_id,
      const GURL& service_worker_scope,
      const GURL& script_url,
      mojom::ServiceWorkerEventDispatcherRequest dispatcher_request,
      std::unique_ptr<EmbeddedWorkerInstanceClientImpl> embedded_worker_client);
  ~ServiceWorkerContextClient() override;

  void OnMessageReceived(int thread_id,
                         int embedded_worker_id,
                         const IPC::Message& message);

  // WebServiceWorkerContextClient overrides.
  blink::WebURL scope() const override;
  void getClient(
      const blink::WebString&,
      std::unique_ptr<blink::WebServiceWorkerClientCallbacks>) override;
  void getClients(
      const blink::WebServiceWorkerClientQueryOptions&,
      std::unique_ptr<blink::WebServiceWorkerClientsCallbacks>) override;
  void openWindow(
      const blink::WebURL&,
      std::unique_ptr<blink::WebServiceWorkerClientCallbacks>) override;
  void setCachedMetadata(const blink::WebURL&,
                         const char* data,
                         size_t size) override;
  void clearCachedMetadata(const blink::WebURL&) override;
  void workerReadyForInspection() override;

  // Called on the main thread.
  void workerContextFailedToStart() override;
  void workerScriptLoaded() override;
  bool hasAssociatedRegistration() override;

  void workerContextStarted(
      blink::WebServiceWorkerContextProxy* proxy) override;
  void didEvaluateWorkerScript(bool success) override;
  void didInitializeWorkerContext(v8::Local<v8::Context> context) override;
  void willDestroyWorkerContext(v8::Local<v8::Context> context) override;
  void workerContextDestroyed() override;
  void countFeature(uint32_t feature) override;
  void reportException(const blink::WebString& error_message,
                       int line_number,
                       int column_number,
                       const blink::WebString& source_url) override;
  void reportConsoleMessage(int source,
                            int level,
                            const blink::WebString& message,
                            int line_number,
                            const blink::WebString& source_url) override;
  void sendDevToolsMessage(int session_id,
                           int call_id,
                           const blink::WebString& message,
                           const blink::WebString& state) override;
  blink::WebDevToolsAgentClient::WebKitClientMessageLoop*
  createDevToolsMessageLoop() override;
  void didHandleActivateEvent(int request_id,
                              blink::WebServiceWorkerEventResult,
                              double dispatch_event_time) override;
  void didHandleExtendableMessageEvent(
      int request_id,
      blink::WebServiceWorkerEventResult result,
      double dispatch_event_time) override;
  void didHandleInstallEvent(int request_id,
                             blink::WebServiceWorkerEventResult result,
                             double event_dispatch_time) override;
  void respondToFetchEvent(int fetch_event_id,
                           double event_dispatch_time) override;
  void respondToFetchEvent(int fetch_event_id,
                           const blink::WebServiceWorkerResponse& response,
                           double event_dispatch_time) override;
  void didHandleFetchEvent(int fetch_event_id,
                           blink::WebServiceWorkerEventResult result,
                           double dispatch_event_time) override;
  void didHandleNotificationClickEvent(
      int request_id,
      blink::WebServiceWorkerEventResult result,
      double dispatch_event_time) override;
  void didHandleNotificationCloseEvent(
      int request_id,
      blink::WebServiceWorkerEventResult result,
      double dispatch_event_time) override;
  void didHandlePushEvent(int request_id,
                          blink::WebServiceWorkerEventResult result,
                          double dispatch_event_time) override;
  void didHandleSyncEvent(int request_id,
                          blink::WebServiceWorkerEventResult result,
                          double dispatch_event_time) override;
  void didHandlePaymentRequestEvent(int request_id,
                                    blink::WebServiceWorkerEventResult result,
                                    double dispatch_event_time) override;

  // Called on the main thread.
  blink::WebServiceWorkerNetworkProvider* createServiceWorkerNetworkProvider(
      blink::WebDataSource* data_source) override;
  blink::WebServiceWorkerProvider* createServiceWorkerProvider() override;

  void postMessageToClient(const blink::WebString& uuid,
                           const blink::WebString& message,
                           blink::WebMessagePortChannelArray channels) override;
  void focus(const blink::WebString& uuid,
             std::unique_ptr<blink::WebServiceWorkerClientCallbacks>) override;
  void navigate(
      const blink::WebString& uuid,
      const blink::WebURL&,
      std::unique_ptr<blink::WebServiceWorkerClientCallbacks>) override;
  void skipWaiting(std::unique_ptr<blink::WebServiceWorkerSkipWaitingCallbacks>
                       callbacks) override;
  void claim(std::unique_ptr<blink::WebServiceWorkerClientsClaimCallbacks>
                 callbacks) override;
  void registerForeignFetchScopes(
      const blink::WebVector<blink::WebURL>& sub_scopes,
      const blink::WebVector<blink::WebSecurityOrigin>& origins) override;

 private:
  struct WorkerContextData;
  class NavigationPreloadRequest;

  // Get routing_id for sending message to the ServiceWorkerVersion
  // in the browser process.
  int GetRoutingID() const { return embedded_worker_id_; }

  void Send(IPC::Message* message);
  void SendWorkerStarted();
  void SetRegistrationInServiceWorkerGlobalScope(
      const ServiceWorkerRegistrationObjectInfo& info,
      const ServiceWorkerVersionAttributes& attrs);

  // mojom::ServiceWorkerEventDispatcher
  void DispatchActivateEvent(
      const DispatchActivateEventCallback& callback) override;
  void DispatchExtendableMessageEvent(
      mojom::ExtendableMessageEventPtr event,
      const DispatchExtendableMessageEventCallback& callback) override;
  void DispatchFetchEvent(int fetch_event_id,
                          const ServiceWorkerFetchRequest& request,
                          mojom::FetchEventPreloadHandlePtr preload_handle,
                          const DispatchFetchEventCallback& callback) override;
  void DispatchNotificationClickEvent(
      const std::string& notification_id,
      const PlatformNotificationData& notification_data,
      int action_index,
      const base::Optional<base::string16>& reply,
      const DispatchNotificationClickEventCallback& callback) override;
  void DispatchNotificationCloseEvent(
      const std::string& notification_id,
      const PlatformNotificationData& notification_data,
      const DispatchNotificationCloseEventCallback& callback) override;
  void DispatchPushEvent(const PushEventPayload& payload,
                         const DispatchPushEventCallback& callback) override;
  void DispatchSyncEvent(
      const std::string& tag,
      blink::mojom::BackgroundSyncEventLastChance last_chance,
      const DispatchSyncEventCallback& callback) override;
  void DispatchPaymentRequestEvent(
      payments::mojom::PaymentAppRequestPtr app_request,
      const DispatchPaymentRequestEventCallback& callback) override;

  void OnInstallEvent(int request_id);
  void OnNotificationClickEvent(
      int request_id,
      const std::string& notification_id,
      const PlatformNotificationData& notification_data,
      int action_index,
      const base::NullableString16& reply);
  void OnNotificationCloseEvent(
      int request_id,
      const std::string& notification_id,
      const PlatformNotificationData& notification_data);

  void OnDidGetClient(int request_id, const ServiceWorkerClientInfo& client);
  void OnDidGetClients(
      int request_id, const std::vector<ServiceWorkerClientInfo>& clients);
  void OnOpenWindowResponse(int request_id,
                            const ServiceWorkerClientInfo& client);
  void OnOpenWindowError(int request_id, const std::string& message);
  void OnFocusClientResponse(int request_id,
                             const ServiceWorkerClientInfo& client);
  void OnNavigateClientResponse(int request_id,
                                const ServiceWorkerClientInfo& client);
  void OnNavigateClientError(int request_id, const GURL& url);
  void OnDidSkipWaiting(int request_id);
  void OnDidClaimClients(int request_id);
  void OnClaimClientsError(int request_id,
                           blink::WebServiceWorkerError::ErrorType error_type,
                           const base::string16& message);
  void OnPing();

  // Called to resolve the FetchEvent.preloadResponse promise.
  void OnNavigationPreloadResponse(
      int fetch_event_id,
      std::unique_ptr<blink::WebURLResponse> response,
      std::unique_ptr<blink::WebDataConsumerHandle> data_consumer_handle);
  // Called when the navigation preload request completed. Either
  // OnNavigationPreloadComplete() or OnNavigationPreloadError() must be called
  // to release the preload related resources.
  void OnNavigationPreloadComplete(int fetch_event_id);
  // Called when an error occurred while receiving the response of the
  // navigation preload request.
  void OnNavigationPreloadError(
      int fetch_event_id,
      std::unique_ptr<blink::WebServiceWorkerError> error);

  base::WeakPtr<ServiceWorkerContextClient> GetWeakPtr();

  const int embedded_worker_id_;
  const int64_t service_worker_version_id_;
  const GURL service_worker_scope_;
  const GURL script_url_;
  scoped_refptr<ThreadSafeSender> sender_;
  scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner_;
  scoped_refptr<base::TaskRunner> worker_task_runner_;

  scoped_refptr<ServiceWorkerProviderContext> provider_context_;

  // Not owned; this object is destroyed when proxy_ becomes invalid.
  blink::WebServiceWorkerContextProxy* proxy_;

  // This is bound on the worker thread.
  mojom::ServiceWorkerEventDispatcherRequest pending_dispatcher_request_;

  // Renderer-side object corresponding to WebEmbeddedWorkerInstance.
  // This is valid from the ctor to workerContextDestroyed.
  std::unique_ptr<EmbeddedWorkerInstanceClientImpl> embedded_worker_client_;

  // Initialized on the worker thread in workerContextStarted and
  // destructed on the worker thread in willDestroyWorkerContext.
  std::unique_ptr<WorkerContextData> context_;

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerContextClient);
};

}  // namespace content

#endif  // CONTENT_RENDERER_SERVICE_WORKER_SERVICE_WORKER_CONTEXT_CLIENT_H_
