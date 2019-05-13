// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/service_worker/service_worker_context_client.h"

#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/debug/alias.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/sequenced_task_runner.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_checker.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "content/common/service_worker/service_worker_utils.h"
#include "content/public/common/content_features.h"
#include "content/public/common/network_service_util.h"
#include "content/public/common/referrer.h"
#include "content/public/renderer/content_renderer_client.h"
#include "content/public/renderer/document_state.h"
#include "content/public/renderer/worker_thread.h"
#include "content/renderer/loader/child_url_loader_factory_bundle.h"
#include "content/renderer/loader/tracked_child_url_loader_factory_bundle.h"
#include "content/renderer/loader/web_url_loader_impl.h"
#include "content/renderer/loader/web_url_request_util.h"
#include "content/renderer/render_thread_impl.h"
#include "content/renderer/renderer_blink_platform_impl.h"
#include "content/renderer/service_worker/controller_service_worker_impl.h"
#include "content/renderer/service_worker/embedded_worker_instance_client_impl.h"
#include "content/renderer/service_worker/navigation_preload_request.h"
#include "content/renderer/service_worker/service_worker_fetch_context_impl.h"
#include "content/renderer/service_worker/service_worker_network_provider_for_service_worker.h"
#include "content/renderer/service_worker/service_worker_timeout_timer.h"
#include "content/renderer/service_worker/service_worker_type_converters.h"
#include "content/renderer/service_worker/service_worker_type_util.h"
#include "net/base/net_errors.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/request_context_frame_type.mojom.h"
#include "storage/common/blob_storage/blob_handle.h"
#include "third_party/blink/public/common/messaging/message_port_channel.h"
#include "third_party/blink/public/common/service_worker/service_worker_status_code.h"
#include "third_party/blink/public/common/service_worker/service_worker_utils.h"
#include "third_party/blink/public/mojom/blob/blob.mojom.h"
#include "third_party/blink/public/mojom/blob/blob_registry.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_client.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_object.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_registration.mojom.h"
#include "third_party/blink/public/platform/interface_provider.h"
#include "third_party/blink/public/platform/modules/background_fetch/web_background_fetch_registration.h"
#include "third_party/blink/public/platform/modules/notifications/web_notification_data.h"
#include "third_party/blink/public/platform/modules/payments/web_payment_handler_response.h"
#include "third_party/blink/public/platform/modules/payments/web_payment_request_event_data.h"
#include "third_party/blink/public/platform/modules/service_worker/web_service_worker_clients_info.h"
#include "third_party/blink/public/platform/modules/service_worker/web_service_worker_error.h"
#include "third_party/blink/public/platform/modules/service_worker/web_service_worker_request.h"
#include "third_party/blink/public/platform/modules/service_worker/web_service_worker_response.h"
#include "third_party/blink/public/platform/notification_data_conversions.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/url_conversion.h"
#include "third_party/blink/public/platform/web_blob_registry.h"
#include "third_party/blink/public/platform/web_http_body.h"
#include "third_party/blink/public/platform/web_security_origin.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "third_party/blink/public/platform/web_url_response.h"
#include "third_party/blink/public/web/modules/service_worker/web_service_worker_context_client.h"
#include "third_party/blink/public/web/modules/service_worker/web_service_worker_context_proxy.h"

using blink::WebURLRequest;
using blink::MessagePortChannel;

namespace content {

namespace {

constexpr char kServiceWorkerContextClientScope[] =
    "ServiceWorkerContextClient";

class StreamHandleListener
    : public blink::WebServiceWorkerStreamHandle::Listener {
 public:
  StreamHandleListener(
      blink::mojom::ServiceWorkerStreamCallbackPtr callback_ptr,
      std::unique_ptr<ServiceWorkerTimeoutTimer::StayAwakeToken> token)
      : callback_ptr_(std::move(callback_ptr)), token_(std::move(token)) {}

  ~StreamHandleListener() override {}

  void OnAborted() override {
    callback_ptr_->OnAborted();
    token_.reset();
  }

  void OnCompleted() override {
    callback_ptr_->OnCompleted();
    token_.reset();
  }

 private:
  blink::mojom::ServiceWorkerStreamCallbackPtr callback_ptr_;
  std::unique_ptr<ServiceWorkerTimeoutTimer::StayAwakeToken> token_;
};

blink::WebServiceWorkerClientInfo ToWebServiceWorkerClientInfo(
    blink::mojom::ServiceWorkerClientInfoPtr client_info) {
  DCHECK(!client_info->client_uuid.empty());

  blink::WebServiceWorkerClientInfo web_client_info;

  web_client_info.uuid = blink::WebString::FromASCII(client_info->client_uuid);
  web_client_info.page_hidden = client_info->page_hidden;
  web_client_info.is_focused = client_info->is_focused;
  web_client_info.url = client_info->url;
  web_client_info.frame_type = client_info->frame_type;
  web_client_info.client_type = client_info->client_type;

  return web_client_info;
}

// Converts a blink::mojom::BackgroundFetchRegistrationPtr object to
// a blink::WebBackgroundFetchRegistration object.
blink::WebBackgroundFetchRegistration ToWebBackgroundFetchRegistration(
    blink::mojom::BackgroundFetchRegistrationPtr registration) {
  return blink::WebBackgroundFetchRegistration(
      blink::WebString::FromUTF8(registration->registration_data->developer_id),
      registration->registration_data->upload_total,
      registration->registration_data->uploaded,
      registration->registration_data->download_total,
      registration->registration_data->downloaded,
      registration->registration_data->result,
      registration->registration_data->failure_reason,
      mojo::ScopedMessagePipeHandle(
          registration->registration_interface.PassHandle()),
      registration->registration_interface.version());
}

// This is complementary to ConvertWebKitPriorityToNetPriority, defined in
// web_url_loader_impl.cc.
WebURLRequest::Priority ConvertNetPriorityToWebKitPriority(
    const net::RequestPriority priority) {
  switch (priority) {
    case net::HIGHEST:
      return WebURLRequest::Priority::kVeryHigh;
    case net::MEDIUM:
      return WebURLRequest::Priority::kHigh;
    case net::LOW:
      return WebURLRequest::Priority::kMedium;
    case net::LOWEST:
      return WebURLRequest::Priority::kLow;
    case net::IDLE:
      return WebURLRequest::Priority::kVeryLow;
    case net::THROTTLED:
      NOTREACHED();
      return WebURLRequest::Priority::kVeryLow;
  }

  NOTREACHED();
  return WebURLRequest::Priority::kVeryLow;
}

// Finds an event callback keyed by |event_id| from |map|, and runs the callback
// with |args|. Returns true if the callback was found and called, otherwise
// returns false.
template <typename MapType, class... Args>
bool RunEventCallback(MapType* map,
                      ServiceWorkerTimeoutTimer* timer,
                      int event_id,
                      Args... args) {
  auto iter = map->find(event_id);
  // The event may have been aborted.
  if (iter == map->end())
    return false;
  std::move(iter->second).Run(args...);
  map->erase(iter);
  timer->EndEvent(event_id);
  return true;
}

// Creates a callback which takes an |event_id| and |status|, which calls the
// given event's callback with the given status and removes it from |map|.
template <typename MapType, typename... Args>
ServiceWorkerTimeoutTimer::AbortCallback CreateAbortCallback(MapType* map,
                                                             Args... args) {
  return base::BindOnce(
      [](MapType* map, Args... args, int event_id,
         blink::mojom::ServiceWorkerEventStatus status) {
        auto iter = map->find(event_id);
        DCHECK(iter != map->end());
        std::move(iter->second).Run(status, std::forward<Args>(args)...);
        map->erase(iter);
      },
      map, std::forward<Args>(args)...);
}

}  // namespace

// Holds data that needs to be bound to the worker context on the
// worker thread.
struct ServiceWorkerContextClient::WorkerContextData {
  explicit WorkerContextData(ServiceWorkerContextClient* owner)
      : service_worker_binding(owner),
        weak_factory(owner),
        proxy_weak_factory(owner->proxy_) {}

  ~WorkerContextData() { DCHECK(thread_checker.CalledOnValidThread()); }

  mojo::Binding<blink::mojom::ServiceWorker> service_worker_binding;

  // Maps for inflight event callbacks.
  // These are mapped from an event id issued from ServiceWorkerTimeoutTimer to
  // the Mojo callback to notify the end of the event.
  std::map<int, DispatchInstallEventCallback> install_event_callbacks;
  std::map<int, DispatchActivateEventCallback> activate_event_callbacks;
  std::map<int, DispatchBackgroundFetchAbortEventCallback>
      background_fetch_abort_event_callbacks;
  std::map<int, DispatchBackgroundFetchClickEventCallback>
      background_fetch_click_event_callbacks;
  std::map<int, DispatchBackgroundFetchFailEventCallback>
      background_fetch_fail_event_callbacks;
  std::map<int, DispatchBackgroundFetchSuccessEventCallback>
      background_fetched_event_callbacks;
  std::map<int, DispatchSyncEventCallback> sync_event_callbacks;
  std::map<int, DispatchPeriodicSyncEventCallback>
      periodic_sync_event_callbacks;
  std::map<int, payments::mojom::PaymentHandlerResponseCallbackPtr>
      abort_payment_result_callbacks;
  std::map<int, DispatchCanMakePaymentEventCallback>
      abort_payment_event_callbacks;
  std::map<int, DispatchCanMakePaymentEventCallback>
      can_make_payment_event_callbacks;
  std::map<int, DispatchPaymentRequestEventCallback>
      payment_request_event_callbacks;
  std::map<int, DispatchNotificationClickEventCallback>
      notification_click_event_callbacks;
  std::map<int, DispatchNotificationCloseEventCallback>
      notification_close_event_callbacks;
  std::map<int, DispatchPushEventCallback> push_event_callbacks;
  std::map<int, DispatchFetchEventCallback> fetch_event_callbacks;
  std::map<int, DispatchCookieChangeEventCallback>
      cookie_change_event_callbacks;
  std::map<int, DispatchExtendableMessageEventCallback> message_event_callbacks;

  // Maps for response callbacks.
  // These are mapped from an event id to the Mojo interface pointer which is
  // passed from the relevant DispatchSomeEvent() method.
  std::map<int, payments::mojom::PaymentHandlerResponseCallbackPtr>
      can_make_payment_result_callbacks;
  std::map<int, payments::mojom::PaymentHandlerResponseCallbackPtr>
      payment_response_callbacks;
  std::map<int, blink::mojom::ServiceWorkerFetchResponseCallbackPtr>
      fetch_response_callbacks;

