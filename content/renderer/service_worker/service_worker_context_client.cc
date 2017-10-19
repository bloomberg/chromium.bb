// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/service_worker/service_worker_context_client.h"

#include <map>
#include <memory>
#include <utility>

#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_checker.h"
#include "base/threading/thread_local.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "content/child/background_sync/background_sync_type_converters.h"
#include "content/child/notifications/notification_data_conversions.h"
#include "content/child/request_extra_data.h"
#include "content/child/service_worker/service_worker_dispatcher.h"
#include "content/child/service_worker/service_worker_handle_reference.h"
#include "content/child/service_worker/service_worker_network_provider.h"
#include "content/child/service_worker/service_worker_provider_context.h"
#include "content/child/service_worker/web_service_worker_impl.h"
#include "content/child/service_worker/web_service_worker_provider_impl.h"
#include "content/child/service_worker/web_service_worker_registration_impl.h"
#include "content/child/thread_safe_sender.h"
#include "content/child/web_data_consumer_handle_impl.h"
#include "content/child/web_url_loader_impl.h"
#include "content/common/devtools_messages.h"
#include "content/common/service_worker/embedded_worker_messages.h"
#include "content/common/service_worker/service_worker_event_dispatcher.mojom.h"
#include "content/common/service_worker/service_worker_messages.h"
#include "content/common/service_worker/service_worker_status_code.h"
#include "content/common/service_worker/service_worker_utils.h"
#include "content/public/child/child_url_loader_factory_getter.h"
#include "content/public/common/content_features.h"
#include "content/public/common/push_event_payload.h"
#include "content/public/common/referrer.h"
#include "content/public/common/service_names.mojom.h"
#include "content/public/renderer/content_renderer_client.h"
#include "content/public/renderer/document_state.h"
#include "content/renderer/devtools/devtools_agent.h"
#include "content/renderer/render_thread_impl.h"
#include "content/renderer/renderer_blink_platform_impl.h"
#include "content/renderer/service_worker/controller_service_worker_impl.h"
#include "content/renderer/service_worker/embedded_worker_devtools_agent.h"
#include "content/renderer/service_worker/embedded_worker_instance_client_impl.h"
#include "content/renderer/service_worker/service_worker_fetch_context_impl.h"
#include "content/renderer/service_worker/service_worker_type_converters.h"
#include "content/renderer/service_worker/service_worker_type_util.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_message_macros.h"
#include "net/base/net_errors.h"
#include "net/http/http_response_headers.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "storage/common/blob_storage/blob_handle.h"
#include "storage/public/interfaces/blobs.mojom.h"
#include "third_party/WebKit/public/platform/InterfaceProvider.h"
#include "third_party/WebKit/public/platform/Platform.h"
#include "third_party/WebKit/public/platform/URLConversion.h"
#include "third_party/WebKit/public/platform/WebBlobRegistry.h"
#include "third_party/WebKit/public/platform/WebReferrerPolicy.h"
#include "third_party/WebKit/public/platform/WebSecurityOrigin.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/WebURLResponse.h"
#include "third_party/WebKit/public/platform/modules/background_fetch/WebBackgroundFetchSettledFetch.h"
#include "third_party/WebKit/public/platform/modules/notifications/WebNotificationData.h"
#include "third_party/WebKit/public/platform/modules/payments/WebPaymentHandlerResponse.h"
#include "third_party/WebKit/public/platform/modules/payments/WebPaymentRequestEventData.h"
#include "third_party/WebKit/public/platform/modules/serviceworker/WebServiceWorkerClientQueryOptions.h"
#include "third_party/WebKit/public/platform/modules/serviceworker/WebServiceWorkerError.h"
#include "third_party/WebKit/public/platform/modules/serviceworker/WebServiceWorkerNetworkProvider.h"
#include "third_party/WebKit/public/platform/modules/serviceworker/WebServiceWorkerRequest.h"
#include "third_party/WebKit/public/platform/modules/serviceworker/WebServiceWorkerResponse.h"
#include "third_party/WebKit/public/platform/modules/serviceworker/service_worker_error_type.mojom.h"
#include "third_party/WebKit/public/platform/modules/serviceworker/service_worker_event_status.mojom.h"
#include "third_party/WebKit/public/platform/modules/serviceworker/service_worker_object.mojom.h"
#include "third_party/WebKit/public/platform/modules/serviceworker/service_worker_registration.mojom.h"
#include "third_party/WebKit/public/web/modules/serviceworker/WebServiceWorkerContextClient.h"
#include "third_party/WebKit/public/web/modules/serviceworker/WebServiceWorkerContextProxy.h"

using blink::WebURLRequest;
using blink::MessagePortChannel;

namespace content {

namespace {

// For now client must be a per-thread instance.
base::LazyInstance<base::ThreadLocalPointer<ServiceWorkerContextClient>>::
    Leaky g_worker_client_tls = LAZY_INSTANCE_INITIALIZER;

// Called on the main thread only and blink owns it.
class WebServiceWorkerNetworkProviderImpl
    : public blink::WebServiceWorkerNetworkProvider {
 public:
  explicit WebServiceWorkerNetworkProviderImpl(
      std::unique_ptr<ServiceWorkerNetworkProvider> provider)
      : provider_(std::move(provider)) {}

  // Blink calls this method for each request starting with the main script,
  // we tag them with the provider id.
  void WillSendRequest(WebURLRequest& request) override {
    std::unique_ptr<RequestExtraData> extra_data(new RequestExtraData);
    extra_data->set_service_worker_provider_id(provider_->provider_id());
    extra_data->set_originated_from_service_worker(true);
    // Service workers are only available in secure contexts, so all requests
    // are initiated in a secure context.
    extra_data->set_initiated_in_secure_context(true);
    request.SetExtraData(extra_data.release());
  }

  std::unique_ptr<blink::WebURLLoader> CreateURLLoader(
      const blink::WebURLRequest& request,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner) override {
    RenderThreadImpl* child_thread = RenderThreadImpl::current();
    if (child_thread && provider_->script_loader_factory() &&
        ServiceWorkerUtils::IsServicificationEnabled() &&
        IsScriptRequest(request)) {
      return base::MakeUnique<WebURLLoaderImpl>(
          child_thread->resource_dispatcher(), std::move(task_runner),
          provider_->script_loader_factory());
    }
    return nullptr;
  }

  int ProviderID() const override { return provider_->provider_id(); }

 private:
  static bool IsScriptRequest(const WebURLRequest& request) {
    auto request_context = request.GetRequestContext();
    return request_context == WebURLRequest::kRequestContextServiceWorker ||
           request_context == WebURLRequest::kRequestContextScript ||
           request_context == WebURLRequest::kRequestContextImport;
  }

  std::unique_ptr<ServiceWorkerNetworkProvider> provider_;
};

class StreamHandleListener
    : public blink::WebServiceWorkerStreamHandle::Listener {
 public:
  StreamHandleListener(
      blink::mojom::ServiceWorkerStreamCallbackPtr callback_ptr)
      : callback_ptr_(std::move(callback_ptr)) {}

  ~StreamHandleListener() override {}

  void OnAborted() override { callback_ptr_->OnAborted(); }
  void OnCompleted() override { callback_ptr_->OnCompleted(); }

 private:
  blink::mojom::ServiceWorkerStreamCallbackPtr callback_ptr_;
};

WebURLRequest::FetchRequestMode GetBlinkFetchRequestMode(
    FetchRequestMode mode) {
  return static_cast<WebURLRequest::FetchRequestMode>(mode);
}

WebURLRequest::FetchCredentialsMode GetBlinkFetchCredentialsMode(
    FetchCredentialsMode credentials_mode) {
  return static_cast<WebURLRequest::FetchCredentialsMode>(credentials_mode);
}

WebURLRequest::FetchRedirectMode GetBlinkFetchRedirectMode(
    FetchRedirectMode redirect_mode) {
  return static_cast<WebURLRequest::FetchRedirectMode>(redirect_mode);
}

WebURLRequest::RequestContext GetBlinkRequestContext(
    RequestContextType request_context_type) {
  return static_cast<WebURLRequest::RequestContext>(request_context_type);
}

WebURLRequest::FrameType GetBlinkFrameType(RequestContextFrameType frame_type) {
  return static_cast<WebURLRequest::FrameType>(frame_type);
}

blink::WebServiceWorkerClientInfo
ToWebServiceWorkerClientInfo(const ServiceWorkerClientInfo& client_info) {
  DCHECK(client_info.IsValid());

  blink::WebServiceWorkerClientInfo web_client_info;

  web_client_info.uuid = blink::WebString::FromASCII(client_info.client_uuid);
  web_client_info.page_visibility_state = client_info.page_visibility_state;
  web_client_info.is_focused = client_info.is_focused;
  web_client_info.url = client_info.url;
  web_client_info.frame_type = GetBlinkFrameType(client_info.frame_type);
  web_client_info.client_type = client_info.client_type;

  return web_client_info;
}

// Converts the |request| to its equivalent type in the Blink API.
// TODO(peter): Remove this when the Mojo FetchAPIRequest type exists.
void ToWebServiceWorkerRequest(const ServiceWorkerFetchRequest& request,
                               blink::WebServiceWorkerRequest* web_request) {
  DCHECK(web_request);

  web_request->SetURL(blink::WebURL(request.url));
  web_request->SetMethod(blink::WebString::FromUTF8(request.method));
  for (const auto& pair : request.headers) {
    web_request->SetHeader(blink::WebString::FromUTF8(pair.first),
                           blink::WebString::FromUTF8(pair.second));
  }
  if (!request.blob_uuid.empty()) {
    DCHECK_EQ(request.blob != nullptr, features::IsMojoBlobsEnabled());
    mojo::ScopedMessagePipeHandle blob_pipe;
    if (request.blob)
      blob_pipe = request.blob->Clone().PassInterface().PassHandle();
    web_request->SetBlob(blink::WebString::FromASCII(request.blob_uuid),
                         request.blob_size, std::move(blob_pipe));
  }
  web_request->SetReferrer(
      blink::WebString::FromUTF8(request.referrer.url.spec()),
      request.referrer.policy);
  web_request->SetMode(GetBlinkFetchRequestMode(request.mode));
  web_request->SetIsMainResourceLoad(request.is_main_resource_load);
  web_request->SetCredentialsMode(
      GetBlinkFetchCredentialsMode(request.credentials_mode));
  web_request->SetRedirectMode(
      GetBlinkFetchRedirectMode(request.redirect_mode));
  web_request->SetRequestContext(
      GetBlinkRequestContext(request.request_context_type));
  web_request->SetFrameType(GetBlinkFrameType(request.frame_type));
  web_request->SetClientId(blink::WebString::FromUTF8(request.client_id));
  web_request->SetIsReload(request.is_reload);
  web_request->SetIntegrity(blink::WebString::FromUTF8(request.integrity));
}

// Converts |response| to its equivalent type in the Blink API.
// TODO(peter): Remove this when the Mojo FetchAPIResponse type exists.
void ToWebServiceWorkerResponse(const ServiceWorkerResponse& response,
                                blink::WebServiceWorkerResponse* web_response) {
  DCHECK(web_response);

  std::vector<blink::WebURL> url_list;
  for (const GURL& url : response.url_list)
    url_list.push_back(blink::WebURL(url));

  web_response->SetURLList(blink::WebVector<blink::WebURL>(url_list));
  web_response->SetStatus(static_cast<unsigned short>(response.status_code));
  web_response->SetStatusText(blink::WebString::FromUTF8(response.status_text));
  web_response->SetResponseType(response.response_type);
  for (const auto& pair : response.headers) {
    web_response->SetHeader(blink::WebString::FromUTF8(pair.first),
                            blink::WebString::FromUTF8(pair.second));
  }
  if (!response.blob_uuid.empty()) {
    DCHECK_EQ(response.blob != nullptr, features::IsMojoBlobsEnabled());
    mojo::ScopedMessagePipeHandle blob_pipe;
    if (response.blob)
      blob_pipe = response.blob->Clone().PassInterface().PassHandle();
    web_response->SetBlob(blink::WebString::FromASCII(response.blob_uuid),
                          response.blob_size, std::move(blob_pipe));
  }
  web_response->SetError(response.error);
  web_response->SetResponseTime(response.response_time);
  if (response.is_in_cache_storage) {
    web_response->SetCacheStorageCacheName(
        blink::WebString::FromUTF8(response.cache_storage_cache_name));
  }

  std::vector<blink::WebString> cors_exposed_header_names;
  for (const auto& name : response.cors_exposed_header_names)
    cors_exposed_header_names.push_back(blink::WebString::FromUTF8(name));

  web_response->SetCorsExposedHeaderNames(
      blink::WebVector<blink::WebString>(cors_exposed_header_names));
}

template <typename T, class... TArgs>
void AbortPendingEventCallbacks(T& callbacks, TArgs... args) {
  for (typename T::iterator it(&callbacks); !it.IsAtEnd(); it.Advance()) {
    std::move(*it.GetCurrentValue())
        .Run(blink::mojom::ServiceWorkerEventStatus::ABORTED, args...,
             base::Time::Now());
  }
}

void GetBlobRegistry(storage::mojom::BlobRegistryRequest request) {
  ChildThreadImpl::current()->GetConnector()->BindInterface(
      mojom::kBrowserServiceName, std::move(request));
}

}  // namespace

// Holding data that needs to be bound to the worker context on the
// worker thread.
struct ServiceWorkerContextClient::WorkerContextData {
  using ClientsCallbacksMap =
      base::IDMap<std::unique_ptr<blink::WebServiceWorkerClientsCallbacks>>;
  using ClaimClientsCallbacksMap = base::IDMap<
      std::unique_ptr<blink::WebServiceWorkerClientsClaimCallbacks>>;
  using ClientCallbacksMap =
      base::IDMap<std::unique_ptr<blink::WebServiceWorkerClientCallbacks>>;
  using SkipWaitingCallbacksMap =
      base::IDMap<std::unique_ptr<blink::WebServiceWorkerSkipWaitingCallbacks>>;
  using InstallEventCallbacksMap =
      base::IDMap<std::unique_ptr<DispatchInstallEventCallback>>;
  using ActivateEventCallbacksMap =
      base::IDMap<std::unique_ptr<DispatchActivateEventCallback>>;
  using BackgroundFetchAbortEventCallbacksMap =
      base::IDMap<std::unique_ptr<DispatchBackgroundFetchAbortEventCallback>>;
  using BackgroundFetchClickEventCallbacksMap =
      base::IDMap<std::unique_ptr<DispatchBackgroundFetchClickEventCallback>>;
  using BackgroundFetchFailEventCallbacksMap =
      base::IDMap<std::unique_ptr<DispatchBackgroundFetchFailEventCallback>>;
  using BackgroundFetchedEventCallbacksMap =
      base::IDMap<std::unique_ptr<DispatchBackgroundFetchedEventCallback>>;
  using SyncEventCallbacksMap =
      base::IDMap<std::unique_ptr<DispatchSyncEventCallback>>;
  using NotificationClickEventCallbacksMap =
      base::IDMap<std::unique_ptr<DispatchNotificationClickEventCallback>>;
  using NotificationCloseEventCallbacksMap =
      base::IDMap<std::unique_ptr<DispatchNotificationCloseEventCallback>>;
  using PushEventCallbacksMap =
      base::IDMap<std::unique_ptr<DispatchPushEventCallback>>;
  using ExtendableMessageEventCallbacksMap =
      base::IDMap<std::unique_ptr<DispatchExtendableMessageEventCallback>>;
  using NavigationPreloadRequestsMap =
      base::IDMap<std::unique_ptr<NavigationPreloadRequest>>;
  using InstallEventMethodsMap =
      std::map<int, mojom::ServiceWorkerInstallEventMethodsAssociatedPtr>;