  // Inflight navigation preload requests.
  base::IDMap<std::unique_ptr<NavigationPreloadRequest>> preload_requests;

  // Timer triggered when the service worker considers it should be stopped or
  // an event should be aborted.
  std::unique_ptr<ServiceWorkerTimeoutTimer> timeout_timer;

  // |controller_impl| should be destroyed before |timeout_timer| since the
  // pipe needs to be disconnected before callbacks passed by
  // DispatchSomeEvent() get destructed, which may be stored in |timeout_timer|
  // as parameters of pending tasks added after a termination request.
  std::unique_ptr<ControllerServiceWorkerImpl> controller_impl;

  base::ThreadChecker thread_checker;
  base::WeakPtrFactory<ServiceWorkerContextClient> weak_factory;
  base::WeakPtrFactory<blink::WebServiceWorkerContextProxy> proxy_weak_factory;
};

ServiceWorkerContextClient::ServiceWorkerContextClient(
    int64_t service_worker_version_id,
    const GURL& service_worker_scope,
    const GURL& script_url,
    bool is_starting_installed_worker,
    blink::mojom::RendererPreferencesPtr renderer_preferences,
    blink::mojom::ServiceWorkerRequest service_worker_request,
    blink::mojom::ControllerServiceWorkerRequest controller_request,
    blink::mojom::EmbeddedWorkerInstanceHostAssociatedPtrInfo instance_host,
    blink::mojom::ServiceWorkerProviderInfoForStartWorkerPtr provider_info,
    EmbeddedWorkerInstanceClientImpl* owner,
    blink::mojom::EmbeddedWorkerStartTimingPtr start_timing,
    blink::mojom::RendererPreferenceWatcherRequest preference_watcher_request,
    std::unique_ptr<blink::URLLoaderFactoryBundleInfo> subresource_loaders,
    scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner)
    : service_worker_version_id_(service_worker_version_id),
      service_worker_scope_(service_worker_scope),
      script_url_(script_url),
      is_starting_installed_worker_(is_starting_installed_worker),
      renderer_preferences_(std::move(renderer_preferences)),
      preference_watcher_request_(std::move(preference_watcher_request)),
      main_thread_task_runner_(std::move(main_thread_task_runner)),
      proxy_(nullptr),
      pending_service_worker_request_(std::move(service_worker_request)),
      pending_controller_request_(std::move(controller_request)),
      owner_(owner),
      start_timing_(std::move(start_timing)) {
  DCHECK(main_thread_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(owner_);
  instance_host_ =
      blink::mojom::ThreadSafeEmbeddedWorkerInstanceHostAssociatedPtr::Create(
          std::move(instance_host), main_thread_task_runner_);

  if (subresource_loaders) {
    if (IsOutOfProcessNetworkService()) {
      // If the network service crashes, this worker self-terminates, so it can
      // be restarted later with a connection to the restarted network
      // service.
      // Note that the default factory is the network service factory. It's set
      // on the start worker sequence.
      network_service_connection_error_handler_holder_.Bind(
          std::move(subresource_loaders->default_factory_info()));
      network_service_connection_error_handler_holder_->Clone(
          mojo::MakeRequest(&subresource_loaders->default_factory_info()));
      network_service_connection_error_handler_holder_
          .set_connection_error_handler(base::BindOnce(
              &ServiceWorkerContextClient::StopWorkerOnMainThread,
              base::Unretained(this)));
    }

    loader_factories_ = base::MakeRefCounted<HostChildURLLoaderFactoryBundle>(
        main_thread_task_runner_);
    loader_factories_->Update(std::make_unique<ChildURLLoaderFactoryBundleInfo>(
        std::move(subresource_loaders)));
  }

  service_worker_provider_info_ = std::move(provider_info);

  TRACE_EVENT_NESTABLE_ASYNC_BEGIN1("ServiceWorker",
                                    "ServiceWorkerContextClient", this,
                                    "script_url", script_url_.spec());
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN1(
      "ServiceWorker", "LOAD_SCRIPT", this, "Source",
      (is_starting_installed_worker_ ? "InstalledScriptsManager"
                                     : "ResourceLoader"));
}

ServiceWorkerContextClient::~ServiceWorkerContextClient() {
  DCHECK(main_thread_task_runner_->RunsTasksInCurrentSequence());
}

void ServiceWorkerContextClient::StartWorkerContext(
    std::unique_ptr<blink::WebEmbeddedWorker> worker,
    const blink::WebEmbeddedWorkerStartData& start_data) {
  DCHECK(main_thread_task_runner_->RunsTasksInCurrentSequence());
  worker_ = std::move(worker);
  worker_->StartWorkerContext(start_data);
}

blink::WebEmbeddedWorker& ServiceWorkerContextClient::worker() {
  DCHECK(main_thread_task_runner_->RunsTasksInCurrentSequence());
  return *worker_;
}

void ServiceWorkerContextClient::UpdateSubresourceLoaderFactories(
    std::unique_ptr<blink::URLLoaderFactoryBundleInfo>
        subresource_loader_factories) {
  DCHECK(main_thread_task_runner_->RunsTasksInCurrentSequence());
  loader_factories_->UpdateThisAndAllClones(
      std::make_unique<ChildURLLoaderFactoryBundleInfo>(
          std::move(subresource_loader_factories)));
}

void ServiceWorkerContextClient::WorkerReadyForInspectionOnMainThread() {
  DCHECK(main_thread_task_runner_->RunsTasksInCurrentSequence());
  (*instance_host_)->OnReadyForInspection();
}

void ServiceWorkerContextClient::WorkerContextFailedToStartOnMainThread() {
  DCHECK(main_thread_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(!proxy_);

  (*instance_host_)->OnStopped();

  TRACE_EVENT_NESTABLE_ASYNC_END1("ServiceWorker", "ServiceWorkerContextClient",
                                  this, "Status",
                                  "WorkerContextFailedToStartOnMainThread");

  owner_->WorkerContextDestroyed();
}

void ServiceWorkerContextClient::FailedToLoadClassicScript() {
  DCHECK(worker_task_runner_->RunsTasksInCurrentSequence());
  TRACE_EVENT_NESTABLE_ASYNC_END1("ServiceWorker", "LOAD_SCRIPT", this,
                                  "Status", "FailedToLoadClassicScript");
  // Cleanly send an OnStopped() message instead of just breaking the
  // Mojo connection on termination, for consistency with the other
  // startup failure paths.
  (*instance_host_)->OnStopped();

  // The caller is responsible for terminating the thread which
  // eventually destroys |this|.
}

void ServiceWorkerContextClient::FailedToFetchModuleScript() {
  DCHECK(worker_task_runner_->RunsTasksInCurrentSequence());
  TRACE_EVENT_NESTABLE_ASYNC_END1("ServiceWorker", "LOAD_SCRIPT", this,
                                  "Status", "FailedToFetchModuleScript");
  // Cleanly send an OnStopped() message instead of just breaking the
  // Mojo connection on termination, for consistency with the other
  // startup failure paths.
  (*instance_host_)->OnStopped();

  // The caller is responsible for terminating the thread which
  // eventually destroys |this|.
}

void ServiceWorkerContextClient::WorkerScriptLoadedOnMainThread() {
  DCHECK(main_thread_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(!is_starting_installed_worker_);
  (*instance_host_)->OnScriptLoaded();
  TRACE_EVENT_NESTABLE_ASYNC_END0("ServiceWorker", "LOAD_SCRIPT", this);
}

void ServiceWorkerContextClient::WorkerScriptLoadedOnWorkerThread() {
  DCHECK(worker_task_runner_->RunsTasksInCurrentSequence());
  (*instance_host_)->OnScriptLoaded();
  TRACE_EVENT_NESTABLE_ASYNC_END0("ServiceWorker", "LOAD_SCRIPT", this);
}

void ServiceWorkerContextClient::WorkerContextStarted(
    blink::WebServiceWorkerContextProxy* proxy,
    scoped_refptr<base::SequencedTaskRunner> worker_task_runner) {
  DCHECK_NE(0, WorkerThread::GetCurrentId())
      << "service worker started on the main thread instead of a worker thread";
  DCHECK(worker_task_runner->RunsTasksInCurrentSequence());
  DCHECK(!worker_task_runner_);
  worker_task_runner_ = std::move(worker_task_runner);
  DCHECK(!proxy_);
  proxy_ = proxy;

  context_ = std::make_unique<WorkerContextData>(this);

  DCHECK(pending_service_worker_request_.is_pending());
  DCHECK(pending_controller_request_.is_pending());
  DCHECK(!context_->service_worker_binding.is_bound());
  DCHECK(!context_->controller_impl);
  context_->service_worker_binding.Bind(
      std::move(pending_service_worker_request_), worker_task_runner_);

  auto weak_ptr = GetWeakPtr();
  // Intentionally dereferences the weak ptr in order to
  // ensure the WeakPtrFactory is bound to this thread (the worker thread).
  weak_ptr = weak_ptr->GetWeakPtr();
  context_->controller_impl = std::make_unique<ControllerServiceWorkerImpl>(
      std::move(pending_controller_request_), std::move(weak_ptr),
      worker_task_runner_);

  // Create the idle timer. At this point the timer is not started. It will be
  // started after the WorkerStarted message is sent.
  context_->timeout_timer =
      std::make_unique<ServiceWorkerTimeoutTimer>(base::BindRepeating(
          &ServiceWorkerContextClient::OnIdleTimeout, base::Unretained(this)));
}

void ServiceWorkerContextClient::WillEvaluateScript() {
  DCHECK(worker_task_runner_->RunsTasksInCurrentSequence());
  start_timing_->script_evaluation_start_time = base::TimeTicks::Now();

  // Temporary CHECK for https://crbug.com/881100
  int64_t t0 =
      start_timing_->start_worker_received_time.since_origin().InMicroseconds();
  int64_t t1 = start_timing_->script_evaluation_start_time.since_origin()
                   .InMicroseconds();
  base::debug::Alias(&t0);
  base::debug::Alias(&t1);
  CHECK_LE(start_timing_->start_worker_received_time,
           start_timing_->script_evaluation_start_time);

  (*instance_host_)->OnScriptEvaluationStart();
}

void ServiceWorkerContextClient::DidEvaluateScript(bool success) {
  DCHECK(worker_task_runner_->RunsTasksInCurrentSequence());
  start_timing_->script_evaluation_end_time = base::TimeTicks::Now();

  // Temporary CHECK for https://crbug.com/881100
  int64_t t0 = start_timing_->script_evaluation_start_time.since_origin()
                   .InMicroseconds();
  int64_t t1 =
      start_timing_->script_evaluation_end_time.since_origin().InMicroseconds();
  base::debug::Alias(&t0);
  base::debug::Alias(&t1);
  CHECK_LE(start_timing_->script_evaluation_start_time,
           start_timing_->script_evaluation_end_time);

  blink::mojom::ServiceWorkerStartStatus status =
      success ? blink::mojom::ServiceWorkerStartStatus::kNormalCompletion
              : blink::mojom::ServiceWorkerStartStatus::kAbruptCompletion;

  // Schedule a task to send back WorkerStarted asynchronously, so we can be
  // sure that the worker is really started.
  // TODO(falken): Is this really needed? Probably if kNormalCompletion, the
  // worker is definitely running so we can SendStartWorker immediately.
  worker_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&ServiceWorkerContextClient::SendWorkerStarted,
                                GetWeakPtr(), status));
}

void ServiceWorkerContextClient::DidInitializeWorkerContext(
    v8::Local<v8::Context> context) {
  DCHECK(worker_task_runner_->RunsTasksInCurrentSequence());
  GetContentClient()
      ->renderer()
      ->DidInitializeServiceWorkerContextOnWorkerThread(
          context, service_worker_version_id_, service_worker_scope_,
          script_url_);
}

void ServiceWorkerContextClient::WillDestroyWorkerContext(
    v8::Local<v8::Context> context) {
  DCHECK(worker_task_runner_->RunsTasksInCurrentSequence());

  // At this point WillStopCurrentWorkerThread is already called, so
  // worker_task_runner_->RunsTasksInCurrentSequence() returns false
  // (while we're still on the worker thread).
  proxy_ = nullptr;

  blob_registry_.reset();

  // We have to clear callbacks now, as they need to be freed on the
  // same thread.
  context_.reset();

  GetContentClient()->renderer()->WillDestroyServiceWorkerContextOnWorkerThread(
      context, service_worker_version_id_, service_worker_scope_, script_url_);
}

void ServiceWorkerContextClient::WorkerContextDestroyed() {
  DCHECK(worker_task_runner_->RunsTasksInCurrentSequence());

  (*instance_host_)->OnStopped();

  // base::Unretained is safe because |owner_| does not destroy itself until
  // WorkerContextDestroyed is called.
  main_thread_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&EmbeddedWorkerInstanceClientImpl::WorkerContextDestroyed,
                     base::Unretained(owner_)));
}