  explicit WorkerContextData(ServiceWorkerContextClient* owner)
      : event_dispatcher_binding(owner),
        weak_factory(owner),
        proxy_weak_factory(owner->proxy_) {}

  ~WorkerContextData() {
    DCHECK(thread_checker.CalledOnValidThread());

    AbortPendingEventCallbacks(install_event_callbacks,
                               false /* has_fetch_handler */);
    AbortPendingEventCallbacks(activate_event_callbacks);
    AbortPendingEventCallbacks(background_fetch_abort_event_callbacks);
    AbortPendingEventCallbacks(background_fetch_click_event_callbacks);
    AbortPendingEventCallbacks(background_fetch_fail_event_callbacks);
    AbortPendingEventCallbacks(background_fetched_event_callbacks);
    AbortPendingEventCallbacks(sync_event_callbacks);
    AbortPendingEventCallbacks(notification_click_event_callbacks);
    AbortPendingEventCallbacks(notification_close_event_callbacks);
    AbortPendingEventCallbacks(push_event_callbacks);
    AbortPendingEventCallbacks(fetch_event_callbacks);
    AbortPendingEventCallbacks(message_event_callbacks);
  }

  mojo::Binding<mojom::ServiceWorkerEventDispatcher> event_dispatcher_binding;

  // Pending callbacks for GetClientDocuments().
  ClientsCallbacksMap clients_callbacks;

  // Pending callbacks for OpenWindow() and FocusClient().
  ClientCallbacksMap client_callbacks;

  // Pending callbacks for SkipWaiting().
  SkipWaitingCallbacksMap skip_waiting_callbacks;

  // Pending callbacks for ClaimClients().
  ClaimClientsCallbacksMap claim_clients_callbacks;

  // Pending callbacks for Install Events.
  InstallEventCallbacksMap install_event_callbacks;

  // Pending callbacks for Activate Events.
  ActivateEventCallbacksMap activate_event_callbacks;

  // Pending callbacks for Background Fetch Abort Events.
  BackgroundFetchAbortEventCallbacksMap background_fetch_abort_event_callbacks;

  // Pending callbacks for Background Fetch Click Events.
  BackgroundFetchClickEventCallbacksMap background_fetch_click_event_callbacks;

  // Pending callbacks for Background Fetch Fail Events.
  BackgroundFetchFailEventCallbacksMap background_fetch_fail_event_callbacks;

  // Pending callbacks for Background Fetched Events.
  BackgroundFetchedEventCallbacksMap background_fetched_event_callbacks;

  // Pending callbacks for Background Sync Events.
  SyncEventCallbacksMap sync_event_callbacks;

  // Pending callbacks for result of AbortPayment.
  std::map<int /* abort_payment_event_id */,
           payments::mojom::PaymentHandlerResponseCallbackPtr>
      abort_payment_result_callbacks;

  // Pending callbacks for AbortPayment Events.
  std::map<int /* abort_payment_event_id */,
           DispatchCanMakePaymentEventCallback>
      abort_payment_event_callbacks;

  // Pending callbacks for result of CanMakePayment.
  std::map<int /* can_make_payment_event_id */,
           payments::mojom::PaymentHandlerResponseCallbackPtr>
      can_make_payment_result_callbacks;

  // Pending callbacks for CanMakePayment Events.
  std::map<int /* can_make_payment_event_id */,
           DispatchCanMakePaymentEventCallback>
      can_make_payment_event_callbacks;

  // Pending callbacks for Payment App Response.
  std::map<int /* payment_request_id */,
           payments::mojom::PaymentHandlerResponseCallbackPtr>
      payment_response_callbacks;

  // Pending callbacks for Payment Request Events.
  std::map<int /* payment_request_id */, DispatchPaymentRequestEventCallback>
      payment_request_event_callbacks;

  // Pending callbacks for Notification Click Events.
  NotificationClickEventCallbacksMap notification_click_event_callbacks;

  // Pending callbacks for Notification Close Events.
  NotificationCloseEventCallbacksMap notification_close_event_callbacks;

  // Pending callbacks for Push Events.
  PushEventCallbacksMap push_event_callbacks;

  // Pending callbacks for Fetch Events.
  // We use IDMap for minting an id for the event, which is used in
  // other places too like |fetch_response_callbacks| and
  // |NavigationPreloadRequest|.
  base::IDMap<std::unique_ptr<DispatchFetchEventCallback>>
      fetch_event_callbacks;

  // Pending callbacks for respondWith on each fetch event.
  std::map<int /* fetch_event_id */,
           mojom::ServiceWorkerFetchResponseCallbackPtr>
      fetch_response_callbacks;

  // Pending callbacks for Extendable Message Events.
  ExtendableMessageEventCallbacksMap message_event_callbacks;

  // Pending navigation preload requests.
  NavigationPreloadRequestsMap preload_requests;