void ServiceWorkerContextClient::CountFeature(
    blink::mojom::WebFeature feature) {
  (*instance_host_)->CountFeature(feature);
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
    blink::mojom::ConsoleMessageSource source,
    blink::mojom::ConsoleMessageLevel level,
    const blink::WebString& message,
    int line_number,
    const blink::WebString& source_url) {
  (*instance_host_)
      ->OnReportConsoleMessage(source, level, message.Utf16(), line_number,
                               blink::WebStringToGURL(source_url));
}

void ServiceWorkerContextClient::DidHandleActivateEvent(
    int request_id,
    blink::mojom::ServiceWorkerEventStatus status) {
  DCHECK(worker_task_runner_->RunsTasksInCurrentSequence());
  if (!context_)
    return;
  TRACE_EVENT_WITH_FLOW1("ServiceWorker",
                         "ServiceWorkerContextClient::DidHandleActivateEvent",
                         TRACE_ID_WITH_SCOPE(kServiceWorkerContextClientScope,
                                             TRACE_ID_LOCAL(request_id)),
                         TRACE_EVENT_FLAG_FLOW_IN, "status",
                         ServiceWorkerUtils::MojoEnumToString(status));
  RunEventCallback(&context_->activate_event_callbacks,
                   context_->timeout_timer.get(), request_id, status);
}

void ServiceWorkerContextClient::DidHandleBackgroundFetchAbortEvent(
    int request_id,
    blink::mojom::ServiceWorkerEventStatus status) {
  DCHECK(worker_task_runner_->RunsTasksInCurrentSequence());
  if (!context_)
    return;
  TRACE_EVENT_WITH_FLOW1(
      "ServiceWorker",
      "ServiceWorkerContextClient::DidHandleBackgroundFetchAbortEvent",
      TRACE_ID_WITH_SCOPE(kServiceWorkerContextClientScope,
                          TRACE_ID_LOCAL(request_id)),
      TRACE_EVENT_FLAG_FLOW_IN, "status",
      ServiceWorkerUtils::MojoEnumToString(status));
  RunEventCallback(&context_->background_fetch_abort_event_callbacks,
                   context_->timeout_timer.get(), request_id, status);
}

void ServiceWorkerContextClient::DidHandleBackgroundFetchClickEvent(
    int request_id,
    blink::mojom::ServiceWorkerEventStatus status) {
  DCHECK(worker_task_runner_->RunsTasksInCurrentSequence());
  if (!context_)
    return;
  TRACE_EVENT_WITH_FLOW1(
      "ServiceWorker",
      "ServiceWorkerContextClient::DidHandleBackgroundFetchClickEvent",
      TRACE_ID_WITH_SCOPE(kServiceWorkerContextClientScope,
                          TRACE_ID_LOCAL(request_id)),
      TRACE_EVENT_FLAG_FLOW_IN, "status",
      ServiceWorkerUtils::MojoEnumToString(status));
  RunEventCallback(&context_->background_fetch_click_event_callbacks,
                   context_->timeout_timer.get(), request_id, status);
}

void ServiceWorkerContextClient::DidHandleBackgroundFetchFailEvent(
    int request_id,
    blink::mojom::ServiceWorkerEventStatus status) {
  DCHECK(worker_task_runner_->RunsTasksInCurrentSequence());
  if (!context_)
    return;
  TRACE_EVENT_WITH_FLOW1(
      "ServiceWorker",
      "ServiceWorkerContextClient::DidHandleBackgroundFetchFailEvent",
      TRACE_ID_WITH_SCOPE(kServiceWorkerContextClientScope,
                          TRACE_ID_LOCAL(request_id)),
      TRACE_EVENT_FLAG_FLOW_IN, "status",
      ServiceWorkerUtils::MojoEnumToString(status));
  RunEventCallback(&context_->background_fetch_fail_event_callbacks,
                   context_->timeout_timer.get(), request_id, status);
}

void ServiceWorkerContextClient::DidHandleBackgroundFetchSuccessEvent(
    int request_id,
    blink::mojom::ServiceWorkerEventStatus status) {
  DCHECK(worker_task_runner_->RunsTasksInCurrentSequence());
  if (!context_)
    return;
  TRACE_EVENT_WITH_FLOW1(
      "ServiceWorker",
      "ServiceWorkerContextClient::DidHandleBackgroundFetchSuccessEvent",
      TRACE_ID_WITH_SCOPE(kServiceWorkerContextClientScope,
                          TRACE_ID_LOCAL(request_id)),
      TRACE_EVENT_FLAG_FLOW_IN, "status",
      ServiceWorkerUtils::MojoEnumToString(status));
  RunEventCallback(&context_->background_fetched_event_callbacks,
                   context_->timeout_timer.get(), request_id, status);
}

void ServiceWorkerContextClient::DidHandleCookieChangeEvent(
    int request_id,
    blink::mojom::ServiceWorkerEventStatus status) {
  DCHECK(worker_task_runner_->RunsTasksInCurrentSequence());
  if (!context_)
    return;
  TRACE_EVENT_WITH_FLOW1(
      "ServiceWorker", "ServiceWorkerContextClient::DidHandleCookieChangeEvent",
      TRACE_ID_WITH_SCOPE(kServiceWorkerContextClientScope,
                          TRACE_ID_LOCAL(request_id)),
      TRACE_EVENT_FLAG_FLOW_IN, "status",
      ServiceWorkerUtils::MojoEnumToString(status));
  RunEventCallback(&context_->cookie_change_event_callbacks,
                   context_->timeout_timer.get(), request_id, status);
}

void ServiceWorkerContextClient::DidHandleExtendableMessageEvent(
    int request_id,
    blink::mojom::ServiceWorkerEventStatus status) {
  DCHECK(worker_task_runner_->RunsTasksInCurrentSequence());
  if (!context_)
    return;
  TRACE_EVENT_WITH_FLOW1(
      "ServiceWorker",
      "ServiceWorkerContextClient::DidHandleExtendableMessageEvent",
      TRACE_ID_WITH_SCOPE(kServiceWorkerContextClientScope,
                          TRACE_ID_LOCAL(request_id)),
      TRACE_EVENT_FLAG_FLOW_IN, "status",
      ServiceWorkerUtils::MojoEnumToString(status));
  RunEventCallback(&context_->message_event_callbacks,
                   context_->timeout_timer.get(), request_id, status);
}

void ServiceWorkerContextClient::DidHandleInstallEvent(
    int event_id,
    blink::mojom::ServiceWorkerEventStatus status) {
  DCHECK(worker_task_runner_->RunsTasksInCurrentSequence());
  proxy_->SetFetchHandlerExistence(proxy_->HasFetchEventHandler()
                                       ? FetchHandlerExistence::EXISTS
                                       : FetchHandlerExistence::DOES_NOT_EXIST);
  if (!context_)
    return;
  TRACE_EVENT_WITH_FLOW1("ServiceWorker",
                         "ServiceWorkerContextClient::DidHandleInstallEvent",
                         TRACE_ID_WITH_SCOPE(kServiceWorkerContextClientScope,
                                             TRACE_ID_LOCAL(event_id)),
                         TRACE_EVENT_FLAG_FLOW_IN, "status",
                         ServiceWorkerUtils::MojoEnumToString(status));
  RunEventCallback(&context_->install_event_callbacks,
                   context_->timeout_timer.get(), event_id, status,
                   proxy_->HasFetchEventHandler());
}

void ServiceWorkerContextClient::RespondToFetchEventWithNoResponse(
    int fetch_event_id,
    base::TimeTicks event_dispatch_time,
    base::TimeTicks respond_with_settled_time) {
  DCHECK(worker_task_runner_->RunsTasksInCurrentSequence());
  if (!context_)
    return;
  TRACE_EVENT_WITH_FLOW0(
      "ServiceWorker",
      "ServiceWorkerContextClient::RespondToFetchEventWithNoResponse",
      TRACE_ID_WITH_SCOPE(kServiceWorkerContextClientScope,
                          TRACE_ID_LOCAL(fetch_event_id)),
      TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT);
  DCHECK(base::ContainsKey(context_->fetch_response_callbacks, fetch_event_id));
  const blink::mojom::ServiceWorkerFetchResponseCallbackPtr& response_callback =
      context_->fetch_response_callbacks[fetch_event_id];

  auto timing = blink::mojom::ServiceWorkerFetchEventTiming::New();
  timing->dispatch_event_time = event_dispatch_time;
  timing->respond_with_settled_time = respond_with_settled_time;

  response_callback->OnFallback(std::move(timing));
  context_->fetch_response_callbacks.erase(fetch_event_id);
}