  // Maps every install event id with its corresponding
  // mojom::ServiceWorkerInstallEventMethodsAssociatedPt.
  InstallEventMethodsMap install_methods_map;

  std::unique_ptr<ControllerServiceWorkerImpl> controller_impl;

  base::ThreadChecker thread_checker;
  base::WeakPtrFactory<ServiceWorkerContextClient> weak_factory;
  base::WeakPtrFactory<blink::WebServiceWorkerContextProxy> proxy_weak_factory;
};

class ServiceWorkerContextClient::NavigationPreloadRequest final
    : public mojom::URLLoaderClient {
 public:
  NavigationPreloadRequest(int fetch_event_id,
                           const GURL& url,
                           mojom::FetchEventPreloadHandlePtr preload_handle)
      : fetch_event_id_(fetch_event_id),
        url_(url),
        url_loader_(std::move(preload_handle->url_loader)),
        binding_(this, std::move(preload_handle->url_loader_client_request)) {}

  ~NavigationPreloadRequest() override {}

  void OnReceiveResponse(
      const ResourceResponseHead& response_head,
      const base::Optional<net::SSLInfo>& ssl_info,
      mojom::DownloadedTempFilePtr downloaded_file) override {
    DCHECK(!response_);
    DCHECK(!downloaded_file);
    response_ = base::MakeUnique<blink::WebURLResponse>();
    // TODO(horo): Set report_security_info to true when DevTools is attached.
    const bool report_security_info = false;
    WebURLLoaderImpl::PopulateURLResponse(url_, response_head, response_.get(),
                                          report_security_info);
    MaybeReportResponseToClient();
  }

  void OnReceiveRedirect(const net::RedirectInfo& redirect_info,
                         const ResourceResponseHead& response_head) override {
    DCHECK(!response_);
    DCHECK(net::HttpResponseHeaders::IsRedirectResponseCode(
        response_head.headers->response_code()));

    ServiceWorkerContextClient* client =
        ServiceWorkerContextClient::ThreadSpecificInstance();
    if (!client)
      return;
    response_ = base::MakeUnique<blink::WebURLResponse>();
    WebURLLoaderImpl::PopulateURLResponse(url_, response_head, response_.get(),
                                          false /* report_security_info */);
    client->OnNavigationPreloadResponse(fetch_event_id_, std::move(response_),
                                        nullptr);
    // This will delete |this|.
    client->OnNavigationPreloadComplete(
        fetch_event_id_, response_head.response_start,
        response_head.encoded_data_length, 0 /* encoded_body_length */,
        0 /* decoded_body_length */);
  }

  void OnDataDownloaded(int64_t data_length,
                        int64_t encoded_data_length) override {
    NOTREACHED();
  }

  void OnUploadProgress(int64_t current_position,
                        int64_t total_size,
                        OnUploadProgressCallback ack_callback) override {
    NOTREACHED();
  }

  void OnReceiveCachedMetadata(const std::vector<uint8_t>& data) override {}

  void OnTransferSizeUpdated(int32_t transfer_size_diff) override {
  }

  void OnStartLoadingResponseBody(
      mojo::ScopedDataPipeConsumerHandle body) override {
    DCHECK(!body_.is_valid());
    body_ = std::move(body);
    MaybeReportResponseToClient();
  }

  void OnComplete(const ResourceRequestCompletionStatus& status) override {
    if (status.error_code != net::OK) {
      std::string message;
      std::string unsanitized_message;
      if (status.error_code == net::ERR_ABORTED) {
        message =
            "The service worker navigation preload request was cancelled "
            "before 'preloadResponse' settled. If you intend to use "
            "'preloadResponse', use waitUntil() or respondWith() to wait for "
            "the promise to settle.";
      } else {
        message =
            "The service worker navigation preload request failed with a "
            "network error.";
        unsanitized_message =
            "The service worker navigation preload request failed with network "
            "error: " +
            net::ErrorToString(status.error_code) + ".";
      }

      // This will delete |this|.
      ReportErrorToClient(message, unsanitized_message);
      return;
    }

    ServiceWorkerContextClient* client =
        ServiceWorkerContextClient::ThreadSpecificInstance();
    if (!client)
      return;
    if (response_) {
      // When the response body from the server is empty, OnComplete() is called
      // without OnStartLoadingResponseBody().
      DCHECK(!body_.is_valid());
      client->OnNavigationPreloadResponse(fetch_event_id_, std::move(response_),
                                          nullptr);
    }
    // This will delete |this|.
    client->OnNavigationPreloadComplete(
        fetch_event_id_, status.completion_time, status.encoded_data_length,
        status.encoded_body_length, status.decoded_body_length);
  }

 private:
  void MaybeReportResponseToClient() {
    if (!response_ || !body_.is_valid())
      return;
    ServiceWorkerContextClient* client =
        ServiceWorkerContextClient::ThreadSpecificInstance();
    if (!client)
      return;

    client->OnNavigationPreloadResponse(
        fetch_event_id_, std::move(response_),
        base::MakeUnique<WebDataConsumerHandleImpl>(std::move(body_)));
  }

  void ReportErrorToClient(const std::string& message,
                           const std::string& unsanitized_message) {
    ServiceWorkerContextClient* client =
        ServiceWorkerContextClient::ThreadSpecificInstance();
    if (!client)
      return;
    // This will delete |this|.
    client->OnNavigationPreloadError(
        fetch_event_id_, base::MakeUnique<blink::WebServiceWorkerError>(
                             blink::mojom::ServiceWorkerErrorType::kNetwork,
                             blink::WebString::FromUTF8(message),
                             blink::WebString::FromUTF8(unsanitized_message)));
  }

  const int fetch_event_id_;
  const GURL url_;
  mojom::URLLoaderPtr url_loader_;
  mojo::Binding<mojom::URLLoaderClient> binding_;

  std::unique_ptr<blink::WebURLResponse> response_;
  mojo::ScopedDataPipeConsumerHandle body_;
};

ServiceWorkerContextClient*
ServiceWorkerContextClient::ThreadSpecificInstance() {
  return g_worker_client_tls.Pointer()->Get();
}

ServiceWorkerContextClient::ServiceWorkerContextClient(
    int embedded_worker_id,
    int64_t service_worker_version_id,
    const GURL& service_worker_scope,
    const GURL& script_url,
    bool is_script_streaming,
    mojom::ServiceWorkerEventDispatcherRequest dispatcher_request,
    mojom::ControllerServiceWorkerRequest controller_request,
    mojom::EmbeddedWorkerInstanceHostAssociatedPtrInfo instance_host,
    mojom::ServiceWorkerProviderInfoForStartWorkerPtr provider_info,
    std::unique_ptr<EmbeddedWorkerInstanceClientImpl> embedded_worker_client)
    : embedded_worker_id_(embedded_worker_id),
      service_worker_version_id_(service_worker_version_id),
      service_worker_scope_(service_worker_scope),
      script_url_(script_url),
      sender_(ChildThreadImpl::current()->thread_safe_sender()),
      main_thread_task_runner_(base::ThreadTaskRunnerHandle::Get()),
      io_thread_task_runner_(ChildThreadImpl::current()->GetIOTaskRunner()),
      proxy_(nullptr),
      pending_dispatcher_request_(std::move(dispatcher_request)),
      pending_controller_request_(std::move(controller_request)),
      embedded_worker_client_(std::move(embedded_worker_client)) {
  instance_host_ =
      mojom::ThreadSafeEmbeddedWorkerInstanceHostAssociatedPtr::Create(
          std::move(instance_host), main_thread_task_runner_);

  // Create a content::ServiceWorkerNetworkProvider for this data source so
  // we can observe its requests.
  pending_network_provider_ = ServiceWorkerNetworkProvider::CreateForController(
      std::move(provider_info));
  provider_context_ = pending_network_provider_->context();

  TRACE_EVENT_NESTABLE_ASYNC_BEGIN1("ServiceWorker",
                                    "ServiceWorkerContextClient", this,
                                    "script_url", script_url_.spec());
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN1(
      "ServiceWorker", "LOAD_SCRIPT", this, "Source",
      (is_script_streaming ? "InstalledScriptsManager" : "ResourceLoader"));
}

ServiceWorkerContextClient::~ServiceWorkerContextClient() {}

void ServiceWorkerContextClient::OnMessageReceived(
    int thread_id,
    int embedded_worker_id,
    const IPC::Message& message) {
  CHECK_EQ(embedded_worker_id_, embedded_worker_id);
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(ServiceWorkerContextClient, message)
    IPC_MESSAGE_HANDLER(ServiceWorkerMsg_DidGetClient, OnDidGetClient)
    IPC_MESSAGE_HANDLER(ServiceWorkerMsg_DidGetClients, OnDidGetClients)
    IPC_MESSAGE_HANDLER(ServiceWorkerMsg_OpenWindowResponse,
                        OnOpenWindowResponse)
    IPC_MESSAGE_HANDLER(ServiceWorkerMsg_OpenWindowError,
                        OnOpenWindowError)
    IPC_MESSAGE_HANDLER(ServiceWorkerMsg_FocusClientResponse,
                        OnFocusClientResponse)
    IPC_MESSAGE_HANDLER(ServiceWorkerMsg_NavigateClientResponse,
                        OnNavigateClientResponse)
    IPC_MESSAGE_HANDLER(ServiceWorkerMsg_NavigateClientError,
                        OnNavigateClientError)
    IPC_MESSAGE_HANDLER(ServiceWorkerMsg_DidSkipWaiting, OnDidSkipWaiting)
    IPC_MESSAGE_HANDLER(ServiceWorkerMsg_DidClaimClients, OnDidClaimClients)
    IPC_MESSAGE_HANDLER(ServiceWorkerMsg_ClaimClientsError, OnClaimClientsError)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  DCHECK(handled);
}

blink::WebURL ServiceWorkerContextClient::Scope() const {
  return service_worker_scope_;
}

void ServiceWorkerContextClient::GetClient(
    const blink::WebString& id,
    std::unique_ptr<blink::WebServiceWorkerClientCallbacks> callbacks) {
  DCHECK(callbacks);
  int request_id = context_->client_callbacks.Add(std::move(callbacks));
  Send(new ServiceWorkerHostMsg_GetClient(GetRoutingID(), request_id,
                                          id.Utf8()));
}

void ServiceWorkerContextClient::GetClients(
    const blink::WebServiceWorkerClientQueryOptions& weboptions,
    std::unique_ptr<blink::WebServiceWorkerClientsCallbacks> callbacks) {
  DCHECK(callbacks);
  int request_id = context_->clients_callbacks.Add(std::move(callbacks));
  ServiceWorkerClientQueryOptions options;
  options.client_type = weboptions.client_type;
  options.include_uncontrolled = weboptions.include_uncontrolled;
  Send(new ServiceWorkerHostMsg_GetClients(
      GetRoutingID(), request_id, options));
}

void ServiceWorkerContextClient::OpenNewTab(
    const blink::WebURL& url,
    std::unique_ptr<blink::WebServiceWorkerClientCallbacks> callbacks) {
  DCHECK(callbacks);
  int request_id = context_->client_callbacks.Add(std::move(callbacks));
  Send(new ServiceWorkerHostMsg_OpenNewTab(GetRoutingID(), request_id, url));
}

void ServiceWorkerContextClient::OpenNewPopup(
    const blink::WebURL& url,
    std::unique_ptr<blink::WebServiceWorkerClientCallbacks> callbacks) {
  DCHECK(callbacks);
  int request_id = context_->client_callbacks.Add(std::move(callbacks));
  Send(new ServiceWorkerHostMsg_OpenNewPopup(GetRoutingID(), request_id, url));
}

void ServiceWorkerContextClient::SetCachedMetadata(const blink::WebURL& url,
                                                   const char* data,
                                                   size_t size) {
  std::vector<char> copy(data, data + size);
  Send(new ServiceWorkerHostMsg_SetCachedMetadata(GetRoutingID(), url, copy));
}

void ServiceWorkerContextClient::ClearCachedMetadata(const blink::WebURL& url) {
  Send(new ServiceWorkerHostMsg_ClearCachedMetadata(GetRoutingID(), url));
}

void ServiceWorkerContextClient::WorkerReadyForInspection() {
  DCHECK(main_thread_task_runner_->RunsTasksInCurrentSequence());
  (*instance_host_)->OnReadyForInspection();
}

void ServiceWorkerContextClient::WorkerContextFailedToStart() {
  DCHECK(main_thread_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(!proxy_);

  (*instance_host_)->OnScriptLoadFailed();
  (*instance_host_)->OnStopped();

  TRACE_EVENT_NESTABLE_ASYNC_END1("ServiceWorker", "ServiceWorkerContextClient",
                                  this, "Status", "WorkerContextFailedToStart");

  DCHECK(embedded_worker_client_);
  embedded_worker_client_->WorkerContextDestroyed();
}

void ServiceWorkerContextClient::WorkerScriptLoaded() {
  (*instance_host_)->OnScriptLoaded();
  TRACE_EVENT_NESTABLE_ASYNC_END0("ServiceWorker", "LOAD_SCRIPT", this);
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN0("ServiceWorker", "START_WORKER_CONTEXT",
                                    this);
}

void ServiceWorkerContextClient::WorkerContextStarted(
    blink::WebServiceWorkerContextProxy* proxy) {
  DCHECK(!worker_task_runner_.get());
  DCHECK_NE(0, WorkerThread::GetCurrentId());
  worker_task_runner_ = base::ThreadTaskRunnerHandle::Get();
  // g_worker_client_tls.Pointer()->Get() could return nullptr if this context
  // gets deleted before workerContextStarted() is called.
  DCHECK(g_worker_client_tls.Pointer()->Get() == nullptr);
  DCHECK(!proxy_);
  g_worker_client_tls.Pointer()->Set(this);
  proxy_ = proxy;

  // Initialize pending callback maps. This needs to be freed on the
  // same thread before the worker context goes away in
  // willDestroyWorkerContext.
  context_.reset(new WorkerContextData(this));

  blink::mojom::ServiceWorkerRegistrationObjectInfoPtr registration_info;
  ServiceWorkerVersionAttributes version_attrs;
  provider_context_->TakeRegistrationForServiceWorkerGlobalScope(
      &registration_info, &version_attrs);
  DCHECK_NE(registration_info->registration_id,
            blink::mojom::kInvalidServiceWorkerRegistrationId);

  DCHECK(pending_dispatcher_request_.is_pending());
  DCHECK(pending_controller_request_.is_pending());
  DCHECK(!context_->event_dispatcher_binding.is_bound());
  DCHECK(!context_->controller_impl);
  context_->event_dispatcher_binding.Bind(
      std::move(pending_dispatcher_request_));

  if (ServiceWorkerUtils::IsServicificationEnabled()) {
    context_->controller_impl = std::make_unique<ControllerServiceWorkerImpl>(
        std::move(pending_controller_request_), GetWeakPtr());
  }

  SetRegistrationInServiceWorkerGlobalScope(std::move(registration_info),
                                            version_attrs);

  (*instance_host_)->OnThreadStarted(WorkerThread::GetCurrentId());

  TRACE_EVENT_NESTABLE_ASYNC_END0("ServiceWorker", "START_WORKER_CONTEXT",
                                  this);
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN0("ServiceWorker", "EVALUATE_SCRIPT", this);
}

void ServiceWorkerContextClient::DidEvaluateWorkerScript(bool success) {
  DCHECK(worker_task_runner_->RunsTasksInCurrentSequence());
  (*instance_host_)->OnScriptEvaluated(success);

  // Schedule a task to send back WorkerStarted asynchronously,
  // so that at the time we send it we can be sure that the
  // worker run loop has been started.
  worker_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&ServiceWorkerContextClient::SendWorkerStarted,
                                GetWeakPtr()));

  TRACE_EVENT_NESTABLE_ASYNC_END1("ServiceWorker", "EVALUATE_SCRIPT", this,
                                  "Status", success ? "Success" : "Failure");
}

void ServiceWorkerContextClient::DidInitializeWorkerContext(
    v8::Local<v8::Context> context) {
  GetContentClient()
      ->renderer()
      ->DidInitializeServiceWorkerContextOnWorkerThread(
          context, service_worker_version_id_, service_worker_scope_,
          script_url_);
}

void ServiceWorkerContextClient::WillDestroyWorkerContext(
    v8::Local<v8::Context> context) {
  // At this point WillStopCurrentWorkerThread is already called, so
  // worker_task_runner_->RunsTasksInCurrentSequence() returns false
  // (while we're still on the worker thread).
  proxy_ = nullptr;

  blob_registry_.reset();

  // We have to clear callbacks now, as they need to be freed on the
  // same thread.
  context_.reset();

  // This also lets the message filter stop dispatching messages to
  // this client.
  g_worker_client_tls.Pointer()->Set(nullptr);

  GetContentClient()->renderer()->WillDestroyServiceWorkerContextOnWorkerThread(
      context, service_worker_version_id_, service_worker_scope_, script_url_);
}

void ServiceWorkerContextClient::WorkerContextDestroyed() {
  DCHECK(g_worker_client_tls.Pointer()->Get() == nullptr);

  // TODO(shimazu): The signals to the browser should be in the order:
  // (1) WorkerStopped (via mojo call EmbeddedWorkerInstanceHost.OnStopped())
  // (2) ProviderDestroyed (via mojo call
  // ServiceWorkerDispatcherHost.OnProviderDestroyed()), this is triggered by
  // the following EmbeddedWorkerInstanceClientImpl::WorkerContextDestroyed(),
  // which will eventually lead to destruction of the service worker provider.
  // But currently EmbeddedWorkerInstanceHost interface is associated with
  // EmbeddedWorkerInstanceClient interface, and ServiceWorkerDispatcherHost
  // interface is associated with the IPC channel, since they are using
  // different mojo message pipes, the FIFO ordering can not be guaranteed now.
  // This will be solved once ServiceWorkerProvider{Host,Client} are mojoified
  // and they are also associated with EmbeddedWorkerInstanceClient in other CLs
  // (https://crrev.com/2653493009 and https://crrev.com/2779763004).
  (*instance_host_)->OnStopped();

  DCHECK(embedded_worker_client_);
  main_thread_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&EmbeddedWorkerInstanceClientImpl::WorkerContextDestroyed,
                     base::Passed(&embedded_worker_client_)));
}

void ServiceWorkerContextClient::CountFeature(uint32_t feature) {
  Send(new EmbeddedWorkerHostMsg_CountFeature(service_worker_version_id_,
                                              feature));
}

void ServiceWorkerContextClient::ReportException(
    const blink::WebString& error_message,
    int line_number,
    int column_number,
    const blink::WebString& source_url) {
  (*instance_host_)
      ->OnReportException(error_message.Utf16(), line_number, column_number,
                          blink::WebStringToGURL(source_url));
}

void ServiceWorkerContextClient::ReportConsoleMessage(
    int source,
    int level,
    const blink::WebString& message,
    int line_number,
    const blink::WebString& source_url) {
  (*instance_host_)
      ->OnReportConsoleMessage(source, level, message.Utf16(), line_number,
                               blink::WebStringToGURL(source_url));
}

void ServiceWorkerContextClient::SendDevToolsMessage(
    int session_id,
    int call_id,
    const blink::WebString& message,
    const blink::WebString& state_cookie) {
  // Return if this context has been stopped.
  if (!embedded_worker_client_)
    return;
  embedded_worker_client_->devtools_agent()->SendMessage(
      sender_.get(), session_id, call_id, message.Utf8(), state_cookie.Utf8());
}

blink::WebDevToolsAgentClient::WebKitClientMessageLoop*
ServiceWorkerContextClient::CreateDevToolsMessageLoop() {
  return DevToolsAgent::createMessageLoopWrapper();
}

void ServiceWorkerContextClient::DidHandleActivateEvent(
    int request_id,
    blink::mojom::ServiceWorkerEventStatus status,
    double event_dispatch_time) {
  DispatchActivateEventCallback* callback =
      context_->activate_event_callbacks.Lookup(request_id);
  DCHECK(callback);
  DCHECK(*callback);
  std::move(*callback).Run(status,
                           base::Time::FromDoubleT(event_dispatch_time));
  context_->activate_event_callbacks.Remove(request_id);
}

void ServiceWorkerContextClient::DidHandleBackgroundFetchAbortEvent(
    int request_id,
    blink::mojom::ServiceWorkerEventStatus status,
    double event_dispatch_time) {
  DispatchBackgroundFetchAbortEventCallback* callback =
      context_->background_fetch_abort_event_callbacks.Lookup(request_id);
  DCHECK(callback);
  DCHECK(*callback);
  std::move(*callback).Run(status,
                           base::Time::FromDoubleT(event_dispatch_time));
  context_->background_fetch_abort_event_callbacks.Remove(request_id);
}