void ServiceWorkerContextClient::RespondToFetchEvent(
    int fetch_event_id,
    const blink::WebServiceWorkerResponse& web_response,
    base::TimeTicks event_dispatch_time,
    base::TimeTicks respond_with_settled_time) {
  DCHECK(worker_task_runner_->RunsTasksInCurrentSequence());
  if (!context_)
    return;
  TRACE_EVENT_WITH_FLOW0(
      "ServiceWorker", "ServiceWorkerContextClient::RespondToFetchEvent",
      TRACE_ID_WITH_SCOPE(kServiceWorkerContextClientScope,
                          TRACE_ID_LOCAL(fetch_event_id)),
      TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT);
  DCHECK(base::ContainsKey(context_->fetch_response_callbacks, fetch_event_id));

  blink::mojom::FetchAPIResponsePtr response(
      GetFetchAPIResponseFromWebResponse(web_response));
  const blink::mojom::ServiceWorkerFetchResponseCallbackPtr& response_callback =
      context_->fetch_response_callbacks[fetch_event_id];

  auto timing = blink::mojom::ServiceWorkerFetchEventTiming::New();
  timing->dispatch_event_time = event_dispatch_time;
  timing->respond_with_settled_time = respond_with_settled_time;

  response_callback->OnResponse(std::move(response), std::move(timing));
  context_->fetch_response_callbacks.erase(fetch_event_id);
}

void ServiceWorkerContextClient::RespondToFetchEventWithResponseStream(
    int fetch_event_id,
    const blink::WebServiceWorkerResponse& web_response,
    blink::WebServiceWorkerStreamHandle* web_body_as_stream,
    base::TimeTicks event_dispatch_time,
    base::TimeTicks respond_with_settled_time) {
  DCHECK(worker_task_runner_->RunsTasksInCurrentSequence());
  if (!context_)
    return;
  TRACE_EVENT_WITH_FLOW0(
      "ServiceWorker",
      "ServiceWorkerContextClient::RespondToFetchEventWithResponseStream",
      TRACE_ID_WITH_SCOPE(kServiceWorkerContextClientScope,
                          TRACE_ID_LOCAL(fetch_event_id)),
      TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT);
  DCHECK(base::ContainsKey(context_->fetch_response_callbacks, fetch_event_id));
  blink::mojom::FetchAPIResponsePtr response(
      GetFetchAPIResponseFromWebResponse(web_response));
  const blink::mojom::ServiceWorkerFetchResponseCallbackPtr& response_callback =
      context_->fetch_response_callbacks[fetch_event_id];
  blink::mojom::ServiceWorkerStreamHandlePtr body_as_stream =
      blink::mojom::ServiceWorkerStreamHandle::New();
  blink::mojom::ServiceWorkerStreamCallbackPtr callback_ptr;
  body_as_stream->callback_request = mojo::MakeRequest(&callback_ptr);
  body_as_stream->stream = web_body_as_stream->DrainStreamDataPipe();
  DCHECK(body_as_stream->stream.is_valid());

  web_body_as_stream->SetListener(std::make_unique<StreamHandleListener>(
      std::move(callback_ptr),
      context_->timeout_timer->CreateStayAwakeToken()));

  auto timing = blink::mojom::ServiceWorkerFetchEventTiming::New();
  timing->dispatch_event_time = event_dispatch_time;
  timing->respond_with_settled_time = respond_with_settled_time;

  response_callback->OnResponseStream(
      std::move(response), std::move(body_as_stream), std::move(timing));
  context_->fetch_response_callbacks.erase(fetch_event_id);
}

void ServiceWorkerContextClient::DidHandleFetchEvent(
    int event_id,
    blink::mojom::ServiceWorkerEventStatus status) {
  DCHECK(worker_task_runner_->RunsTasksInCurrentSequence());
  if (!context_)
    return;
  // This TRACE_EVENT is used for perf benchmark to confirm if all of fetch
  // events have completed. (crbug.com/736697)
  TRACE_EVENT_WITH_FLOW1("ServiceWorker",
                         "ServiceWorkerContextClient::DidHandleFetchEvent",
                         TRACE_ID_WITH_SCOPE(kServiceWorkerContextClientScope,
                                             TRACE_ID_LOCAL(event_id)),
                         TRACE_EVENT_FLAG_FLOW_IN, "status",
                         ServiceWorkerUtils::MojoEnumToString(status));
  if (!RunEventCallback(&context_->fetch_event_callbacks,
                        context_->timeout_timer.get(), event_id, status)) {
    // The event may have been aborted. Its response callback also needs to be
    // deleted.
    context_->fetch_response_callbacks.erase(event_id);
  } else {
    // |fetch_response_callback| should be used before settling a promise for
    // waitUntil().
    DCHECK(!base::ContainsKey(context_->fetch_response_callbacks, event_id));
  }
}

void ServiceWorkerContextClient::DidHandleNotificationClickEvent(
    int request_id,
    blink::mojom::ServiceWorkerEventStatus status) {
  DCHECK(worker_task_runner_->RunsTasksInCurrentSequence());
  if (!context_)
    return;
  TRACE_EVENT_WITH_FLOW1(
      "ServiceWorker",
      "ServiceWorkerContextClient::DidHandleNotificationClickEvent",
      TRACE_ID_WITH_SCOPE(kServiceWorkerContextClientScope,
                          TRACE_ID_LOCAL(request_id)),
      TRACE_EVENT_FLAG_FLOW_IN, "status",
      ServiceWorkerUtils::MojoEnumToString(status));
  RunEventCallback(&context_->notification_click_event_callbacks,
                   context_->timeout_timer.get(), request_id, status);
}

void ServiceWorkerContextClient::DidHandleNotificationCloseEvent(
    int request_id,
    blink::mojom::ServiceWorkerEventStatus status) {
  DCHECK(worker_task_runner_->RunsTasksInCurrentSequence());
  if (!context_)
    return;
  TRACE_EVENT_WITH_FLOW1(
      "ServiceWorker",
      "ServiceWorkerContextClient::DidHandleNotificationCloseEvent",
      TRACE_ID_WITH_SCOPE(kServiceWorkerContextClientScope,
                          TRACE_ID_LOCAL(request_id)),
      TRACE_EVENT_FLAG_FLOW_IN, "status",
      ServiceWorkerUtils::MojoEnumToString(status));
  RunEventCallback(&context_->notification_close_event_callbacks,
                   context_->timeout_timer.get(), request_id, status);
}

void ServiceWorkerContextClient::DidHandlePushEvent(
    int request_id,
    blink::mojom::ServiceWorkerEventStatus status) {
  DCHECK(worker_task_runner_->RunsTasksInCurrentSequence());
  if (!context_)
    return;
  TRACE_EVENT_WITH_FLOW1("ServiceWorker",
                         "ServiceWorkerContextClient::DidHandlePushEvent",
                         TRACE_ID_WITH_SCOPE(kServiceWorkerContextClientScope,
                                             TRACE_ID_LOCAL(request_id)),
                         TRACE_EVENT_FLAG_FLOW_IN, "status",
                         ServiceWorkerUtils::MojoEnumToString(status));
  RunEventCallback(&context_->push_event_callbacks,
                   context_->timeout_timer.get(), request_id, status);
}

void ServiceWorkerContextClient::DidHandleSyncEvent(
    int request_id,
    blink::mojom::ServiceWorkerEventStatus status) {
  DCHECK(worker_task_runner_->RunsTasksInCurrentSequence());
  if (!context_)
    return;
  TRACE_EVENT_WITH_FLOW1("ServiceWorker",
                         "ServiceWorkerContextClient::DidHandleSyncEvent",
                         TRACE_ID_WITH_SCOPE(kServiceWorkerContextClientScope,
                                             TRACE_ID_LOCAL(request_id)),
                         TRACE_EVENT_FLAG_FLOW_IN, "status",
                         ServiceWorkerUtils::MojoEnumToString(status));
  RunEventCallback(&context_->sync_event_callbacks,
                   context_->timeout_timer.get(), request_id, status);
}

void ServiceWorkerContextClient::DidHandlePeriodicSyncEvent(
    int request_id,
    blink::mojom::ServiceWorkerEventStatus status) {
  DCHECK(worker_task_runner_->RunsTasksInCurrentSequence());
  if (!context_)
    return;
  TRACE_EVENT_WITH_FLOW1(
      "ServiceWorker", "ServiceWorkerContextClient::DidHandlePeriodicSyncEvent",
      TRACE_ID_WITH_SCOPE(kServiceWorkerContextClientScope,
                          TRACE_ID_LOCAL(request_id)),
      TRACE_EVENT_FLAG_FLOW_IN, "status",
      ServiceWorkerUtils::MojoEnumToString(status));
  RunEventCallback(&context_->periodic_sync_event_callbacks,
                   context_->timeout_timer.get(), request_id, status);
}

void ServiceWorkerContextClient::RespondToAbortPaymentEvent(
    int event_id,
    bool payment_aborted) {
  DCHECK(worker_task_runner_->RunsTasksInCurrentSequence());
  if (!context_)
    return;
  TRACE_EVENT_WITH_FLOW0(
      "ServiceWorker", "ServiceWorkerContextClient::RespondToAbortPaymentEvent",
      TRACE_ID_WITH_SCOPE(kServiceWorkerContextClientScope,
                          TRACE_ID_LOCAL(event_id)),
      TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT);
  DCHECK(base::ContainsKey(context_->abort_payment_result_callbacks, event_id));
  const payments::mojom::PaymentHandlerResponseCallbackPtr& result_callback =
      context_->abort_payment_result_callbacks[event_id];
  result_callback->OnResponseForAbortPayment(payment_aborted);
  context_->abort_payment_result_callbacks.erase(event_id);
}

void ServiceWorkerContextClient::DidHandleAbortPaymentEvent(
    int event_id,
    blink::mojom::ServiceWorkerEventStatus status) {
  DCHECK(worker_task_runner_->RunsTasksInCurrentSequence());
  if (!context_)
    return;
  TRACE_EVENT_WITH_FLOW1(
      "ServiceWorker", "ServiceWorkerContextClient::DidHandleAbortPaymentEvent",
      TRACE_ID_WITH_SCOPE(kServiceWorkerContextClientScope,
                          TRACE_ID_LOCAL(event_id)),
      TRACE_EVENT_FLAG_FLOW_IN, "status",
      ServiceWorkerUtils::MojoEnumToString(status));
  if (RunEventCallback(&context_->abort_payment_event_callbacks,
                       context_->timeout_timer.get(), event_id, status)) {
    context_->abort_payment_result_callbacks.erase(event_id);
  }
}