void ServiceWorkerContextClient::DidHandleBackgroundFetchClickEvent(
    int request_id,
    blink::mojom::ServiceWorkerEventStatus status,
    double event_dispatch_time) {
  DispatchBackgroundFetchClickEventCallback* callback =
      context_->background_fetch_click_event_callbacks.Lookup(request_id);
  DCHECK(callback);
  DCHECK(*callback);
  std::move(*callback).Run(status,
                           base::Time::FromDoubleT(event_dispatch_time));
  context_->background_fetch_click_event_callbacks.Remove(request_id);
}

void ServiceWorkerContextClient::DidHandleBackgroundFetchFailEvent(
    int request_id,
    blink::mojom::ServiceWorkerEventStatus status,
    double event_dispatch_time) {
  DispatchBackgroundFetchFailEventCallback* callback =
      context_->background_fetch_fail_event_callbacks.Lookup(request_id);
  DCHECK(callback);
  DCHECK(*callback);
  std::move(*callback).Run(status,
                           base::Time::FromDoubleT(event_dispatch_time));
  context_->background_fetch_fail_event_callbacks.Remove(request_id);
}

void ServiceWorkerContextClient::DidHandleBackgroundFetchedEvent(
    int request_id,
    blink::mojom::ServiceWorkerEventStatus status,
    double event_dispatch_time) {
  DispatchBackgroundFetchedEventCallback* callback =
      context_->background_fetched_event_callbacks.Lookup(request_id);
  DCHECK(callback);
  DCHECK(*callback);
  std::move(*callback).Run(status,
                           base::Time::FromDoubleT(event_dispatch_time));
  context_->background_fetched_event_callbacks.Remove(request_id);
}

void ServiceWorkerContextClient::DidHandleExtendableMessageEvent(
    int request_id,
    blink::mojom::ServiceWorkerEventStatus status,
    double event_dispatch_time) {
  DispatchExtendableMessageEventCallback* callback =
      context_->message_event_callbacks.Lookup(request_id);
  DCHECK(callback);
  DCHECK(*callback);
  std::move(*callback).Run(status,
                           base::Time::FromDoubleT(event_dispatch_time));
  context_->message_event_callbacks.Remove(request_id);
}

void ServiceWorkerContextClient::DidHandleInstallEvent(
    int event_id,
    blink::mojom::ServiceWorkerEventStatus status,
    double event_dispatch_time) {
  DispatchInstallEventCallback* callback =
      context_->install_event_callbacks.Lookup(event_id);
  DCHECK(callback);
  DCHECK(*callback);
  std::move(*callback).Run(status, proxy_->HasFetchEventHandler(),
                           base::Time::FromDoubleT(event_dispatch_time));
  context_->install_event_callbacks.Remove(event_id);
  context_->install_methods_map.erase(event_id);
}

void ServiceWorkerContextClient::RespondToFetchEventWithNoResponse(
    int fetch_event_id,
    double event_dispatch_time) {
  const mojom::ServiceWorkerFetchResponseCallbackPtr& response_callback =
      context_->fetch_response_callbacks[fetch_event_id];
  DCHECK(response_callback.is_bound());
  response_callback->OnFallback(base::Time::FromDoubleT(event_dispatch_time));
  context_->fetch_response_callbacks.erase(fetch_event_id);
}

void ServiceWorkerContextClient::RespondToFetchEvent(
    int fetch_event_id,
    const blink::WebServiceWorkerResponse& web_response,
    double event_dispatch_time) {
  ServiceWorkerResponse response(
      GetServiceWorkerResponseFromWebResponse(web_response));
  const mojom::ServiceWorkerFetchResponseCallbackPtr& response_callback =
      context_->fetch_response_callbacks[fetch_event_id];

  if (response.blob_uuid.size()) {
    // TODO(kinuko): Remove this hack once kMojoBlobs is enabled by default
    // and crbug.com/755523 is resolved.
    storage::mojom::BlobPtr blob_ptr;
    if (response.blob) {
      blob_ptr = response.blob->TakeBlobPtr();
      response.blob = nullptr;
    } else {
      if (!blob_registry_) {
        // TODO(kinuko): We should use per-frame / per-worker InterfaceProvider
        // instead (crbug.com/734210).
        main_thread_task_runner_->PostTask(
            FROM_HERE,
            base::BindOnce(&GetBlobRegistry, MakeRequest(&blob_registry_)));
      }
      blob_registry_->GetBlobFromUUID(MakeRequest(&blob_ptr),
                                      response.blob_uuid);
    }
    response_callback->OnResponseBlob(
        response, std::move(blob_ptr),
        base::Time::FromDoubleT(event_dispatch_time));
  } else {
    response_callback->OnResponse(response,
                                  base::Time::FromDoubleT(event_dispatch_time));
  }
  context_->fetch_response_callbacks.erase(fetch_event_id);
}

void ServiceWorkerContextClient::RespondToFetchEventWithResponseStream(
    int fetch_event_id,
    const blink::WebServiceWorkerResponse& web_response,
    blink::WebServiceWorkerStreamHandle* web_body_as_stream,
    double event_dispatch_time) {
  ServiceWorkerResponse response(
      GetServiceWorkerResponseFromWebResponse(web_response));
  const mojom::ServiceWorkerFetchResponseCallbackPtr& response_callback =
      context_->fetch_response_callbacks[fetch_event_id];
  blink::mojom::ServiceWorkerStreamHandlePtr body_as_stream =
      blink::mojom::ServiceWorkerStreamHandle::New();
  blink::mojom::ServiceWorkerStreamCallbackPtr callback_ptr;
  body_as_stream->callback_request = mojo::MakeRequest(&callback_ptr);
  body_as_stream->stream = web_body_as_stream->DrainStreamDataPipe();
  DCHECK(body_as_stream->stream.is_valid());

  web_body_as_stream->SetListener(
      base::MakeUnique<StreamHandleListener>(std::move(callback_ptr)));

  response_callback->OnResponseStream(
      response, std::move(body_as_stream),
      base::Time::FromDoubleT(event_dispatch_time));
  context_->fetch_response_callbacks.erase(fetch_event_id);
}

void ServiceWorkerContextClient::DidHandleFetchEvent(
    int fetch_event_id,
    blink::mojom::ServiceWorkerEventStatus status,
    double event_dispatch_time) {
  // This TRACE_EVENT is used for perf benchmark to confirm if all of fetch
  // events have completed. (crbug.com/736697)
  TRACE_EVENT1("ServiceWorker",
               "ServiceWorkerContextClient::DidHandleFetchEvent", "event_id",
               fetch_event_id);
  DispatchFetchEventCallback* callback =
      context_->fetch_event_callbacks.Lookup(fetch_event_id);
  DCHECK(callback);
  DCHECK(*callback);
  std::move(*callback).Run(status,
                           base::Time::FromDoubleT(event_dispatch_time));

  context_->fetch_event_callbacks.Remove(fetch_event_id);
}

void ServiceWorkerContextClient::DidHandleNotificationClickEvent(
    int request_id,
    blink::mojom::ServiceWorkerEventStatus status,
    double event_dispatch_time) {
  DispatchNotificationClickEventCallback* callback =
      context_->notification_click_event_callbacks.Lookup(request_id);
  DCHECK(callback);
  DCHECK(*callback);
  std::move(*callback).Run(status,
                           base::Time::FromDoubleT(event_dispatch_time));

  context_->notification_click_event_callbacks.Remove(request_id);
}

void ServiceWorkerContextClient::DidHandleNotificationCloseEvent(
    int request_id,
    blink::mojom::ServiceWorkerEventStatus status,
    double event_dispatch_time) {
  DispatchNotificationCloseEventCallback* callback =
      context_->notification_close_event_callbacks.Lookup(request_id);
  DCHECK(callback);
  DCHECK(*callback);
  std::move(*callback).Run(status,
                           base::Time::FromDoubleT(event_dispatch_time));

  context_->notification_close_event_callbacks.Remove(request_id);
}

void ServiceWorkerContextClient::DidHandlePushEvent(
    int request_id,
    blink::mojom::ServiceWorkerEventStatus status,
    double event_dispatch_time) {
  DispatchPushEventCallback* callback =
      context_->push_event_callbacks.Lookup(request_id);
  DCHECK(callback);
  DCHECK(*callback);
  std::move(*callback).Run(status,
                           base::Time::FromDoubleT(event_dispatch_time));
  context_->push_event_callbacks.Remove(request_id);
}

void ServiceWorkerContextClient::DidHandleSyncEvent(
    int request_id,
    blink::mojom::ServiceWorkerEventStatus status,
    double event_dispatch_time) {
  DispatchSyncEventCallback* callback =
      context_->sync_event_callbacks.Lookup(request_id);
  DCHECK(callback);
  DCHECK(*callback);
  std::move(*callback).Run(status,
                           base::Time::FromDoubleT(event_dispatch_time));
  context_->sync_event_callbacks.Remove(request_id);
}

void ServiceWorkerContextClient::RespondToAbortPaymentEvent(
    int event_id,
    bool payment_aborted,
    double dispatch_event_time) {
  const payments::mojom::PaymentHandlerResponseCallbackPtr& result_callback =
      context_->abort_payment_result_callbacks[event_id];
  result_callback->OnResponseForAbortPayment(
      payment_aborted, base::Time::FromDoubleT(dispatch_event_time));
  context_->abort_payment_result_callbacks.erase(event_id);
}

void ServiceWorkerContextClient::DidHandleAbortPaymentEvent(
    int event_id,
    blink::mojom::ServiceWorkerEventStatus status,
    double dispatch_event_time) {
  DispatchAbortPaymentEventCallback callback =
      std::move(context_->abort_payment_event_callbacks[event_id]);
  std::move(callback).Run(status, base::Time::FromDoubleT(dispatch_event_time));
  context_->abort_payment_event_callbacks.erase(event_id);
}

void ServiceWorkerContextClient::RespondToCanMakePaymentEvent(
    int event_id,
    bool can_make_payment,
    double dispatch_event_time) {
  const payments::mojom::PaymentHandlerResponseCallbackPtr& result_callback =
      context_->can_make_payment_result_callbacks[event_id];
  result_callback->OnResponseForCanMakePayment(
      can_make_payment, base::Time::FromDoubleT(dispatch_event_time));
  context_->can_make_payment_result_callbacks.erase(event_id);
}

void ServiceWorkerContextClient::DidHandleCanMakePaymentEvent(
    int event_id,
    blink::mojom::ServiceWorkerEventStatus status,
    double dispatch_event_time) {
  DispatchCanMakePaymentEventCallback callback =
      std::move(context_->can_make_payment_event_callbacks[event_id]);
  std::move(callback).Run(status, base::Time::FromDoubleT(dispatch_event_time));
  context_->can_make_payment_event_callbacks.erase(event_id);
}

void ServiceWorkerContextClient::RespondToPaymentRequestEvent(
    int payment_request_id,
    const blink::WebPaymentHandlerResponse& web_response,
    double dispatch_event_time) {
  const payments::mojom::PaymentHandlerResponseCallbackPtr& response_callback =
      context_->payment_response_callbacks[payment_request_id];
  payments::mojom::PaymentHandlerResponsePtr response =
      payments::mojom::PaymentHandlerResponse::New();
  response->method_name = web_response.method_name.Utf8();
  response->stringified_details = web_response.stringified_details.Utf8();
  response_callback->OnResponseForPaymentRequest(
      std::move(response), base::Time::FromDoubleT(dispatch_event_time));
  context_->payment_response_callbacks.erase(payment_request_id);
}

void ServiceWorkerContextClient::DidHandlePaymentRequestEvent(
    int payment_request_id,
    blink::mojom::ServiceWorkerEventStatus status,
    double event_dispatch_time) {
  DispatchPaymentRequestEventCallback callback =
      std::move(context_->payment_request_event_callbacks[payment_request_id]);
  std::move(callback).Run(status, base::Time::FromDoubleT(event_dispatch_time));
  context_->payment_request_event_callbacks.erase(payment_request_id);
}

std::unique_ptr<blink::WebServiceWorkerNetworkProvider>
ServiceWorkerContextClient::CreateServiceWorkerNetworkProvider() {
  DCHECK(main_thread_task_runner_->RunsTasksInCurrentSequence());
  // Blink is responsible for deleting the returned object.
  return base::MakeUnique<WebServiceWorkerNetworkProviderImpl>(
      std::move(pending_network_provider_));
}