void ServiceWorkerContextClient::RespondToCanMakePaymentEvent(
    int event_id,
    bool can_make_payment) {
  DCHECK(worker_task_runner_->RunsTasksInCurrentSequence());
  if (!context_)
    return;
  TRACE_EVENT_WITH_FLOW0(
      "ServiceWorker",
      "ServiceWorkerContextClient::RespondToCanMakePaymentEvent",
      TRACE_ID_WITH_SCOPE(kServiceWorkerContextClientScope,
                          TRACE_ID_LOCAL(event_id)),
      TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT);
  DCHECK(
      base::ContainsKey(context_->can_make_payment_result_callbacks, event_id));
  const payments::mojom::PaymentHandlerResponseCallbackPtr& result_callback =
      context_->can_make_payment_result_callbacks[event_id];
  result_callback->OnResponseForCanMakePayment(can_make_payment);
  context_->can_make_payment_result_callbacks.erase(event_id);
}

void ServiceWorkerContextClient::DidHandleCanMakePaymentEvent(
    int event_id,
    blink::mojom::ServiceWorkerEventStatus status) {
  DCHECK(worker_task_runner_->RunsTasksInCurrentSequence());
  if (!context_)
    return;
  TRACE_EVENT_WITH_FLOW1(
      "ServiceWorker",
      "ServiceWorkerContextClient::DidHandleCanMakePaymentEvent",
      TRACE_ID_WITH_SCOPE(kServiceWorkerContextClientScope,
                          TRACE_ID_LOCAL(event_id)),
      TRACE_EVENT_FLAG_FLOW_IN, "status",
      ServiceWorkerUtils::MojoEnumToString(status));
  if (RunEventCallback(&context_->can_make_payment_event_callbacks,
                       context_->timeout_timer.get(), event_id, status)) {
    context_->can_make_payment_result_callbacks.erase(event_id);
  }
}

void ServiceWorkerContextClient::RespondToPaymentRequestEvent(
    int payment_request_id,
    const blink::WebPaymentHandlerResponse& web_response) {
  DCHECK(worker_task_runner_->RunsTasksInCurrentSequence());
  if (!context_)
    return;
  TRACE_EVENT_WITH_FLOW0(
      "ServiceWorker",
      "ServiceWorkerContextClient::RespondToPaymentRequestEvent",
      TRACE_ID_WITH_SCOPE(kServiceWorkerContextClientScope,
                          TRACE_ID_LOCAL(payment_request_id)),
      TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT);
  DCHECK(base::ContainsKey(context_->payment_response_callbacks,
                           payment_request_id));
  const payments::mojom::PaymentHandlerResponseCallbackPtr& response_callback =
      context_->payment_response_callbacks[payment_request_id];
  payments::mojom::PaymentHandlerResponsePtr response =
      payments::mojom::PaymentHandlerResponse::New();
  response->method_name = web_response.method_name.Utf8();
  response->stringified_details = web_response.stringified_details.Utf8();
  response_callback->OnResponseForPaymentRequest(std::move(response));
  context_->payment_response_callbacks.erase(payment_request_id);
}

void ServiceWorkerContextClient::DidHandlePaymentRequestEvent(
    int payment_request_id,
    blink::mojom::ServiceWorkerEventStatus status) {
  DCHECK(worker_task_runner_->RunsTasksInCurrentSequence());
  if (!context_)
    return;
  TRACE_EVENT_WITH_FLOW1(
      "ServiceWorker",
      "ServiceWorkerContextClient::DidHandlePaymentRequestEvent",
      TRACE_ID_WITH_SCOPE(kServiceWorkerContextClientScope,
                          TRACE_ID_LOCAL(payment_request_id)),
      TRACE_EVENT_FLAG_FLOW_IN, "status",
      ServiceWorkerUtils::MojoEnumToString(status));
  if (RunEventCallback(&context_->payment_request_event_callbacks,
                       context_->timeout_timer.get(), payment_request_id,
                       status)) {
    context_->payment_response_callbacks.erase(payment_request_id);
  }
}

std::unique_ptr<blink::WebServiceWorkerNetworkProvider>
ServiceWorkerContextClient::CreateServiceWorkerNetworkProviderOnMainThread() {
  DCHECK(main_thread_task_runner_->RunsTasksInCurrentSequence());
  return std::make_unique<ServiceWorkerNetworkProviderForServiceWorker>(
      std::move(service_worker_provider_info_->script_loader_factory_ptr_info));
}

scoped_refptr<blink::WebWorkerFetchContext>
ServiceWorkerContextClient::CreateServiceWorkerFetchContextOnMainThread(
    blink::WebServiceWorkerNetworkProvider* provider) {
  DCHECK(main_thread_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(preference_watcher_request_.is_pending());

  scoped_refptr<ChildURLLoaderFactoryBundle> url_loader_factory_bundle;
  if (loader_factories_) {
    url_loader_factory_bundle = loader_factories_;
  } else {
    url_loader_factory_bundle = RenderThreadImpl::current()
                                    ->blink_platform_impl()
                                    ->CreateDefaultURLLoaderFactoryBundle();
  }
  DCHECK(url_loader_factory_bundle);

  // TODO(crbug.com/796425): Temporarily wrap the raw
  // mojom::URLLoaderFactory pointer into SharedURLLoaderFactory.
  std::unique_ptr<network::SharedURLLoaderFactoryInfo>
      script_loader_factory_info =
          base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
              static_cast<ServiceWorkerNetworkProviderForServiceWorker*>(
                  provider)
                  ->script_loader_factory())
              ->Clone();

  return base::MakeRefCounted<ServiceWorkerFetchContextImpl>(
      *renderer_preferences_, script_url_, url_loader_factory_bundle->Clone(),
      std::move(script_loader_factory_info),
      GetContentClient()->renderer()->CreateURLLoaderThrottleProvider(
          URLLoaderThrottleProviderType::kWorker),
      GetContentClient()
          ->renderer()
          ->CreateWebSocketHandshakeThrottleProvider(),
      std::move(preference_watcher_request_));
}

int ServiceWorkerContextClient::WillStartTask() {
  DCHECK(worker_task_runner_->RunsTasksInCurrentSequence());
  // This is called from the Blink's code running in the worker thread and we
  // expect |context_| to still be alive.
  DCHECK(context_);
  DCHECK(context_->timeout_timer);
  return context_->timeout_timer->StartEvent(base::DoNothing());
}

void ServiceWorkerContextClient::DidEndTask(int task_id) {
  DCHECK(worker_task_runner_->RunsTasksInCurrentSequence());
  // This is called from the Blink's code running in the worker thread and we
  // expect |context_| to still be alive.
  DCHECK(context_);
  DCHECK(context_->timeout_timer);
  // Check if the task is still alive, since the timeout timer might have
  // already timed it out (which calls the abort callback passed to StartEvent()
  // but that does nothing, since we just check HasEvent() here instead of
  // maintaining our own set of started events).
  if (context_->timeout_timer->HasEvent(task_id))
    context_->timeout_timer->EndEvent(task_id);
}

void ServiceWorkerContextClient::DispatchOrQueueFetchEvent(
    blink::mojom::DispatchFetchEventParamsPtr params,
    blink::mojom::ServiceWorkerFetchResponseCallbackPtr response_callback,
    DispatchFetchEventCallback callback) {
  DCHECK(worker_task_runner_->RunsTasksInCurrentSequence());
  // |context_| is valid because the Mojo binding is on |worker_task_runner_|.
  DCHECK(context_);
  TRACE_EVENT2("ServiceWorker",
               "ServiceWorkerContextClient::DispatchOrQueueFetchEvent", "url",
               params->request->url.spec(), "queued",
               RequestedTermination() ? "true" : "false");
  if (RequestedTermination()) {
    context_->timeout_timer->PushPendingTask(base::BindOnce(
        &ServiceWorkerContextClient::DispatchFetchEvent, GetWeakPtr(),
        std::move(params), std::move(response_callback), std::move(callback)));
    return;
  }
  DispatchFetchEvent(std::move(params), std::move(response_callback),
                     std::move(callback));
}

void ServiceWorkerContextClient::DispatchSyncEvent(
    const std::string& tag,
    bool last_chance,
    base::TimeDelta timeout,
    DispatchSyncEventCallback callback) {
  DCHECK(worker_task_runner_->RunsTasksInCurrentSequence());
  // |context_| is valid because the Mojo binding is on |worker_task_runner_|.
  DCHECK(context_);
  int request_id = context_->timeout_timer->StartEventWithCustomTimeout(
      CreateAbortCallback(&context_->sync_event_callbacks), timeout);
  context_->sync_event_callbacks.emplace(request_id, std::move(callback));
  TRACE_EVENT_WITH_FLOW0("ServiceWorker",
                         "ServiceWorkerContextClient::DispatchSyncEvent",
                         TRACE_ID_WITH_SCOPE(kServiceWorkerContextClientScope,
                                             TRACE_ID_LOCAL(request_id)),
                         TRACE_EVENT_FLAG_FLOW_OUT);

  // TODO(jkarlin): Make this blink::WebString::FromUTF8Lenient once
  // https://crrev.com/1768063002/ lands.
  proxy_->DispatchSyncEvent(request_id, blink::WebString::FromUTF8(tag),
                            last_chance);
}

void ServiceWorkerContextClient::DispatchPeriodicSyncEvent(
    const std::string& tag,
    base::TimeDelta timeout,
    DispatchSyncEventCallback callback) {
  DCHECK(worker_task_runner_->RunsTasksInCurrentSequence());
  // |context_| is valid because the Mojo binding is on |worker_task_runner_|.
  DCHECK(context_);

  int request_id = context_->timeout_timer->StartEventWithCustomTimeout(
      CreateAbortCallback(&context_->periodic_sync_event_callbacks), timeout);
  context_->periodic_sync_event_callbacks.emplace(request_id,
                                                  std::move(callback));
  TRACE_EVENT_WITH_FLOW0(
      "ServiceWorker", "ServiceWorkerContextClient::DispatchPeriodicSyncEvent",
      TRACE_ID_WITH_SCOPE(kServiceWorkerContextClientScope,
                          TRACE_ID_LOCAL(request_id)),
      TRACE_EVENT_FLAG_FLOW_OUT);

  proxy_->DispatchPeriodicSyncEvent(request_id,
                                    blink::WebString::FromUTF8(tag));
}