std::unique_ptr<blink::WebWorkerFetchContext>
ServiceWorkerContextClient::CreateServiceWorkerFetchContext() {
  DCHECK(main_thread_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(base::FeatureList::IsEnabled(features::kOffMainThreadFetch));

  scoped_refptr<ChildURLLoaderFactoryGetter> url_loader_factory_getter =
      RenderThreadImpl::current()
          ->blink_platform_impl()
          ->CreateDefaultURLLoaderFactoryGetter();
  DCHECK(url_loader_factory_getter);
  // Blink is responsible for deleting the returned object.
  return base::MakeUnique<ServiceWorkerFetchContextImpl>(
      script_url_, url_loader_factory_getter->GetClonedInfo(),
      provider_context_->provider_id());
}

std::unique_ptr<blink::WebServiceWorkerProvider>
ServiceWorkerContextClient::CreateServiceWorkerProvider() {
  DCHECK(main_thread_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(provider_context_);

  // Blink is responsible for deleting the returned object.
  return base::MakeUnique<WebServiceWorkerProviderImpl>(
      sender_.get(), provider_context_.get());
}

void ServiceWorkerContextClient::PostMessageToClient(
    const blink::WebString& uuid,
    const blink::WebString& message,
    blink::WebVector<MessagePortChannel> channels) {
  Send(new ServiceWorkerHostMsg_PostMessageToClient(
      GetRoutingID(), uuid.Utf8(), message.Utf16(), channels.ReleaseVector()));
}

void ServiceWorkerContextClient::Focus(
    const blink::WebString& uuid,
    std::unique_ptr<blink::WebServiceWorkerClientCallbacks> callback) {
  DCHECK(callback);
  int request_id = context_->client_callbacks.Add(std::move(callback));
  Send(new ServiceWorkerHostMsg_FocusClient(GetRoutingID(), request_id,
                                            uuid.Utf8()));
}

void ServiceWorkerContextClient::Navigate(
    const blink::WebString& uuid,
    const blink::WebURL& url,
    std::unique_ptr<blink::WebServiceWorkerClientCallbacks> callback) {
  DCHECK(callback);
  int request_id = context_->client_callbacks.Add(std::move(callback));
  Send(new ServiceWorkerHostMsg_NavigateClient(GetRoutingID(), request_id,
                                               uuid.Utf8(), url));
}

void ServiceWorkerContextClient::SkipWaiting(
    std::unique_ptr<blink::WebServiceWorkerSkipWaitingCallbacks> callbacks) {
  DCHECK(callbacks);
  int request_id = context_->skip_waiting_callbacks.Add(std::move(callbacks));
  Send(new ServiceWorkerHostMsg_SkipWaiting(GetRoutingID(), request_id));
}

void ServiceWorkerContextClient::Claim(
    std::unique_ptr<blink::WebServiceWorkerClientsClaimCallbacks> callbacks) {
  DCHECK(callbacks);
  int request_id = context_->claim_clients_callbacks.Add(std::move(callbacks));
  Send(new ServiceWorkerHostMsg_ClaimClients(GetRoutingID(), request_id));
}

void ServiceWorkerContextClient::RegisterForeignFetchScopes(
    int install_event_id,
    const blink::WebVector<blink::WebURL>& sub_scopes,
    const blink::WebVector<blink::WebSecurityOrigin>& origins) {
  DCHECK(context_->install_methods_map[install_event_id].is_bound());
  context_->install_methods_map[install_event_id]->RegisterForeignFetchScopes(
      std::vector<GURL>(sub_scopes.begin(), sub_scopes.end()),
      std::vector<url::Origin>(origins.begin(), origins.end()));
}

void ServiceWorkerContextClient::DispatchSyncEvent(
    const std::string& tag,
    blink::mojom::BackgroundSyncEventLastChance last_chance,
    DispatchSyncEventCallback callback) {
  TRACE_EVENT0("ServiceWorker",
               "ServiceWorkerContextClient::DispatchSyncEvent");
  int request_id = context_->sync_event_callbacks.Add(
      base::MakeUnique<DispatchSyncEventCallback>(std::move(callback)));

  // TODO(shimazu): Use typemap when this is moved to blink-side.
  blink::WebServiceWorkerContextProxy::LastChanceOption web_last_chance =
      mojo::ConvertTo<blink::WebServiceWorkerContextProxy::LastChanceOption>(
          last_chance);

  // TODO(jkarlin): Make this blink::WebString::FromUTF8Lenient once
  // https://crrev.com/1768063002/ lands.
  proxy_->DispatchSyncEvent(request_id, blink::WebString::FromUTF8(tag),
                            web_last_chance);
}

void ServiceWorkerContextClient::DispatchAbortPaymentEvent(
    int event_id,
    payments::mojom::PaymentHandlerResponseCallbackPtr response_callback,
    DispatchAbortPaymentEventCallback callback) {
  TRACE_EVENT0("ServiceWorker",
               "ServiceWorkerContextClient::DispatchAbortPaymentEvent");
  context_->abort_payment_result_callbacks.insert(
      std::make_pair(event_id, std::move(response_callback)));
  context_->abort_payment_event_callbacks.insert(
      std::make_pair(event_id, std::move(callback)));
  proxy_->DispatchAbortPaymentEvent(event_id);
}

void ServiceWorkerContextClient::DispatchCanMakePaymentEvent(
    int event_id,
    payments::mojom::CanMakePaymentEventDataPtr eventData,
    payments::mojom::PaymentHandlerResponseCallbackPtr response_callback,
    DispatchCanMakePaymentEventCallback callback) {
  TRACE_EVENT0("ServiceWorker",
               "ServiceWorkerContextClient::DispatchCanMakePaymentEvent");
  context_->can_make_payment_result_callbacks.insert(
      std::make_pair(event_id, std::move(response_callback)));
  context_->can_make_payment_event_callbacks.insert(
      std::make_pair(event_id, std::move(callback)));

  blink::WebCanMakePaymentEventData webEventData =
      mojo::ConvertTo<blink::WebCanMakePaymentEventData>(std::move(eventData));
  proxy_->DispatchCanMakePaymentEvent(event_id, webEventData);
}

void ServiceWorkerContextClient::DispatchPaymentRequestEvent(
    int payment_request_id,
    payments::mojom::PaymentRequestEventDataPtr eventData,
    payments::mojom::PaymentHandlerResponseCallbackPtr response_callback,
    DispatchPaymentRequestEventCallback callback) {
  TRACE_EVENT0("ServiceWorker",
               "ServiceWorkerContextClient::DispatchPaymentRequestEvent");
  context_->payment_response_callbacks.insert(
      std::make_pair(payment_request_id, std::move(response_callback)));
  context_->payment_request_event_callbacks.insert(
      std::make_pair(payment_request_id, std::move(callback)));

  blink::WebPaymentRequestEventData webEventData =
      mojo::ConvertTo<blink::WebPaymentRequestEventData>(std::move(eventData));
  proxy_->DispatchPaymentRequestEvent(payment_request_id, webEventData);
}

void ServiceWorkerContextClient::Send(IPC::Message* message) {
  sender_->Send(message);
}

void ServiceWorkerContextClient::SendWorkerStarted() {
  DCHECK(worker_task_runner_->RunsTasksInCurrentSequence());
  mojom::EmbeddedWorkerStartTimingPtr timing =
      mojom::EmbeddedWorkerStartTiming::New();
  timing->start_worker_received_time = start_worker_received_time_;
  timing->blink_initialized_time = blink_initialized_time_;
  (*instance_host_)->OnStarted(std::move(timing));
  TRACE_EVENT_NESTABLE_ASYNC_END0("ServiceWorker", "ServiceWorkerContextClient",
                                  this);
}

void ServiceWorkerContextClient::SetRegistrationInServiceWorkerGlobalScope(
    blink::mojom::ServiceWorkerRegistrationObjectInfoPtr info,
    const ServiceWorkerVersionAttributes& attrs) {
  DCHECK(worker_task_runner_->RunsTasksInCurrentSequence());
  ServiceWorkerDispatcher* dispatcher =
      ServiceWorkerDispatcher::GetOrCreateThreadSpecificInstance(
          sender_.get(), main_thread_task_runner_.get());

  // Register a registration and its version attributes with the dispatcher
  // living on the worker thread.
  scoped_refptr<WebServiceWorkerRegistrationImpl> registration(
      dispatcher->GetOrCreateRegistrationForServiceWorkerGlobalScope(
          std::move(info), attrs, io_thread_task_runner_));

  proxy_->SetRegistration(
      WebServiceWorkerRegistrationImpl::CreateHandle(registration));
}

void ServiceWorkerContextClient::DispatchActivateEvent(
    DispatchActivateEventCallback callback) {
  TRACE_EVENT0("ServiceWorker",
               "ServiceWorkerContextClient::DispatchActivateEvent");
  int request_id = context_->activate_event_callbacks.Add(
      base::MakeUnique<DispatchActivateEventCallback>(std::move(callback)));
  proxy_->DispatchActivateEvent(request_id);
}

void ServiceWorkerContextClient::DispatchBackgroundFetchAbortEvent(
    const std::string& developer_id,
    DispatchBackgroundFetchAbortEventCallback callback) {
  TRACE_EVENT0("ServiceWorker",
               "ServiceWorkerContextClient::DispatchBackgroundFetchAbortEvent");
  int request_id = context_->background_fetch_abort_event_callbacks.Add(
      base::MakeUnique<DispatchBackgroundFetchAbortEventCallback>(
          std::move(callback)));

  proxy_->DispatchBackgroundFetchAbortEvent(
      request_id, blink::WebString::FromUTF8(developer_id));
}

void ServiceWorkerContextClient::DispatchBackgroundFetchClickEvent(
    const std::string& developer_id,
    mojom::BackgroundFetchState state,
    DispatchBackgroundFetchClickEventCallback callback) {
  TRACE_EVENT0("ServiceWorker",
               "ServiceWorkerContextClient::DispatchBackgroundFetchClickEvent");
  int request_id = context_->background_fetch_click_event_callbacks.Add(
      base::MakeUnique<DispatchBackgroundFetchClickEventCallback>(
          std::move(callback)));

  // TODO(peter): Use typemap when this is moved to blink-side.
  blink::WebServiceWorkerContextProxy::BackgroundFetchState web_state =
      mojo::ConvertTo<
          blink::WebServiceWorkerContextProxy::BackgroundFetchState>(state);

  proxy_->DispatchBackgroundFetchClickEvent(
      request_id, blink::WebString::FromUTF8(developer_id), web_state);
}

void ServiceWorkerContextClient::DispatchBackgroundFetchFailEvent(
    const std::string& developer_id,
    const std::vector<BackgroundFetchSettledFetch>& fetches,
    DispatchBackgroundFetchFailEventCallback callback) {
  TRACE_EVENT0("ServiceWorker",
               "ServiceWorkerContextClient::DispatchBackgroundFetchFailEvent");
  int request_id = context_->background_fetch_fail_event_callbacks.Add(
      base::MakeUnique<DispatchBackgroundFetchFailEventCallback>(
          std::move(callback)));

  blink::WebVector<blink::WebBackgroundFetchSettledFetch> web_fetches(
      fetches.size());
  for (size_t i = 0; i < fetches.size(); ++i) {
    ToWebServiceWorkerRequest(fetches[i].request, &web_fetches[i].request);
    ToWebServiceWorkerResponse(fetches[i].response, &web_fetches[i].response);
  }

  proxy_->DispatchBackgroundFetchFailEvent(
      request_id, blink::WebString::FromUTF8(developer_id), web_fetches);
}

void ServiceWorkerContextClient::DispatchBackgroundFetchedEvent(
    const std::string& developer_id,
    const std::string& unique_id,
    const std::vector<BackgroundFetchSettledFetch>& fetches,
    DispatchBackgroundFetchedEventCallback callback) {
  TRACE_EVENT0("ServiceWorker",
               "ServiceWorkerContextClient::DispatchBackgroundFetchedEvent");
  int request_id = context_->background_fetched_event_callbacks.Add(
      base::MakeUnique<DispatchBackgroundFetchedEventCallback>(
          std::move(callback)));

  blink::WebVector<blink::WebBackgroundFetchSettledFetch> web_fetches(
      fetches.size());
  for (size_t i = 0; i < fetches.size(); ++i) {
    ToWebServiceWorkerRequest(fetches[i].request, &web_fetches[i].request);
    ToWebServiceWorkerResponse(fetches[i].response, &web_fetches[i].response);
  }

  proxy_->DispatchBackgroundFetchedEvent(
      request_id, blink::WebString::FromUTF8(developer_id),
      blink::WebString::FromUTF8(unique_id), web_fetches);
}

void ServiceWorkerContextClient::DispatchInstallEvent(
    mojom::ServiceWorkerInstallEventMethodsAssociatedPtrInfo client,
    DispatchInstallEventCallback callback) {
  TRACE_EVENT0("ServiceWorker",
               "ServiceWorkerContextClient::DispatchInstallEvent");

  int event_id = context_->install_event_callbacks.Add(
      base::MakeUnique<DispatchInstallEventCallback>(std::move(callback)));

  DCHECK(!context_->install_methods_map.count(event_id));
  mojom::ServiceWorkerInstallEventMethodsAssociatedPtr install_methods;
  install_methods.Bind(std::move(client));
  context_->install_methods_map[event_id] = std::move(install_methods);

  proxy_->DispatchInstallEvent(event_id);
}

void ServiceWorkerContextClient::DispatchExtendableMessageEvent(
    mojom::ExtendableMessageEventPtr event,
    DispatchExtendableMessageEventCallback callback) {
  TRACE_EVENT0("ServiceWorker",
               "ServiceWorkerContextClient::DispatchExtendableMessageEvent");
  int request_id = context_->message_event_callbacks.Add(
      base::MakeUnique<DispatchExtendableMessageEventCallback>(
          std::move(callback)));

  if (event->source.client_info.IsValid()) {
    blink::WebServiceWorkerClientInfo web_client =
        ToWebServiceWorkerClientInfo(event->source.client_info);
    proxy_->DispatchExtendableMessageEvent(
        request_id, blink::WebString::FromUTF16(event->message),
        event->source_origin,
        MessagePortChannel::CreateFromHandles(std::move(event->message_ports)),
        web_client);
    return;
  }

  DCHECK(event->source.service_worker_info.handle_id !=
             blink::mojom::kInvalidServiceWorkerHandleId &&
         event->source.service_worker_info.version_id !=
             blink::mojom::kInvalidServiceWorkerVersionId);
  std::unique_ptr<ServiceWorkerHandleReference> handle =
      ServiceWorkerHandleReference::Adopt(event->source.service_worker_info,
                                          sender_.get());
  ServiceWorkerDispatcher* dispatcher =
      ServiceWorkerDispatcher::GetOrCreateThreadSpecificInstance(
          sender_.get(), main_thread_task_runner_.get());
  scoped_refptr<WebServiceWorkerImpl> worker =
      dispatcher->GetOrCreateServiceWorker(std::move(handle));
  proxy_->DispatchExtendableMessageEvent(
      request_id, blink::WebString::FromUTF16(event->message),
      event->source_origin,
      MessagePortChannel::CreateFromHandles(std::move(event->message_ports)),
      WebServiceWorkerImpl::CreateHandle(worker));
}

void ServiceWorkerContextClient::DispatchFetchEvent(
    const ServiceWorkerFetchRequest& request,
    mojom::FetchEventPreloadHandlePtr preload_handle,
    mojom::ServiceWorkerFetchResponseCallbackPtr response_callback,
    DispatchFetchEventCallback callback) {
  // Register callbacks for notifying about event completion and the response.
  int fetch_event_id = context_->fetch_event_callbacks.Add(
      base::MakeUnique<DispatchFetchEventCallback>(std::move(callback)));
  context_->fetch_response_callbacks.insert(
      std::make_pair(fetch_event_id, std::move(response_callback)));

  // This TRACE_EVENT is used for perf benchmark to confirm if all of fetch
  // events have completed. (crbug.com/736697)
  TRACE_EVENT1("ServiceWorker",
               "ServiceWorkerContextClient::DispatchFetchEvent", "event_id",
               fetch_event_id);

  // Set up for navigation preload (FetchEvent#preloadResponse) if needed.
  std::unique_ptr<NavigationPreloadRequest> preload_request =
      preload_handle
          ? base::MakeUnique<NavigationPreloadRequest>(
                fetch_event_id, request.url, std::move(preload_handle))
          : nullptr;
  const bool navigation_preload_sent = !!preload_request;
  if (preload_request) {
    context_->preload_requests.AddWithID(std::move(preload_request),
                                         fetch_event_id);
  }

  // Dispatch the event to the service worker execution context.
  blink::WebServiceWorkerRequest web_request;
  ToWebServiceWorkerRequest(request, &web_request);
  if (request.fetch_type == ServiceWorkerFetchType::FOREIGN_FETCH) {
    proxy_->DispatchForeignFetchEvent(fetch_event_id, web_request);
  } else {
    proxy_->DispatchFetchEvent(fetch_event_id, web_request,
                               navigation_preload_sent);
  }
}

void ServiceWorkerContextClient::DispatchNotificationClickEvent(
    const std::string& notification_id,
    const PlatformNotificationData& notification_data,
    int action_index,
    const base::Optional<base::string16>& reply,
    DispatchNotificationClickEventCallback callback) {
  TRACE_EVENT0("ServiceWorker",
               "ServiceWorkerContextClient::DispatchNotificationClickEvent");

  int request_id = context_->notification_click_event_callbacks.Add(
      base::MakeUnique<DispatchNotificationClickEventCallback>(
          std::move(callback)));

  blink::WebString web_reply;
  if (reply)
    web_reply = blink::WebString::FromUTF16(reply.value());

  proxy_->DispatchNotificationClickEvent(
      request_id, blink::WebString::FromUTF8(notification_id),
      ToWebNotificationData(notification_data), action_index, web_reply);
}

void ServiceWorkerContextClient::DispatchNotificationCloseEvent(
    const std::string& notification_id,
    const PlatformNotificationData& notification_data,
    DispatchNotificationCloseEventCallback callback) {
  TRACE_EVENT0("ServiceWorker",
               "ServiceWorkerContextClient::DispatchNotificationCloseEvent");

  int request_id = context_->notification_close_event_callbacks.Add(
      base::MakeUnique<DispatchNotificationCloseEventCallback>(
          std::move(callback)));

  proxy_->DispatchNotificationCloseEvent(
      request_id, blink::WebString::FromUTF8(notification_id),
      ToWebNotificationData(notification_data));
}

void ServiceWorkerContextClient::DispatchPushEvent(
    const PushEventPayload& payload,
    DispatchPushEventCallback callback) {
  TRACE_EVENT0("ServiceWorker",
               "ServiceWorkerContextClient::DispatchPushEvent");
  int request_id = context_->push_event_callbacks.Add(
      base::MakeUnique<DispatchPushEventCallback>(std::move(callback)));

  // Only set data to be a valid string if the payload had decrypted data.
  blink::WebString data;
  if (!payload.is_null)
    data.Assign(blink::WebString::FromUTF8(payload.data));
  proxy_->DispatchPushEvent(request_id, data);
}

void ServiceWorkerContextClient::OnDidGetClient(
    int request_id,
    const ServiceWorkerClientInfo& client) {
  TRACE_EVENT0("ServiceWorker", "ServiceWorkerContextClient::OnDidGetClient");
  blink::WebServiceWorkerClientCallbacks* callbacks =
      context_->client_callbacks.Lookup(request_id);
  if (!callbacks) {
    NOTREACHED() << "Got stray response: " << request_id;
    return;
  }
  std::unique_ptr<blink::WebServiceWorkerClientInfo> web_client;
  if (!client.IsEmpty()) {
    DCHECK(client.IsValid());
    web_client.reset(new blink::WebServiceWorkerClientInfo(
        ToWebServiceWorkerClientInfo(client)));
  }
  callbacks->OnSuccess(std::move(web_client));
  context_->client_callbacks.Remove(request_id);
}

void ServiceWorkerContextClient::OnDidGetClients(
    int request_id, const std::vector<ServiceWorkerClientInfo>& clients) {
  TRACE_EVENT0("ServiceWorker",
               "ServiceWorkerContextClient::OnDidGetClients");
  blink::WebServiceWorkerClientsCallbacks* callbacks =
      context_->clients_callbacks.Lookup(request_id);
  if (!callbacks) {
    NOTREACHED() << "Got stray response: " << request_id;
    return;
  }
  blink::WebServiceWorkerClientsInfo info;
  blink::WebVector<blink::WebServiceWorkerClientInfo> convertedClients(
      clients.size());
  for (size_t i = 0; i < clients.size(); ++i)
    convertedClients[i] = ToWebServiceWorkerClientInfo(clients[i]);
  info.clients.Swap(convertedClients);
  callbacks->OnSuccess(info);
  context_->clients_callbacks.Remove(request_id);
}

void ServiceWorkerContextClient::OnOpenWindowResponse(
    int request_id,
    const ServiceWorkerClientInfo& client) {
  TRACE_EVENT0("ServiceWorker",
               "ServiceWorkerContextClient::OnOpenWindowResponse");
  blink::WebServiceWorkerClientCallbacks* callbacks =
      context_->client_callbacks.Lookup(request_id);
  if (!callbacks) {
    NOTREACHED() << "Got stray response: " << request_id;
    return;
  }
  std::unique_ptr<blink::WebServiceWorkerClientInfo> web_client;
  if (!client.IsEmpty()) {
    DCHECK(client.IsValid());
    web_client.reset(new blink::WebServiceWorkerClientInfo(
        ToWebServiceWorkerClientInfo(client)));
  }
  callbacks->OnSuccess(std::move(web_client));
  context_->client_callbacks.Remove(request_id);
}

void ServiceWorkerContextClient::OnOpenWindowError(
    int request_id,
    const std::string& message) {
  TRACE_EVENT0("ServiceWorker",
               "ServiceWorkerContextClient::OnOpenWindowError");
  blink::WebServiceWorkerClientCallbacks* callbacks =
      context_->client_callbacks.Lookup(request_id);
  if (!callbacks) {
    NOTREACHED() << "Got stray response: " << request_id;
    return;
  }
  callbacks->OnError(blink::WebServiceWorkerError(
      blink::mojom::ServiceWorkerErrorType::kNavigation,
      blink::WebString::FromUTF8(message)));
  context_->client_callbacks.Remove(request_id);
}

void ServiceWorkerContextClient::OnFocusClientResponse(
    int request_id, const ServiceWorkerClientInfo& client) {
  TRACE_EVENT0("ServiceWorker",
               "ServiceWorkerContextClient::OnFocusClientResponse");
  blink::WebServiceWorkerClientCallbacks* callback =
      context_->client_callbacks.Lookup(request_id);
  if (!callback) {
    NOTREACHED() << "Got stray response: " << request_id;
    return;
  }
  if (!client.IsEmpty()) {
    DCHECK(client.IsValid());
    std::unique_ptr<blink::WebServiceWorkerClientInfo> web_client(
        new blink::WebServiceWorkerClientInfo(
            ToWebServiceWorkerClientInfo(client)));
    callback->OnSuccess(std::move(web_client));
  } else {
    callback->OnError(blink::WebServiceWorkerError(
        blink::mojom::ServiceWorkerErrorType::kNotFound,
        "The WindowClient was not found."));
  }

  context_->client_callbacks.Remove(request_id);
}

void ServiceWorkerContextClient::OnNavigateClientResponse(
    int request_id,
    const ServiceWorkerClientInfo& client) {
  TRACE_EVENT0("ServiceWorker",
               "ServiceWorkerContextClient::OnNavigateClientResponse");
  blink::WebServiceWorkerClientCallbacks* callbacks =
      context_->client_callbacks.Lookup(request_id);
  if (!callbacks) {
    NOTREACHED() << "Got stray response: " << request_id;
    return;
  }
  std::unique_ptr<blink::WebServiceWorkerClientInfo> web_client;
  if (!client.IsEmpty()) {
    DCHECK(client.IsValid());
    web_client.reset(new blink::WebServiceWorkerClientInfo(
        ToWebServiceWorkerClientInfo(client)));
  }
  callbacks->OnSuccess(std::move(web_client));
  context_->client_callbacks.Remove(request_id);
}

void ServiceWorkerContextClient::OnNavigateClientError(int request_id,
                                                       const GURL& url) {
  TRACE_EVENT0("ServiceWorker",
               "ServiceWorkerContextClient::OnNavigateClientError");
  blink::WebServiceWorkerClientCallbacks* callbacks =
      context_->client_callbacks.Lookup(request_id);
  if (!callbacks) {
    NOTREACHED() << "Got stray response: " << request_id;
    return;
  }
  std::string message = "Cannot navigate to URL: " + url.spec();
  callbacks->OnError(blink::WebServiceWorkerError(
      blink::mojom::ServiceWorkerErrorType::kNavigation,
      blink::WebString::FromUTF8(message)));
  context_->client_callbacks.Remove(request_id);
}

void ServiceWorkerContextClient::OnDidSkipWaiting(int request_id) {
  TRACE_EVENT0("ServiceWorker",
               "ServiceWorkerContextClient::OnDidSkipWaiting");
  blink::WebServiceWorkerSkipWaitingCallbacks* callbacks =
      context_->skip_waiting_callbacks.Lookup(request_id);
  if (!callbacks) {
    NOTREACHED() << "Got stray response: " << request_id;
    return;
  }
  callbacks->OnSuccess();
  context_->skip_waiting_callbacks.Remove(request_id);
}

void ServiceWorkerContextClient::OnDidClaimClients(int request_id) {
  TRACE_EVENT0("ServiceWorker",
               "ServiceWorkerContextClient::OnDidClaimClients");
  blink::WebServiceWorkerClientsClaimCallbacks* callbacks =
      context_->claim_clients_callbacks.Lookup(request_id);
  if (!callbacks) {
    NOTREACHED() << "Got stray response: " << request_id;
    return;
  }
  callbacks->OnSuccess();
  context_->claim_clients_callbacks.Remove(request_id);
}

void ServiceWorkerContextClient::OnClaimClientsError(
    int request_id,
    blink::mojom::ServiceWorkerErrorType error_type,
    const base::string16& message) {
  TRACE_EVENT0("ServiceWorker",
               "ServiceWorkerContextClient::OnClaimClientsError");
  blink::WebServiceWorkerClientsClaimCallbacks* callbacks =
      context_->claim_clients_callbacks.Lookup(request_id);
  if (!callbacks) {
    NOTREACHED() << "Got stray response: " << request_id;
    return;
  }
  callbacks->OnError(blink::WebServiceWorkerError(
      error_type, blink::WebString::FromUTF16(message)));
  context_->claim_clients_callbacks.Remove(request_id);
}

void ServiceWorkerContextClient::Ping(PingCallback callback) {
  std::move(callback).Run();
}

void ServiceWorkerContextClient::OnNavigationPreloadResponse(
    int fetch_event_id,
    std::unique_ptr<blink::WebURLResponse> response,
    std::unique_ptr<blink::WebDataConsumerHandle> data_consumer_handle) {
  proxy_->OnNavigationPreloadResponse(fetch_event_id, std::move(response),
                                      std::move(data_consumer_handle));
}

void ServiceWorkerContextClient::OnNavigationPreloadError(
    int fetch_event_id,
    std::unique_ptr<blink::WebServiceWorkerError> error) {
  proxy_->OnNavigationPreloadError(fetch_event_id, std::move(error));
  context_->preload_requests.Remove(fetch_event_id);
}

void ServiceWorkerContextClient::OnNavigationPreloadComplete(
    int fetch_event_id,
    base::TimeTicks completion_time,
    int64_t encoded_data_length,
    int64_t encoded_body_length,
    int64_t decoded_body_length) {
  proxy_->OnNavigationPreloadComplete(
      fetch_event_id, (completion_time - base::TimeTicks()).InSecondsF(),
      encoded_data_length, encoded_body_length, decoded_body_length);
  context_->preload_requests.Remove(fetch_event_id);
}

base::WeakPtr<ServiceWorkerContextClient>
ServiceWorkerContextClient::GetWeakPtr() {
  DCHECK(worker_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(context_);
  return context_->weak_factory.GetWeakPtr();
}

}  // namespace content