void ServiceWorkerContextClient::DispatchAbortPaymentEvent(
    payments::mojom::PaymentHandlerResponseCallbackPtr response_callback,
    DispatchAbortPaymentEventCallback callback) {
  DCHECK(worker_task_runner_->RunsTasksInCurrentSequence());
  // Valid because the Mojo binding is on |worker_task_runner_|.
  DCHECK(context_);
  int event_id = context_->timeout_timer->StartEvent(
      CreateAbortCallback(&context_->abort_payment_event_callbacks));
  context_->abort_payment_event_callbacks.emplace(event_id,
                                                  std::move(callback));
  context_->abort_payment_result_callbacks.emplace(
      event_id, std::move(response_callback));
  TRACE_EVENT_WITH_FLOW0(
      "ServiceWorker", "ServiceWorkerContextClient::DispatchAbortPaymentEvent",
      TRACE_ID_WITH_SCOPE(kServiceWorkerContextClientScope,
                          TRACE_ID_LOCAL(event_id)),
      TRACE_EVENT_FLAG_FLOW_OUT);
  proxy_->DispatchAbortPaymentEvent(event_id);
}

void ServiceWorkerContextClient::DispatchCanMakePaymentEvent(
    payments::mojom::CanMakePaymentEventDataPtr eventData,
    payments::mojom::PaymentHandlerResponseCallbackPtr response_callback,
    DispatchCanMakePaymentEventCallback callback) {
  DCHECK(worker_task_runner_->RunsTasksInCurrentSequence());
  // Valid because the Mojo binding is on |worker_task_runner_|.
  DCHECK(context_);
  int event_id = context_->timeout_timer->StartEvent(
      CreateAbortCallback(&context_->can_make_payment_event_callbacks));
  context_->can_make_payment_event_callbacks.emplace(event_id,
                                                     std::move(callback));
  context_->can_make_payment_result_callbacks.emplace(
      event_id, std::move(response_callback));
  TRACE_EVENT_WITH_FLOW0(
      "ServiceWorker",
      "ServiceWorkerContextClient::DispatchCanMakePaymentEvent",
      TRACE_ID_WITH_SCOPE(kServiceWorkerContextClientScope,
                          TRACE_ID_LOCAL(event_id)),
      TRACE_EVENT_FLAG_FLOW_OUT);

  blink::WebCanMakePaymentEventData webEventData =
      mojo::ConvertTo<blink::WebCanMakePaymentEventData>(std::move(eventData));
  proxy_->DispatchCanMakePaymentEvent(event_id, webEventData);
}

void ServiceWorkerContextClient::DispatchPaymentRequestEvent(
    payments::mojom::PaymentRequestEventDataPtr eventData,
    payments::mojom::PaymentHandlerResponseCallbackPtr response_callback,
    DispatchPaymentRequestEventCallback callback) {
  DCHECK(worker_task_runner_->RunsTasksInCurrentSequence());
  // Valid because the Mojo binding is on |worker_task_runner_|.
  DCHECK(context_);
  int event_id = context_->timeout_timer->StartEvent(
      CreateAbortCallback(&context_->payment_request_event_callbacks));
  context_->payment_request_event_callbacks.emplace(event_id,
                                                    std::move(callback));
  context_->payment_response_callbacks.emplace(event_id,
                                               std::move(response_callback));
  TRACE_EVENT_WITH_FLOW0(
      "ServiceWorker",
      "ServiceWorkerContextClient::DispatchPaymentRequestEvent",
      TRACE_ID_WITH_SCOPE(kServiceWorkerContextClientScope,
                          TRACE_ID_LOCAL(event_id)),
      TRACE_EVENT_FLAG_FLOW_OUT);

  auto webEventData =
      mojo::ConvertTo<std::unique_ptr<blink::WebPaymentRequestEventData>>(
          std::move(eventData));
  proxy_->DispatchPaymentRequestEvent(event_id, std::move(webEventData));
}

void ServiceWorkerContextClient::OnNavigationPreloadResponse(
    int fetch_event_id,
    std::unique_ptr<blink::WebURLResponse> response,
    mojo::ScopedDataPipeConsumerHandle data_pipe) {
  DCHECK(worker_task_runner_->RunsTasksInCurrentSequence());
  TRACE_EVENT_WITH_FLOW0(
      "ServiceWorker",
      "ServiceWorkerContextClient::OnNavigationPreloadResponse",
      TRACE_ID_WITH_SCOPE(kServiceWorkerContextClientScope,
                          TRACE_ID_LOCAL(fetch_event_id)),
      TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT);
  proxy_->OnNavigationPreloadResponse(fetch_event_id, std::move(response),
                                      std::move(data_pipe));
}

void ServiceWorkerContextClient::OnNavigationPreloadError(
    int fetch_event_id,
    std::unique_ptr<blink::WebServiceWorkerError> error) {
  DCHECK(worker_task_runner_->RunsTasksInCurrentSequence());
  // |context_| owns NavigationPreloadRequest which calls this.
  DCHECK(context_);
  TRACE_EVENT_WITH_FLOW0("ServiceWorker",
                         "ServiceWorkerContextClient::OnNavigationPreloadError",
                         TRACE_ID_WITH_SCOPE(kServiceWorkerContextClientScope,
                                             TRACE_ID_LOCAL(fetch_event_id)),
                         TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT);
  proxy_->OnNavigationPreloadError(fetch_event_id, std::move(error));
  context_->preload_requests.Remove(fetch_event_id);
}

void ServiceWorkerContextClient::OnNavigationPreloadComplete(
    int fetch_event_id,
    base::TimeTicks completion_time,
    int64_t encoded_data_length,
    int64_t encoded_body_length,
    int64_t decoded_body_length) {
  DCHECK(worker_task_runner_->RunsTasksInCurrentSequence());
  // |context_| owns NavigationPreloadRequest which calls this.
  DCHECK(context_);
  TRACE_EVENT_WITH_FLOW0(
      "ServiceWorker",
      "ServiceWorkerContextClient::OnNavigationPreloadComplete",
      TRACE_ID_WITH_SCOPE(kServiceWorkerContextClientScope,
                          TRACE_ID_LOCAL(fetch_event_id)),
      TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT);
  proxy_->OnNavigationPreloadComplete(fetch_event_id, completion_time,
                                      encoded_data_length, encoded_body_length,
                                      decoded_body_length);
  context_->preload_requests.Remove(fetch_event_id);
}

void ServiceWorkerContextClient::ToWebServiceWorkerRequestForFetchEvent(
    blink::mojom::FetchAPIRequestPtr request,
    const std::string& client_id,
    blink::WebServiceWorkerRequest* web_request) {
  DCHECK(web_request);
  web_request->SetURL(blink::WebURL(request->url));
  web_request->SetMethod(blink::WebString::FromUTF8(request->method));
  for (const auto& pair : request->headers) {
    if (!GetContentClient()
             ->renderer()
             ->IsExcludedHeaderForServiceWorkerFetchEvent(pair.first)) {
      web_request->SetHeader(blink::WebString::FromUTF8(pair.first),
                             blink::WebString::FromUTF8(pair.second));
    }
  }

  // The body is provided in |request->body|.
  DCHECK(!request->blob);
  if (request->body) {
    std::vector<blink::mojom::BlobPtrInfo> blob_ptrs;
    if (!base::FeatureList::IsEnabled(network::features::kNetworkService)) {
      // We need this as GetBlobFromUUID is a sync IPC.
      // TODO(kinuko): Remove the friend for ScopedAllowBaseSyncPrimitives
      // in //base as well when we remove this code.
      base::ScopedAllowBaseSyncPrimitives allow_sync_primitives;
      blob_ptrs = GetBlobPtrsForRequestBody(*request->body);
    }
    blink::WebHTTPBody body = GetWebHTTPBodyForRequestBodyWithBlobPtrs(
        *request->body, std::move(blob_ptrs));
    body.SetUniqueBoundary();
    web_request->SetBody(body);
  }

  if (request->referrer) {
    web_request->SetReferrer(
        blink::WebString::FromUTF8(request->referrer->url.spec()),
        request->referrer->policy);
  }
  web_request->SetMode(request->mode);
  web_request->SetIsMainResourceLoad(request->is_main_resource_load);
  web_request->SetCredentialsMode(request->credentials_mode);
  web_request->SetCacheMode(request->cache_mode);
  web_request->SetRedirectMode(request->redirect_mode);
  web_request->SetRequestContext(request->request_context_type);
  web_request->SetFrameType(request->frame_type);
  web_request->SetClientId(blink::WebString::FromUTF8(client_id));
  web_request->SetIsReload(request->is_reload);
  if (request->integrity) {
    web_request->SetIntegrity(blink::WebString::FromUTF8(*request->integrity));
  }
  web_request->SetPriority(
      ConvertNetPriorityToWebKitPriority(request->priority));
  web_request->SetKeepalive(request->keepalive);
  web_request->SetIsHistoryNavigation(request->is_history_navigation);
  if (request->fetch_window_id)
    web_request->SetWindowId(*request->fetch_window_id);
}

void ServiceWorkerContextClient::SendWorkerStarted(
    blink::mojom::ServiceWorkerStartStatus status) {
  DCHECK(worker_task_runner_->RunsTasksInCurrentSequence());
  // |context_| is valid because this task was posted to |worker_task_runner_|.
  DCHECK(context_);

  if (GetContentClient()->renderer()) {  // nullptr in unit_tests.
    GetContentClient()->renderer()->DidStartServiceWorkerContextOnWorkerThread(
        service_worker_version_id_, service_worker_scope_, script_url_);
  }

  // Temporary DCHECK for https://crbug.com/881100
  int64_t t0 =
      start_timing_->start_worker_received_time.since_origin().InMicroseconds();
  int64_t t1 = start_timing_->script_evaluation_start_time.since_origin()
                   .InMicroseconds();
  int64_t t2 =
      start_timing_->script_evaluation_end_time.since_origin().InMicroseconds();
  base::debug::Alias(&t0);
  base::debug::Alias(&t1);
  base::debug::Alias(&t2);
  CHECK_LE(start_timing_->start_worker_received_time,
           start_timing_->script_evaluation_start_time);
  CHECK_LE(start_timing_->script_evaluation_start_time,
           start_timing_->script_evaluation_end_time);

  (*instance_host_)
      ->OnStarted(status, WorkerThread::GetCurrentId(),
                  std::move(start_timing_));

  context_->timeout_timer->Start();
  TRACE_EVENT_NESTABLE_ASYNC_END0("ServiceWorker", "ServiceWorkerContextClient",
                                  this);
}

void ServiceWorkerContextClient::DispatchActivateEvent(
    DispatchActivateEventCallback callback) {
  DCHECK(worker_task_runner_->RunsTasksInCurrentSequence());
  // |context_| is valid because the Mojo binding is on |worker_task_runner_|.
  DCHECK(context_);
  int request_id = context_->timeout_timer->StartEvent(
      CreateAbortCallback(&context_->activate_event_callbacks));
  context_->activate_event_callbacks.emplace(request_id, std::move(callback));
  TRACE_EVENT_WITH_FLOW0("ServiceWorker",
                         "ServiceWorkerContextClient::DispatchActivateEvent",
                         TRACE_ID_WITH_SCOPE(kServiceWorkerContextClientScope,
                                             TRACE_ID_LOCAL(request_id)),
                         TRACE_EVENT_FLAG_FLOW_OUT);
  proxy_->DispatchActivateEvent(request_id);
}

void ServiceWorkerContextClient::DispatchBackgroundFetchAbortEvent(
    blink::mojom::BackgroundFetchRegistrationPtr registration,
    DispatchBackgroundFetchAbortEventCallback callback) {
  DCHECK(worker_task_runner_->RunsTasksInCurrentSequence());
  // |context_| is valid because the Mojo binding is on |worker_task_runner_|.
  DCHECK(context_);
  int request_id = context_->timeout_timer->StartEvent(
      CreateAbortCallback(&context_->background_fetch_abort_event_callbacks));
  context_->background_fetch_abort_event_callbacks.emplace(request_id,
                                                           std::move(callback));
  TRACE_EVENT_WITH_FLOW0(
      "ServiceWorker",
      "ServiceWorkerContextClient::DispatchBackgroundFetchAbortEvent",
      TRACE_ID_WITH_SCOPE(kServiceWorkerContextClientScope,
                          TRACE_ID_LOCAL(request_id)),
      TRACE_EVENT_FLAG_FLOW_OUT);

  proxy_->DispatchBackgroundFetchAbortEvent(
      request_id, ToWebBackgroundFetchRegistration(std::move(registration)));
}

void ServiceWorkerContextClient::DispatchBackgroundFetchClickEvent(
    blink::mojom::BackgroundFetchRegistrationPtr registration,
    DispatchBackgroundFetchClickEventCallback callback) {
  DCHECK(worker_task_runner_->RunsTasksInCurrentSequence());
  // |context_| is valid because the Mojo binding is on |worker_task_runner_|.
  DCHECK(context_);
  int request_id = context_->timeout_timer->StartEvent(
      CreateAbortCallback(&context_->background_fetch_click_event_callbacks));
  context_->background_fetch_click_event_callbacks.emplace(request_id,
                                                           std::move(callback));
  TRACE_EVENT_WITH_FLOW0(
      "ServiceWorker",
      "ServiceWorkerContextClient::DispatchBackgroundFetchClickEvent",
      TRACE_ID_WITH_SCOPE(kServiceWorkerContextClientScope,
                          TRACE_ID_LOCAL(request_id)),
      TRACE_EVENT_FLAG_FLOW_OUT);

  proxy_->DispatchBackgroundFetchClickEvent(
      request_id, ToWebBackgroundFetchRegistration(std::move(registration)));
}

void ServiceWorkerContextClient::DispatchBackgroundFetchFailEvent(
    blink::mojom::BackgroundFetchRegistrationPtr registration,
    DispatchBackgroundFetchFailEventCallback callback) {
  DCHECK(worker_task_runner_->RunsTasksInCurrentSequence());
  // |context_| is valid because the Mojo binding is on |worker_task_runner_|.
  DCHECK(context_);
  int request_id = context_->timeout_timer->StartEvent(
      CreateAbortCallback(&context_->background_fetch_fail_event_callbacks));
  context_->background_fetch_fail_event_callbacks.emplace(request_id,
                                                          std::move(callback));
  TRACE_EVENT_WITH_FLOW0(
      "ServiceWorker",
      "ServiceWorkerContextClient::DispatchBackgroundFetchFailEvent",
      TRACE_ID_WITH_SCOPE(kServiceWorkerContextClientScope,
                          TRACE_ID_LOCAL(request_id)),
      TRACE_EVENT_FLAG_FLOW_OUT);

  proxy_->DispatchBackgroundFetchFailEvent(
      request_id, ToWebBackgroundFetchRegistration(std::move(registration)));
}

void ServiceWorkerContextClient::DispatchBackgroundFetchSuccessEvent(
    blink::mojom::BackgroundFetchRegistrationPtr registration,
    DispatchBackgroundFetchSuccessEventCallback callback) {
  DCHECK(worker_task_runner_->RunsTasksInCurrentSequence());
  // |context_| is valid because the Mojo binding is on |worker_task_runner_|.
  DCHECK(context_);
  int request_id = context_->timeout_timer->StartEvent(
      CreateAbortCallback(&context_->background_fetched_event_callbacks));
  context_->background_fetched_event_callbacks.emplace(request_id,
                                                       std::move(callback));
  TRACE_EVENT_WITH_FLOW0(
      "ServiceWorker",
      "ServiceWorkerContextClient::DispatchBackgroundFetchSuccessEvent",
      TRACE_ID_WITH_SCOPE(kServiceWorkerContextClientScope,
                          TRACE_ID_LOCAL(request_id)),
      TRACE_EVENT_FLAG_FLOW_OUT);

  proxy_->DispatchBackgroundFetchSuccessEvent(
      request_id, ToWebBackgroundFetchRegistration(std::move(registration)));
}

void ServiceWorkerContextClient::InitializeGlobalScope(
    blink::mojom::ServiceWorkerHostAssociatedPtrInfo service_worker_host,
    blink::mojom::ServiceWorkerRegistrationObjectInfoPtr registration_info,
    FetchHandlerExistence fetch_handler_existence) {
  DCHECK(worker_task_runner_->RunsTasksInCurrentSequence());
  // Connect to the blink::mojom::ServiceWorkerHost.
  proxy_->BindServiceWorkerHost(service_worker_host.PassHandle());
  // Set ServiceWorkerGlobalScope#registration.
  DCHECK_NE(registration_info->registration_id,
            blink::mojom::kInvalidServiceWorkerRegistrationId);
  DCHECK(registration_info->host_ptr_info.is_valid());
  DCHECK(registration_info->request.is_pending());
  proxy_->SetRegistration(
      registration_info.To<blink::WebServiceWorkerRegistrationObjectInfo>());
  proxy_->SetFetchHandlerExistence(fetch_handler_existence);

  proxy_->ReadyToEvaluateScript();
}

void ServiceWorkerContextClient::DispatchInstallEvent(
    DispatchInstallEventCallback callback) {
  DCHECK(worker_task_runner_->RunsTasksInCurrentSequence());
  // |context_| is valid because the Mojo binding is on |worker_task_runner_|.
  DCHECK(context_);
  int event_id = context_->timeout_timer->StartEvent(CreateAbortCallback(
      &context_->install_event_callbacks, false /* has_fetch_handler */));
  context_->install_event_callbacks.emplace(event_id, std::move(callback));
  TRACE_EVENT_WITH_FLOW0("ServiceWorker",
                         "ServiceWorkerContextClient::DispatchInstallEvent",
                         TRACE_ID_WITH_SCOPE(kServiceWorkerContextClientScope,
                                             TRACE_ID_LOCAL(event_id)),
                         TRACE_EVENT_FLAG_FLOW_OUT);

  proxy_->DispatchInstallEvent(event_id);
}

void ServiceWorkerContextClient::DispatchExtendableMessageEvent(
    blink::mojom::ExtendableMessageEventPtr event,
    DispatchExtendableMessageEventCallback callback) {
  DCHECK(worker_task_runner_->RunsTasksInCurrentSequence());
  // |context_| is valid because the Mojo binding is on |worker_task_runner_|.
  DCHECK(context_);
  int request_id = context_->timeout_timer->StartEvent(
      CreateAbortCallback(&context_->message_event_callbacks));
  context_->message_event_callbacks.emplace(request_id, std::move(callback));
  TRACE_EVENT_WITH_FLOW0(
      "ServiceWorker",
      "ServiceWorkerContextClient::DispatchExtendableMessageEvent",
      TRACE_ID_WITH_SCOPE(kServiceWorkerContextClientScope,
                          TRACE_ID_LOCAL(request_id)),
      TRACE_EVENT_FLAG_FLOW_OUT);

  if (event->source_info_for_client) {
    blink::WebServiceWorkerClientInfo web_client =
        ToWebServiceWorkerClientInfo(std::move(event->source_info_for_client));
    proxy_->DispatchExtendableMessageEvent(request_id,
                                           std::move(event->message),
                                           event->source_origin, web_client);
    return;
  }

  DCHECK_NE(event->source_info_for_service_worker->version_id,
            blink::mojom::kInvalidServiceWorkerVersionId);
  proxy_->DispatchExtendableMessageEvent(
      request_id, std::move(event->message), event->source_origin,
      event->source_info_for_service_worker
          .To<blink::WebServiceWorkerObjectInfo>());
}

void ServiceWorkerContextClient::
    DispatchExtendableMessageEventWithCustomTimeout(
        blink::mojom::ExtendableMessageEventPtr event,
        base::TimeDelta timeout,
        DispatchExtendableMessageEventCallback callback) {
  DCHECK(worker_task_runner_->RunsTasksInCurrentSequence());
  // |context_| is valid because the Mojo binding is on |worker_task_runner_|.
  DCHECK(context_);
  int request_id = context_->timeout_timer->StartEventWithCustomTimeout(
      CreateAbortCallback(&context_->message_event_callbacks), timeout);

  context_->message_event_callbacks.emplace(request_id, std::move(callback));
  TRACE_EVENT1("ServiceWorker",
               "ServiceWorkerContextClient::"
               "DispatchExtendableMessageEventWithCustomTimeout",
               "request_id", request_id);

  if (event->source_info_for_client) {
    blink::WebServiceWorkerClientInfo web_client =
        ToWebServiceWorkerClientInfo(std::move(event->source_info_for_client));
    proxy_->DispatchExtendableMessageEvent(request_id,
                                           std::move(event->message),
                                           event->source_origin, web_client);
    return;
  }

  DCHECK_NE(event->source_info_for_service_worker->version_id,
            blink::mojom::kInvalidServiceWorkerVersionId);
  proxy_->DispatchExtendableMessageEvent(
      request_id, std::move(event->message), event->source_origin,
      event->source_info_for_service_worker
          .To<blink::WebServiceWorkerObjectInfo>());
}

void ServiceWorkerContextClient::DispatchFetchEvent(
    blink::mojom::DispatchFetchEventParamsPtr params,
    blink::mojom::ServiceWorkerFetchResponseCallbackPtr response_callback,
    DispatchFetchEventCallback callback) {
  DCHECK(worker_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(context_);
  int event_id = context_->timeout_timer->StartEvent(
      CreateAbortCallback(&context_->fetch_event_callbacks));
  context_->fetch_event_callbacks.emplace(event_id, std::move(callback));
  context_->fetch_response_callbacks.emplace(event_id,
                                             std::move(response_callback));

  // This TRACE_EVENT is used for perf benchmark to confirm if all of fetch
  // events have completed. (crbug.com/736697)
  TRACE_EVENT_WITH_FLOW1(
      "ServiceWorker", "ServiceWorkerContextClient::DispatchFetchEvent",
      TRACE_ID_WITH_SCOPE(kServiceWorkerContextClientScope,
                          TRACE_ID_LOCAL(event_id)),
      TRACE_EVENT_FLAG_FLOW_OUT, "url", params->request->url.spec());

  // Set up for navigation preload (FetchEvent#preloadResponse) if needed.
  const bool navigation_preload_sent = !!params->preload_handle;
  if (navigation_preload_sent) {
    SetupNavigationPreload(event_id, params->request->url,
                           std::move(params->preload_handle));
  }

  // Dispatch the event to the service worker execution context.
  blink::WebServiceWorkerRequest web_request;
  ToWebServiceWorkerRequestForFetchEvent(std::move(params->request),
                                         params->client_id, &web_request);
  proxy_->DispatchFetchEvent(event_id, web_request, navigation_preload_sent);
}

void ServiceWorkerContextClient::DispatchNotificationClickEvent(
    const std::string& notification_id,
    const blink::PlatformNotificationData& notification_data,
    int action_index,
    const base::Optional<base::string16>& reply,
    DispatchNotificationClickEventCallback callback) {
  DCHECK(worker_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(context_);
  int request_id = context_->timeout_timer->StartEvent(
      CreateAbortCallback(&context_->notification_click_event_callbacks));
  context_->notification_click_event_callbacks.emplace(request_id,
                                                       std::move(callback));
  TRACE_EVENT_WITH_FLOW0(
      "ServiceWorker",
      "ServiceWorkerContextClient::DispatchNotificationClickEvent",
      TRACE_ID_WITH_SCOPE(kServiceWorkerContextClientScope,
                          TRACE_ID_LOCAL(request_id)),
      TRACE_EVENT_FLAG_FLOW_OUT);

  blink::WebString web_reply;
  if (reply)
    web_reply = blink::WebString::FromUTF16(reply.value());

  proxy_->DispatchNotificationClickEvent(
      request_id, blink::WebString::FromUTF8(notification_id),
      ToWebNotificationData(notification_data), action_index, web_reply);
}

void ServiceWorkerContextClient::DispatchNotificationCloseEvent(
    const std::string& notification_id,
    const blink::PlatformNotificationData& notification_data,
    DispatchNotificationCloseEventCallback callback) {
  DCHECK(worker_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(context_);
  int request_id = context_->timeout_timer->StartEvent(
      CreateAbortCallback(&context_->notification_close_event_callbacks));
  context_->notification_close_event_callbacks.emplace(request_id,
                                                       std::move(callback));
  TRACE_EVENT_WITH_FLOW0(
      "ServiceWorker",
      "ServiceWorkerContextClient::DispatchNotificationCloseEvent",
      TRACE_ID_WITH_SCOPE(kServiceWorkerContextClientScope,
                          TRACE_ID_LOCAL(request_id)),
      TRACE_EVENT_FLAG_FLOW_OUT);
  proxy_->DispatchNotificationCloseEvent(
      request_id, blink::WebString::FromUTF8(notification_id),
      ToWebNotificationData(notification_data));
}

void ServiceWorkerContextClient::DispatchPushEvent(
    const base::Optional<std::string>& payload,
    DispatchPushEventCallback callback) {
  DCHECK(worker_task_runner_->RunsTasksInCurrentSequence());
  // |context_| is valid because the Mojo binding is on |worker_task_runner_|.
  DCHECK(context_);
  int request_id = context_->timeout_timer->StartEventWithCustomTimeout(
      CreateAbortCallback(&context_->push_event_callbacks),
      base::TimeDelta::FromSeconds(blink::mojom::kPushEventTimeoutSeconds));
  context_->push_event_callbacks.emplace(request_id, std::move(callback));
  TRACE_EVENT_WITH_FLOW0("ServiceWorker",
                         "ServiceWorkerContextClient::DispatchPushEvent",
                         TRACE_ID_WITH_SCOPE(kServiceWorkerContextClientScope,
                                             TRACE_ID_LOCAL(request_id)),
                         TRACE_EVENT_FLAG_FLOW_OUT);

  // Only set data to be a valid string if the payload had decrypted data.
  blink::WebString data;
  if (payload)
    data = blink::WebString::FromUTF8(*payload);
  proxy_->DispatchPushEvent(request_id, data);
}

void ServiceWorkerContextClient::DispatchCookieChangeEvent(
    const net::CanonicalCookie& cookie,
    ::network::mojom::CookieChangeCause cause,
    DispatchCookieChangeEventCallback callback) {
  DCHECK(worker_task_runner_->RunsTasksInCurrentSequence());
  // |context_| is valid because the Mojo binding is on |worker_task_runner_|.
  DCHECK(context_);
  int request_id = context_->timeout_timer->StartEvent(
      CreateAbortCallback(&context_->cookie_change_event_callbacks));
  context_->cookie_change_event_callbacks.emplace(request_id,
                                                  std::move(callback));
  TRACE_EVENT_WITH_FLOW0(
      "ServiceWorker", "ServiceWorkerContextClient::DispatchCookieChangeEvent",
      TRACE_ID_WITH_SCOPE(kServiceWorkerContextClientScope,
                          TRACE_ID_LOCAL(request_id)),
      TRACE_EVENT_FLAG_FLOW_OUT);

  // After onion-souping, the conversion below will be done by mojo directly.
  DCHECK(!cookie.IsHttpOnly());
  base::Optional<blink::WebCanonicalCookie> web_cookie_opt =
      blink::WebCanonicalCookie::Create(
          blink::WebString::FromUTF8(cookie.Name()),
          blink::WebString::FromUTF8(cookie.Value()),
          blink::WebString::FromUTF8(cookie.Domain()),
          blink::WebString::FromUTF8(cookie.Path()), cookie.CreationDate(),
          cookie.ExpiryDate(), cookie.LastAccessDate(), cookie.IsSecure(),
          false /* cookie.IsHttpOnly() */,
          static_cast<network::mojom::CookieSameSite>(cookie.SameSite()),
          static_cast<network::mojom::CookiePriority>(cookie.Priority()));
  DCHECK(web_cookie_opt.has_value());

  proxy_->DispatchCookieChangeEvent(request_id, web_cookie_opt.value(), cause);
}

void ServiceWorkerContextClient::Ping(PingCallback callback) {
  DCHECK(worker_task_runner_->RunsTasksInCurrentSequence());
  std::move(callback).Run();
}

void ServiceWorkerContextClient::SetIdleTimerDelayToZero() {
  DCHECK(worker_task_runner_->RunsTasksInCurrentSequence());
  // |context_| is valid because the Mojo binding is on |worker_task_runner_|.
  DCHECK(context_);
  DCHECK(context_->timeout_timer);
  context_->timeout_timer->SetIdleTimerDelayToZero();
}

void ServiceWorkerContextClient::SetupNavigationPreload(
    int fetch_event_id,
    const GURL& url,
    blink::mojom::FetchEventPreloadHandlePtr preload_handle) {
  DCHECK(worker_task_runner_->RunsTasksInCurrentSequence());
  // |context_| is valid because it's valid in our callsite.
  DCHECK(context_);
  auto preload_request = std::make_unique<NavigationPreloadRequest>(
      GetWeakPtr(), fetch_event_id, url, std::move(preload_handle));
  context_->preload_requests.AddWithID(std::move(preload_request),
                                       fetch_event_id);
}

void ServiceWorkerContextClient::OnIdleTimeout() {
  DCHECK(worker_task_runner_->RunsTasksInCurrentSequence());
  // |context_| is valid because this is called by a timer owned by |context_|.
  DCHECK(context_);
  // RequestedTermination() returns true if ServiceWorkerTimeoutTimer agrees
  // we should request the host to terminate this worker now.
  DCHECK(RequestedTermination());
  (*instance_host_)
      ->RequestTermination(base::BindOnce(
          &ServiceWorkerContextClient::OnRequestedTermination, GetWeakPtr()));
}

void ServiceWorkerContextClient::OnRequestedTermination(
    bool will_be_terminated) {
  DCHECK(worker_task_runner_->RunsTasksInCurrentSequence());
  // |context_| is valid because the Mojo binding is on |worker_task_runner_|.
  DCHECK(context_);
  DCHECK(context_->timeout_timer);

  // This worker will be terminated soon. Ignore the message.
  if (will_be_terminated)
    return;

  // Dispatch a dummy event to run all of queued tasks. This updates the
  // idle timer too.
  const int event_id = context_->timeout_timer->StartEvent(base::DoNothing());
  context_->timeout_timer->EndEvent(event_id);
}

bool ServiceWorkerContextClient::RequestedTermination() const {
  DCHECK(worker_task_runner_->RunsTasksInCurrentSequence());
  // |context_| is valid because it's valid at our callsites.
  DCHECK(context_);
  DCHECK(context_->timeout_timer);
  return context_->timeout_timer->did_idle_timeout();
}

void ServiceWorkerContextClient::StopWorkerOnMainThread() {
  DCHECK(main_thread_task_runner_->RunsTasksInCurrentSequence());
  owner_->StopWorker();
}

base::WeakPtr<ServiceWorkerContextClient>
ServiceWorkerContextClient::GetWeakPtr() {
  DCHECK(worker_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(context_);
  return context_->weak_factory.GetWeakPtr();
}

void ServiceWorkerContextClient::SetTimeoutTimerForTesting(
    std::unique_ptr<ServiceWorkerTimeoutTimer> timeout_timer) {
  DCHECK(context_);
  context_->timeout_timer = std::move(timeout_timer);
}

ServiceWorkerTimeoutTimer*
ServiceWorkerContextClient::GetTimeoutTimerForTesting() {
  DCHECK(worker_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(context_);
  return context_->timeout_timer.get();
}

}  // namespace content
