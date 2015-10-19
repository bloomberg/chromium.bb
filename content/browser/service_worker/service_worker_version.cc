// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_version.h"

#include <algorithm>
#include <map>
#include <string>

#include "base/command_line.h"
#include "base/guid.h"
#include "base/location.h"
#include "base/memory/ref_counted.h"
#include "base/metrics/histogram_macros.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "base/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "content/browser/bad_message.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/frame_host/frame_tree_node.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/browser/message_port_message_filter.h"
#include "content/browser/message_port_service.h"
#include "content/browser/service_worker/embedded_worker_instance.h"
#include "content/browser/service_worker/embedded_worker_registry.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/service_worker/service_worker_metrics.h"
#include "content/browser/service_worker/service_worker_registration.h"
#include "content/browser/storage_partition_impl.h"
#include "content/common/service_worker/service_worker_messages.h"
#include "content/common/service_worker/service_worker_type_converters.h"
#include "content/common/service_worker/service_worker_utils.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/background_sync.mojom.h"
#include "content/public/common/child_process_host.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/result_codes.h"
#include "content/public/common/service_registry.h"
#include "mojo/common/common_type_converters.h"
#include "mojo/common/url_type_converters.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_response_info.h"

namespace content {

using StatusCallback = ServiceWorkerVersion::StatusCallback;
using ServiceWorkerClients = std::vector<ServiceWorkerClientInfo>;
using GetClientsCallback =
    base::Callback<void(scoped_ptr<ServiceWorkerClients>)>;

namespace {

// Time to wait until stopping an idle worker.
const int kIdleWorkerTimeoutSeconds = 30;

// Default delay for scheduled update.
const int kUpdateDelaySeconds = 1;

// Timeout for waiting for a response to a ping.
const int kPingTimeoutSeconds = 30;

// If the SW was destructed while starting up, how many seconds it
// had to start up for this to be considered a timeout occurrence.
const int kDestructedStartingWorkerTimeoutThresholdSeconds = 5;

const char kClaimClientsStateErrorMesage[] =
    "Only the active worker can claim clients.";

const char kClaimClientsShutdownErrorMesage[] =
    "Failed to claim clients due to Service Worker system shutdown.";

void RunSoon(const base::Closure& callback) {
  if (!callback.is_null())
    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE, callback);
}

template <typename CallbackArray, typename Arg>
void RunCallbacks(ServiceWorkerVersion* version,
                  CallbackArray* callbacks_ptr,
                  const Arg& arg) {
  CallbackArray callbacks;
  callbacks.swap(*callbacks_ptr);
  for (const auto& callback : callbacks)
    callback.Run(arg);
}

template <typename IDMAP, typename... Params>
void RunIDMapCallbacks(IDMAP* requests, const Params&... params) {
  typename IDMAP::iterator iter(requests);
  while (!iter.IsAtEnd()) {
    iter.GetCurrentValue()->callback.Run(params...);
    iter.Advance();
  }
  requests->Clear();
}

template <typename CallbackType, typename... Params>
bool RunIDMapCallback(IDMap<CallbackType, IDMapOwnPointer>* requests,
                      int request_id,
                      const Params&... params) {
  CallbackType* request = requests->Lookup(request_id);
  if (!request)
    return false;

  request->callback.Run(params...);
  requests->Remove(request_id);
  return true;
}

void RunStartWorkerCallback(
    const StatusCallback& callback,
    scoped_refptr<ServiceWorkerRegistration> protect,
    ServiceWorkerStatusCode status) {
  callback.Run(status);
}

// A callback adapter to start a |task| after StartWorker.
void RunTaskAfterStartWorker(
    base::WeakPtr<ServiceWorkerVersion> version,
    const StatusCallback& error_callback,
    const base::Closure& task,
    ServiceWorkerStatusCode status) {
  if (status != SERVICE_WORKER_OK) {
    if (!error_callback.is_null())
      error_callback.Run(status);
    return;
  }
  if (version->running_status() != ServiceWorkerVersion::RUNNING) {
    // We've tried to start the worker (and it has succeeded), but
    // it looks it's not running yet.
    NOTREACHED() << "The worker's not running after successful StartWorker";
    if (!error_callback.is_null())
      error_callback.Run(SERVICE_WORKER_ERROR_START_WORKER_FAILED);
    return;
  }
  task.Run();
}

void RunErrorFetchCallback(const ServiceWorkerVersion::FetchCallback& callback,
                           ServiceWorkerStatusCode status) {
  callback.Run(status,
               SERVICE_WORKER_FETCH_EVENT_RESULT_FALLBACK,
               ServiceWorkerResponse());
}

void RunErrorMessageCallback(
    const std::vector<TransferredMessagePort>& sent_message_ports,
    const ServiceWorkerVersion::StatusCallback& callback,
    ServiceWorkerStatusCode status) {
  // Transfering the message ports failed, so destroy the ports.
  for (const TransferredMessagePort& port : sent_message_ports) {
    MessagePortService::GetInstance()->ClosePort(port.id);
  }
  callback.Run(status);
}

void RunErrorServicePortConnectCallback(
    const ServiceWorkerVersion::ServicePortConnectCallback& callback,
    ServiceWorkerStatusCode status) {
  callback.Run(status, false /* accept_connection */, base::string16(),
               base::string16());
}

using OpenURLCallback = base::Callback<void(int, int)>;

// The OpenURLObserver class is a WebContentsObserver that will wait for a
// WebContents to be initialized, run the |callback| passed to its constructor
// then self destroy.
// The callback will receive the process and frame ids. If something went wrong
// those will be (kInvalidUniqueID, MSG_ROUTING_NONE).
// The callback will be called in the IO thread.
class OpenURLObserver : public WebContentsObserver {
 public:
  OpenURLObserver(WebContents* web_contents,
                  int frame_tree_node_id,
                  const OpenURLCallback& callback)
      : WebContentsObserver(web_contents),
        frame_tree_node_id_(frame_tree_node_id),
        callback_(callback) {}

  void DidCommitProvisionalLoadForFrame(
      RenderFrameHost* render_frame_host,
      const GURL& validated_url,
      ui::PageTransition transition_type) override {
    DCHECK(web_contents());

    RenderFrameHostImpl* rfhi =
        static_cast<RenderFrameHostImpl*>(render_frame_host);
    if (rfhi->frame_tree_node()->frame_tree_node_id() != frame_tree_node_id_)
      return;

    RunCallback(render_frame_host->GetProcess()->GetID(),
                render_frame_host->GetRoutingID());
  }

  void RenderProcessGone(base::TerminationStatus status) override {
    RunCallback(ChildProcessHost::kInvalidUniqueID, MSG_ROUTING_NONE);
  }

  void WebContentsDestroyed() override {
    RunCallback(ChildProcessHost::kInvalidUniqueID, MSG_ROUTING_NONE);
  }

 private:
  void RunCallback(int render_process_id, int render_frame_id) {
    // After running the callback, |this| will stop observing, thus
    // web_contents() should return nullptr and |RunCallback| should no longer
    // be called. Then, |this| will self destroy.
    DCHECK(web_contents());

    BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
                            base::Bind(callback_,
                                       render_process_id,
                                       render_frame_id));
    Observe(nullptr);
    base::MessageLoop::current()->DeleteSoon(FROM_HERE, this);
  }

  int frame_tree_node_id_;
  const OpenURLCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(OpenURLObserver);
};

// This is only called for main frame navigations in OpenWindowOnUI().
void DidOpenURL(const OpenURLCallback& callback, WebContents* web_contents) {
  DCHECK(web_contents);

  RenderFrameHostImpl* rfhi =
      static_cast<RenderFrameHostImpl*>(web_contents->GetMainFrame());
  new OpenURLObserver(web_contents,
                      rfhi->frame_tree_node()->frame_tree_node_id(), callback);
}

void NavigateClientOnUI(const GURL& url,
                        const GURL& script_url,
                        int process_id,
                        int frame_id,
                        const OpenURLCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  RenderFrameHostImpl* rfhi = RenderFrameHostImpl::FromID(process_id, frame_id);
  WebContents* web_contents = WebContents::FromRenderFrameHost(rfhi);

  if (!rfhi || !web_contents) {
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(callback, ChildProcessHost::kInvalidUniqueID,
                   MSG_ROUTING_NONE));
    return;
  }

  ui::PageTransition transition = rfhi->GetParent()
                                      ? ui::PAGE_TRANSITION_AUTO_SUBFRAME
                                      : ui::PAGE_TRANSITION_AUTO_TOPLEVEL;
  int frame_tree_node_id = rfhi->frame_tree_node()->frame_tree_node_id();

  OpenURLParams params(
      url, Referrer::SanitizeForRequest(
               url, Referrer(script_url, blink::WebReferrerPolicyDefault)),
      frame_tree_node_id, CURRENT_TAB, transition,
      true /* is_renderer_initiated */);
  web_contents->OpenURL(params);
  new OpenURLObserver(web_contents, frame_tree_node_id, callback);
}

void OpenWindowOnUI(
    const GURL& url,
    const GURL& script_url,
    int process_id,
    const scoped_refptr<ServiceWorkerContextWrapper>& context_wrapper,
    const OpenURLCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  BrowserContext* browser_context = context_wrapper->storage_partition()
      ? context_wrapper->storage_partition()->browser_context()
      : nullptr;
  // We are shutting down.
  if (!browser_context)
    return;

  RenderProcessHost* render_process_host =
      RenderProcessHost::FromID(process_id);
  if (render_process_host->IsForGuestsOnly()) {
    BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
                            base::Bind(callback,
                                       ChildProcessHost::kInvalidUniqueID,
                                       MSG_ROUTING_NONE));
    return;
  }

  OpenURLParams params(
      url, Referrer::SanitizeForRequest(
               url, Referrer(script_url, blink::WebReferrerPolicyDefault)),
      NEW_FOREGROUND_TAB, ui::PAGE_TRANSITION_AUTO_TOPLEVEL,
      true /* is_renderer_initiated */);

  GetContentClient()->browser()->OpenURL(
      browser_context, params,
      base::Bind(&DidOpenURL, callback));
}

void KillEmbeddedWorkerProcess(int process_id, ResultCode code) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  RenderProcessHost* render_process_host =
      RenderProcessHost::FromID(process_id);
  if (render_process_host->GetHandle() != base::kNullProcessHandle) {
    bad_message::ReceivedBadMessage(render_process_host,
                                    bad_message::SERVICE_WORKER_BAD_URL);
  }
}

void ClearTick(base::TimeTicks* time) {
  *time = base::TimeTicks();
}

void RestartTick(base::TimeTicks* time) {
  *time = base::TimeTicks().Now();
}

base::TimeDelta GetTickDuration(const base::TimeTicks& time) {
  if (time.is_null())
    return base::TimeDelta();
  return base::TimeTicks().Now() - time;
}

void OnGetWindowClientsFromUI(
    // The tuple contains process_id, frame_id, client_uuid.
    const std::vector<base::Tuple<int, int, std::string>>& clients_info,
    const GURL& script_url,
    const GetClientsCallback& callback) {
  scoped_ptr<ServiceWorkerClients> clients(new ServiceWorkerClients);

  for (const auto& it : clients_info) {
    ServiceWorkerClientInfo info =
        ServiceWorkerProviderHost::GetWindowClientInfoOnUI(base::get<0>(it),
                                                           base::get<1>(it));

    // If the request to the provider_host returned an empty
    // ServiceWorkerClientInfo, that means that it wasn't possible to associate
    // it with a valid RenderFrameHost. It might be because the frame was killed
    // or navigated in between.
    if (info.IsEmpty())
      continue;

    // We can get info for a frame that was navigating end ended up with a
    // different URL than expected. In such case, we should make sure to not
    // expose cross-origin WindowClient.
    if (info.url.GetOrigin() != script_url.GetOrigin())
      continue;

    info.client_uuid = base::get<2>(it);
    clients->push_back(info);
  }

  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
                          base::Bind(callback, base::Passed(&clients)));
}

void AddWindowClient(
    ServiceWorkerProviderHost* host,
    std::vector<base::Tuple<int, int, std::string>>* client_info) {
  if (host->client_type() != blink::WebServiceWorkerClientTypeWindow)
    return;
  client_info->push_back(base::MakeTuple(host->process_id(), host->frame_id(),
                                         host->client_uuid()));
}

void AddNonWindowClient(ServiceWorkerProviderHost* host,
                        const ServiceWorkerClientQueryOptions& options,
                        ServiceWorkerClients* clients) {
  blink::WebServiceWorkerClientType host_client_type = host->client_type();
  if (host_client_type == blink::WebServiceWorkerClientTypeWindow)
    return;
  if (options.client_type != blink::WebServiceWorkerClientTypeAll &&
      options.client_type != host_client_type)
    return;

  ServiceWorkerClientInfo client_info(blink::WebPageVisibilityStateHidden,
                                      false,  // is_focused
                                      host->document_url(),
                                      REQUEST_CONTEXT_FRAME_TYPE_NONE,
                                      base::TimeTicks(), host_client_type);
  client_info.client_uuid = host->client_uuid();
  clients->push_back(client_info);
}

bool IsInstalled(ServiceWorkerVersion::Status status) {
  switch (status) {
    case ServiceWorkerVersion::NEW:
    case ServiceWorkerVersion::INSTALLING:
    case ServiceWorkerVersion::REDUNDANT:
      return false;
    case ServiceWorkerVersion::INSTALLED:
    case ServiceWorkerVersion::ACTIVATING:
    case ServiceWorkerVersion::ACTIVATED:
      return true;
  }
  NOTREACHED() << "Unexpected status: " << status;
  return false;
}

struct ServiceWorkerClientInfoSortMRU {
  bool operator()(const ServiceWorkerClientInfo& a,
                  const ServiceWorkerClientInfo& b) const {
    return a.last_focus_time > b.last_focus_time;
  }
};

}  // namespace

const int ServiceWorkerVersion::kTimeoutTimerDelaySeconds = 30;
const int ServiceWorkerVersion::kStartWorkerTimeoutMinutes = 5;
const int ServiceWorkerVersion::kRequestTimeoutMinutes = 5;
const int ServiceWorkerVersion::kStopWorkerTimeoutSeconds = 5;

class ServiceWorkerVersion::Metrics {
 public:
  using EventType = ServiceWorkerMetrics::EventType;
  explicit Metrics(ServiceWorkerVersion* owner) : owner_(owner) {}
  ~Metrics() {
    if (owner_->should_exclude_from_uma_)
      return;
    for (const auto& ev : event_stats_) {
      ServiceWorkerMetrics::RecordEventHandledRatio(
          ev.first, ev.second.handled_events, ev.second.fired_events);
    }
  }

  void RecordEventHandledStatus(EventType event, bool handled) {
    event_stats_[event].fired_events++;
    if (handled)
      event_stats_[event].handled_events++;
  }

 private:
  struct EventStat {
    size_t fired_events = 0;
    size_t handled_events = 0;
  };

  ServiceWorkerVersion* owner_;
  std::map<EventType, EventStat> event_stats_;

  DISALLOW_COPY_AND_ASSIGN(Metrics);
};

// A controller for periodically sending a ping to the worker to see
// if the worker is not stalling.
class ServiceWorkerVersion::PingController {
 public:
  explicit PingController(ServiceWorkerVersion* version) : version_(version) {}
  ~PingController() {}

  void Activate() { ping_state_ = PINGING; }

  void Deactivate() {
    ClearTick(&ping_time_);
    ping_state_ = NOT_PINGING;
  }

  void OnPongReceived() { ClearTick(&ping_time_); }

  bool IsTimedOut() { return ping_state_ == PING_TIMED_OUT; }

  // Checks ping status. This is supposed to be called periodically.
  // This may call:
  // - OnPingTimeout() if the worker hasn't reponded within a certain period.
  // - PingWorker() if we're running ping timer and can send next ping.
  void CheckPingStatus() {
    if (GetTickDuration(ping_time_) >
        base::TimeDelta::FromSeconds(kPingTimeoutSeconds)) {
      ping_state_ = PING_TIMED_OUT;
      version_->OnPingTimeout();
      return;
    }

    // Check if we want to send a next ping.
    if (ping_state_ != PINGING || !ping_time_.is_null())
      return;

    if (version_->PingWorker() != SERVICE_WORKER_OK) {
      // TODO(falken): Maybe try resending Ping a few times first?
      ping_state_ = PING_TIMED_OUT;
      version_->OnPingTimeout();
      return;
    }
    RestartTick(&ping_time_);
  }

  void SimulateTimeoutForTesting() {
    version_->PingWorker();
    ping_state_ = PING_TIMED_OUT;
    version_->OnPingTimeout();
  }

 private:
  enum PingState { NOT_PINGING, PINGING, PING_TIMED_OUT };
  ServiceWorkerVersion* version_;  // Not owned.
  base::TimeTicks ping_time_;
  PingState ping_state_ = NOT_PINGING;
  DISALLOW_COPY_AND_ASSIGN(PingController);
};

ServiceWorkerVersion::ServiceWorkerVersion(
    ServiceWorkerRegistration* registration,
    const GURL& script_url,
    int64 version_id,
    base::WeakPtr<ServiceWorkerContextCore> context)
    : version_id_(version_id),
      registration_id_(registration->id()),
      script_url_(script_url),
      scope_(registration->pattern()),
      context_(context),
      script_cache_map_(this, context),
      ping_controller_(new PingController(this)),
      should_exclude_from_uma_(
          ServiceWorkerMetrics::ShouldExcludeURLFromHistogram(scope_)),
      weak_factory_(this) {
  DCHECK(context_);
  DCHECK(registration);
  context_->AddLiveVersion(this);
  embedded_worker_ = context_->embedded_worker_registry()->CreateWorker();
  embedded_worker_->AddListener(this);
}

ServiceWorkerVersion::~ServiceWorkerVersion() {
  in_dtor_ = true;

  // The user may have closed the tab waiting for SW to start up.
  if (GetTickDuration(start_time_) >
      base::TimeDelta::FromSeconds(
          kDestructedStartingWorkerTimeoutThresholdSeconds)) {
    DCHECK(timeout_timer_.IsRunning());
    DCHECK(!embedded_worker_->devtools_attached());
    RecordStartWorkerResult(SERVICE_WORKER_ERROR_TIMEOUT);
  }

  if (context_)
    context_->RemoveLiveVersion(version_id_);

  if (running_status() == STARTING || running_status() == RUNNING)
    embedded_worker_->Stop();
  embedded_worker_->RemoveListener(this);
}

void ServiceWorkerVersion::SetStatus(Status status) {
  if (status_ == status)
    return;

  status_ = status;

  if (skip_waiting_ && status_ == ACTIVATED) {
    for (int request_id : pending_skip_waiting_requests_)
      DidSkipWaiting(request_id);
    pending_skip_waiting_requests_.clear();
  }

  std::vector<base::Closure> callbacks;
  callbacks.swap(status_change_callbacks_);
  for (const auto& callback : callbacks)
    callback.Run();

  FOR_EACH_OBSERVER(Listener, listeners_, OnVersionStateChanged(this));
}

void ServiceWorkerVersion::RegisterStatusChangeCallback(
    const base::Closure& callback) {
  status_change_callbacks_.push_back(callback);
}

ServiceWorkerVersionInfo ServiceWorkerVersion::GetInfo() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  ServiceWorkerVersionInfo info(
      running_status(), status(), script_url(), registration_id(), version_id(),
      embedded_worker()->process_id(), embedded_worker()->thread_id(),
      embedded_worker()->worker_devtools_agent_route_id());
  for (const auto& controllee : controllee_map_) {
    const ServiceWorkerProviderHost* host = controllee.second;
    info.clients.insert(std::make_pair(
        host->client_uuid(),
        ServiceWorkerVersionInfo::ClientInfo(
            host->process_id(), host->route_id(), host->provider_type())));
  }
  if (!main_script_http_info_)
    return info;
  info.script_response_time = main_script_http_info_->response_time;
  if (main_script_http_info_->headers)
    main_script_http_info_->headers->GetLastModifiedValue(
        &info.script_last_modified);
  return info;
}

void ServiceWorkerVersion::StartWorker(const StatusCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!context_) {
    RecordStartWorkerResult(SERVICE_WORKER_ERROR_ABORT);
    RunSoon(base::Bind(callback, SERVICE_WORKER_ERROR_ABORT));
    return;
  }
  if (is_redundant()) {
    RecordStartWorkerResult(SERVICE_WORKER_ERROR_REDUNDANT);
    RunSoon(base::Bind(callback, SERVICE_WORKER_ERROR_REDUNDANT));
    return;
  }
  // Check that the worker is allowed to start on the given scope. Since this
  // worker might not be used for a specific frame/process, use -1.
  // resource_context() can return null in unit tests.
  if (context_->wrapper()->resource_context() &&
      !GetContentClient()->browser()->AllowServiceWorker(
          scope_, scope_, context_->wrapper()->resource_context(), -1, -1)) {
    RecordStartWorkerResult(SERVICE_WORKER_ERROR_DISALLOWED);
    RunSoon(base::Bind(callback, SERVICE_WORKER_ERROR_DISALLOWED));
    return;
  }

  prestart_status_ = status_;

  // Ensure the live registration during starting worker so that the worker can
  // get associated with it in SWDispatcherHost::OnSetHostedVersionId().
  context_->storage()->FindRegistrationForId(
      registration_id_,
      scope_.GetOrigin(),
      base::Bind(&ServiceWorkerVersion::DidEnsureLiveRegistrationForStartWorker,
                 weak_factory_.GetWeakPtr(),
                 callback));
}

void ServiceWorkerVersion::StopWorker(const StatusCallback& callback) {
  if (running_status() == STOPPED) {
    RunSoon(base::Bind(callback, SERVICE_WORKER_OK));
    return;
  }
  if (stop_callbacks_.empty()) {
    ServiceWorkerStatusCode status = embedded_worker_->Stop();
    if (status != SERVICE_WORKER_OK) {
      RunSoon(base::Bind(callback, status));
      return;
    }
  }
  stop_callbacks_.push_back(callback);
}

void ServiceWorkerVersion::ScheduleUpdate() {
  if (!context_)
    return;
  if (update_timer_.IsRunning()) {
    update_timer_.Reset();
    return;
  }
  if (is_update_scheduled_)
    return;
  is_update_scheduled_ = true;

  // Protect |this| until the timer fires, since we may be stopping
  // and soon no one might hold a reference to us.
  context_->ProtectVersion(make_scoped_refptr(this));
  update_timer_.Start(FROM_HERE,
                      base::TimeDelta::FromSeconds(kUpdateDelaySeconds),
                      base::Bind(&ServiceWorkerVersion::StartUpdate,
                                 weak_factory_.GetWeakPtr()));
}

void ServiceWorkerVersion::StartUpdate() {
  if (!context_)
    return;
  context_->storage()->FindRegistrationForId(
      registration_id_, scope_.GetOrigin(),
      base::Bind(&ServiceWorkerVersion::FoundRegistrationForUpdate,
                 weak_factory_.GetWeakPtr()));
}

void ServiceWorkerVersion::DeferScheduledUpdate() {
  if (update_timer_.IsRunning())
    update_timer_.Reset();
}

void ServiceWorkerVersion::DispatchMessageEvent(
    const base::string16& message,
    const std::vector<TransferredMessagePort>& sent_message_ports,
    const StatusCallback& callback) {
  for (const TransferredMessagePort& port : sent_message_ports) {
    MessagePortService::GetInstance()->HoldMessages(port.id);
  }

  DispatchMessageEventInternal(message, sent_message_ports, callback);
}

void ServiceWorkerVersion::DispatchMessageEventInternal(
    const base::string16& message,
    const std::vector<TransferredMessagePort>& sent_message_ports,
    const StatusCallback& callback) {
  OnBeginEvent();
  if (running_status() != RUNNING) {
    // Schedule calling this method after starting the worker.
    StartWorker(base::Bind(
        &RunTaskAfterStartWorker, weak_factory_.GetWeakPtr(),
        base::Bind(&RunErrorMessageCallback, sent_message_ports, callback),
        base::Bind(&self::DispatchMessageEventInternal,
                   weak_factory_.GetWeakPtr(), message, sent_message_ports,
                   callback)));
    return;
  }

  // TODO(kinuko): Cleanup this (and corresponding unit test) when message
  // event becomes extendable, round-trip event. (crbug.com/498596)
  RestartTick(&idle_time_);

  MessagePortMessageFilter* filter =
      embedded_worker_->message_port_message_filter();
  std::vector<int> new_routing_ids;
  filter->UpdateMessagePortsWithNewRoutes(sent_message_ports, &new_routing_ids);
  ServiceWorkerStatusCode status =
      embedded_worker_->SendMessage(ServiceWorkerMsg_MessageToWorker(
          message, sent_message_ports, new_routing_ids));
  RunSoon(base::Bind(callback, status));
}

void ServiceWorkerVersion::DispatchInstallEvent(
    const StatusCallback& callback) {
  OnBeginEvent();
  DCHECK_EQ(INSTALLING, status()) << status();

  if (running_status() != RUNNING) {
    // Schedule calling this method after starting the worker.
    StartWorker(
        base::Bind(&RunTaskAfterStartWorker,
                   weak_factory_.GetWeakPtr(),
                   callback,
                   base::Bind(&self::DispatchInstallEventAfterStartWorker,
                              weak_factory_.GetWeakPtr(),
                              callback)));
  } else {
    DispatchInstallEventAfterStartWorker(callback);
  }
}

void ServiceWorkerVersion::DispatchActivateEvent(
    const StatusCallback& callback) {
  OnBeginEvent();
  DCHECK_EQ(ACTIVATING, status()) << status();

  if (running_status() != RUNNING) {
    // Schedule calling this method after starting the worker.
    StartWorker(
        base::Bind(&RunTaskAfterStartWorker,
                   weak_factory_.GetWeakPtr(),
                   callback,
                   base::Bind(&self::DispatchActivateEventAfterStartWorker,
                              weak_factory_.GetWeakPtr(),
                              callback)));
  } else {
    DispatchActivateEventAfterStartWorker(callback);
  }
}

void ServiceWorkerVersion::DispatchFetchEvent(
    const ServiceWorkerFetchRequest& request,
    const base::Closure& prepare_callback,
    const FetchCallback& fetch_callback) {
  OnBeginEvent();
  DCHECK_EQ(ACTIVATED, status()) << status();

  if (running_status() != RUNNING) {
    // Schedule calling this method after starting the worker.
    StartWorker(base::Bind(&RunTaskAfterStartWorker,
                           weak_factory_.GetWeakPtr(),
                           base::Bind(&RunErrorFetchCallback, fetch_callback),
                           base::Bind(&self::DispatchFetchEvent,
                                      weak_factory_.GetWeakPtr(),
                                      request,
                                      prepare_callback,
                                      fetch_callback)));
    return;
  }

  prepare_callback.Run();

  int request_id = AddRequest(fetch_callback, &fetch_requests_, REQUEST_FETCH);
  ServiceWorkerStatusCode status = embedded_worker_->SendMessage(
      ServiceWorkerMsg_FetchEvent(request_id, request));
  if (status != SERVICE_WORKER_OK) {
    fetch_requests_.Remove(request_id);
    RunSoon(base::Bind(&RunErrorFetchCallback,
                       fetch_callback,
                       SERVICE_WORKER_ERROR_FAILED));
  }
}

void ServiceWorkerVersion::DispatchSyncEvent(
    BackgroundSyncRegistrationHandle::HandleId handle_id,
    const StatusCallback& callback) {
  OnBeginEvent();
  DCHECK_EQ(ACTIVATED, status()) << status();
  if (running_status() != RUNNING) {
    // Schedule calling this method after starting the worker.
    StartWorker(base::Bind(
        &RunTaskAfterStartWorker, weak_factory_.GetWeakPtr(), callback,
        base::Bind(&self::DispatchSyncEvent, weak_factory_.GetWeakPtr(),
                   handle_id, callback)));
    return;
  }

  int request_id = AddRequest(callback, &sync_requests_, REQUEST_SYNC);
  if (!background_sync_dispatcher_) {
    embedded_worker_->GetServiceRegistry()->ConnectToRemoteService(
        mojo::GetProxy(&background_sync_dispatcher_));
    background_sync_dispatcher_.set_connection_error_handler(base::Bind(
        &ServiceWorkerVersion::OnBackgroundSyncDispatcherConnectionError,
        weak_factory_.GetWeakPtr()));
  }

  background_sync_dispatcher_->Sync(
      handle_id, base::Bind(&self::OnSyncEventFinished,
                            weak_factory_.GetWeakPtr(), request_id));
}

void ServiceWorkerVersion::DispatchNotificationClickEvent(
    const StatusCallback& callback,
    int64_t persistent_notification_id,
    const PlatformNotificationData& notification_data,
    int action_index) {
  OnBeginEvent();
  DCHECK_EQ(ACTIVATED, status()) << status();
  if (running_status() != RUNNING) {
    // Schedule calling this method after starting the worker.
    StartWorker(base::Bind(
        &RunTaskAfterStartWorker, weak_factory_.GetWeakPtr(), callback,
        base::Bind(&self::DispatchNotificationClickEvent,
                   weak_factory_.GetWeakPtr(), callback,
                   persistent_notification_id, notification_data,
                   action_index)));
    return;
  }

  int request_id = AddRequest(callback, &notification_click_requests_,
                              REQUEST_NOTIFICATION_CLICK);
  ServiceWorkerStatusCode status =
      embedded_worker_->SendMessage(ServiceWorkerMsg_NotificationClickEvent(
          request_id, persistent_notification_id, notification_data,
          action_index));
  if (status != SERVICE_WORKER_OK) {
    notification_click_requests_.Remove(request_id);
    RunSoon(base::Bind(callback, status));
  }
}

void ServiceWorkerVersion::DispatchPushEvent(const StatusCallback& callback,
                                             const std::string& data) {
  OnBeginEvent();
  DCHECK_EQ(ACTIVATED, status()) << status();
  if (running_status() != RUNNING) {
    // Schedule calling this method after starting the worker.
    StartWorker(base::Bind(&RunTaskAfterStartWorker,
                           weak_factory_.GetWeakPtr(), callback,
                           base::Bind(&self::DispatchPushEvent,
                                      weak_factory_.GetWeakPtr(),
                                      callback, data)));
    return;
  }

  int request_id = AddRequest(callback, &push_requests_, REQUEST_PUSH);
  ServiceWorkerStatusCode status = embedded_worker_->SendMessage(
      ServiceWorkerMsg_PushEvent(request_id, data));
  if (status != SERVICE_WORKER_OK) {
    push_requests_.Remove(request_id);
    RunSoon(base::Bind(callback, status));
  }
}

void ServiceWorkerVersion::DispatchGeofencingEvent(
    const StatusCallback& callback,
    blink::WebGeofencingEventType event_type,
    const std::string& region_id,
    const blink::WebCircularGeofencingRegion& region) {
  OnBeginEvent();
  DCHECK_EQ(ACTIVATED, status()) << status();

  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableExperimentalWebPlatformFeatures)) {
    callback.Run(SERVICE_WORKER_ERROR_ABORT);
    return;
  }

  if (running_status() != RUNNING) {
    // Schedule calling this method after starting the worker.
    StartWorker(base::Bind(&RunTaskAfterStartWorker,
                           weak_factory_.GetWeakPtr(),
                           callback,
                           base::Bind(&self::DispatchGeofencingEvent,
                                      weak_factory_.GetWeakPtr(),
                                      callback,
                                      event_type,
                                      region_id,
                                      region)));
    return;
  }

  int request_id =
      AddRequest(callback, &geofencing_requests_, REQUEST_GEOFENCING);
  ServiceWorkerStatusCode status =
      embedded_worker_->SendMessage(ServiceWorkerMsg_GeofencingEvent(
          request_id, event_type, region_id, region));
  if (status != SERVICE_WORKER_OK) {
    geofencing_requests_.Remove(request_id);
    RunSoon(base::Bind(callback, status));
  }
}

void ServiceWorkerVersion::DispatchServicePortConnectEvent(
    const ServicePortConnectCallback& callback,
    const GURL& target_url,
    const GURL& origin,
    int port_id) {
  OnBeginEvent();
  DCHECK_EQ(ACTIVATED, status()) << status();

  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableExperimentalWebPlatformFeatures)) {
    callback.Run(SERVICE_WORKER_ERROR_ABORT, false, base::string16(),
                 base::string16());
    return;
  }

  if (running_status() != RUNNING) {
    // Schedule calling this method after starting the worker.
    StartWorker(
        base::Bind(&RunTaskAfterStartWorker, weak_factory_.GetWeakPtr(),
                   base::Bind(&RunErrorServicePortConnectCallback, callback),
                   base::Bind(&self::DispatchServicePortConnectEvent,
                              weak_factory_.GetWeakPtr(), callback, target_url,
                              origin, port_id)));
    return;
  }

  int request_id = AddRequest(callback, &service_port_connect_requests_,
                              REQUEST_SERVICE_PORT_CONNECT);
  if (!service_port_dispatcher_) {
    embedded_worker_->GetServiceRegistry()->ConnectToRemoteService(
        mojo::GetProxy(&service_port_dispatcher_));
    service_port_dispatcher_.set_connection_error_handler(base::Bind(
        &ServiceWorkerVersion::OnServicePortDispatcherConnectionError,
        weak_factory_.GetWeakPtr()));
  }
  service_port_dispatcher_->Connect(
      mojo::String::From(target_url), mojo::String::From(origin), port_id,
      base::Bind(&ServiceWorkerVersion::OnServicePortConnectEventFinished,
                 weak_factory_.GetWeakPtr(), request_id));
}

void ServiceWorkerVersion::DispatchCrossOriginMessageEvent(
    const NavigatorConnectClient& client,
    const base::string16& message,
    const std::vector<TransferredMessagePort>& sent_message_ports,
    const StatusCallback& callback) {
  OnBeginEvent();
  // Unlike in the case of DispatchMessageEvent, here the caller is assumed to
  // have already put all the sent message ports on hold. So no need to do that
  // here again.

  if (running_status() != RUNNING) {
    // Schedule calling this method after starting the worker.
    StartWorker(base::Bind(
        &RunTaskAfterStartWorker, weak_factory_.GetWeakPtr(),
        base::Bind(&RunErrorMessageCallback, sent_message_ports, callback),
        base::Bind(&self::DispatchCrossOriginMessageEvent,
                   weak_factory_.GetWeakPtr(), client, message,
                   sent_message_ports, callback)));
    return;
  }

  MessagePortMessageFilter* filter =
      embedded_worker_->message_port_message_filter();
  std::vector<int> new_routing_ids;
  filter->UpdateMessagePortsWithNewRoutes(sent_message_ports, &new_routing_ids);
  ServiceWorkerStatusCode status =
      embedded_worker_->SendMessage(ServiceWorkerMsg_CrossOriginMessageToWorker(
          client, message, sent_message_ports, new_routing_ids));
  RunSoon(base::Bind(callback, status));
}

void ServiceWorkerVersion::AddControllee(
    ServiceWorkerProviderHost* provider_host) {
  const std::string& uuid = provider_host->client_uuid();
  CHECK(!provider_host->client_uuid().empty());
  DCHECK(!ContainsKey(controllee_map_, uuid));
  controllee_map_[uuid] = provider_host;
  // Keep the worker alive a bit longer right after a new controllee is added.
  RestartTick(&idle_time_);
  FOR_EACH_OBSERVER(Listener, listeners_,
                    OnControlleeAdded(this, provider_host));
}

void ServiceWorkerVersion::RemoveControllee(
    ServiceWorkerProviderHost* provider_host) {
  const std::string& uuid = provider_host->client_uuid();
  DCHECK(ContainsKey(controllee_map_, uuid));
  controllee_map_.erase(uuid);
  FOR_EACH_OBSERVER(Listener, listeners_,
                    OnControlleeRemoved(this, provider_host));
  if (HasControllee())
    return;
  FOR_EACH_OBSERVER(Listener, listeners_, OnNoControllees(this));
}

void ServiceWorkerVersion::AddStreamingURLRequestJob(
    const ServiceWorkerURLRequestJob* request_job) {
  DCHECK(streaming_url_request_jobs_.find(request_job) ==
         streaming_url_request_jobs_.end());
  streaming_url_request_jobs_.insert(request_job);
}

void ServiceWorkerVersion::RemoveStreamingURLRequestJob(
    const ServiceWorkerURLRequestJob* request_job) {
  streaming_url_request_jobs_.erase(request_job);
  if (is_redundant())
    StopWorkerIfIdle();
}

void ServiceWorkerVersion::AddListener(Listener* listener) {
  listeners_.AddObserver(listener);
}

void ServiceWorkerVersion::RemoveListener(Listener* listener) {
  listeners_.RemoveObserver(listener);
}

void ServiceWorkerVersion::ReportError(ServiceWorkerStatusCode status,
                                       const std::string& status_message) {
  if (status_message.empty()) {
    OnReportException(base::UTF8ToUTF16(ServiceWorkerStatusToString(status)),
                      -1, -1, GURL());
  } else {
    OnReportException(base::UTF8ToUTF16(status_message), -1, -1, GURL());
  }
}

void ServiceWorkerVersion::SetStartWorkerStatusCode(
    ServiceWorkerStatusCode status) {
  start_worker_status_ = status;
}

void ServiceWorkerVersion::Doom() {
  DCHECK(!HasControllee());
  SetStatus(REDUNDANT);
  if (running_status() == STARTING || running_status() == RUNNING)
    embedded_worker_->Stop();
  if (!context_)
    return;
  std::vector<ServiceWorkerDatabase::ResourceRecord> resources;
  script_cache_map_.GetResources(&resources);
  context_->storage()->PurgeResources(resources);
}

void ServiceWorkerVersion::SetDevToolsAttached(bool attached) {
  embedded_worker()->set_devtools_attached(attached);
  if (attached) {
    // TODO(falken): Canceling the timeouts when debugging could cause
    // heisenbugs; we should instead run them as normal show an educational
    // message in DevTools when they occur. crbug.com/470419

    // Don't record the startup time metric once DevTools is attached.
    ClearTick(&start_time_);
    skip_recording_startup_time_ = true;

    // Cancel request timeouts.
    SetAllRequestTimes(base::TimeTicks());
    return;
  }
  if (!start_callbacks_.empty()) {
    // Reactivate the timer for start timeout.
    DCHECK(timeout_timer_.IsRunning());
    DCHECK(running_status() == STARTING || running_status() == STOPPING)
        << running_status();
    RestartTick(&start_time_);
  }

  // Reactivate request timeouts.
  SetAllRequestTimes(base::TimeTicks::Now());
}

void ServiceWorkerVersion::SetMainScriptHttpResponseInfo(
    const net::HttpResponseInfo& http_info) {
  main_script_http_info_.reset(new net::HttpResponseInfo(http_info));
  FOR_EACH_OBSERVER(Listener, listeners_,
                    OnMainScriptHttpResponseInfoSet(this));
}

void ServiceWorkerVersion::SimulatePingTimeoutForTesting() {
  ping_controller_->SimulateTimeoutForTesting();
}

const net::HttpResponseInfo*
ServiceWorkerVersion::GetMainScriptHttpResponseInfo() {
  return main_script_http_info_.get();
}

ServiceWorkerVersion::RequestInfo::RequestInfo(int id,
                                               RequestType type,
                                               const base::TimeTicks& now)
    : id(id), type(type), time(now) {
}

ServiceWorkerVersion::RequestInfo::~RequestInfo() {
}

template <typename CallbackType>
ServiceWorkerVersion::PendingRequest<CallbackType>::PendingRequest(
    const CallbackType& callback,
    const base::TimeTicks& time)
    : callback(callback), start_time(time) {
}

template <typename CallbackType>
ServiceWorkerVersion::PendingRequest<CallbackType>::~PendingRequest() {
}

void ServiceWorkerVersion::OnThreadStarted() {
  if (running_status() == STOPPING)
    return;
  DCHECK_EQ(STARTING, running_status());
  // Activate ping/pong now that JavaScript execution will start.
  ping_controller_->Activate();
}

void ServiceWorkerVersion::OnStarting() {
  FOR_EACH_OBSERVER(Listener, listeners_, OnRunningStateChanged(this));
}

void ServiceWorkerVersion::OnStarted() {
  DCHECK_EQ(RUNNING, running_status());
  RestartTick(&idle_time_);

  // Fire all start callbacks.
  scoped_refptr<ServiceWorkerVersion> protect(this);
  RunCallbacks(this, &start_callbacks_, SERVICE_WORKER_OK);
  FOR_EACH_OBSERVER(Listener, listeners_, OnRunningStateChanged(this));
}

void ServiceWorkerVersion::OnStopping() {
  DCHECK(stop_time_.is_null());
  RestartTick(&stop_time_);

  // Shorten the interval so stalling in stopped can be fixed quickly. Once the
  // worker stops, the timer is disabled. The interval will be reset to normal
  // when the worker starts up again.
  DCHECK(timeout_timer_.IsRunning());
  base::TimeDelta delay =
      base::TimeDelta::FromSeconds(kStopWorkerTimeoutSeconds);
  if (timeout_timer_.GetCurrentDelay() != delay) {
    timeout_timer_.Stop();
    timeout_timer_.Start(FROM_HERE, delay, this,
                         &ServiceWorkerVersion::OnTimeoutTimer);
  }

  FOR_EACH_OBSERVER(Listener, listeners_, OnRunningStateChanged(this));
}

void ServiceWorkerVersion::OnStopped(
    EmbeddedWorkerInstance::Status old_status) {
  if (IsInstalled(status())) {
    ServiceWorkerMetrics::RecordWorkerStopped(
        ServiceWorkerMetrics::StopStatus::NORMAL);
  }
  if (!stop_time_.is_null())
    ServiceWorkerMetrics::RecordStopWorkerTime(GetTickDuration(stop_time_));

  OnStoppedInternal(old_status);
}

void ServiceWorkerVersion::OnDetached(
    EmbeddedWorkerInstance::Status old_status) {
  if (IsInstalled(status())) {
    ServiceWorkerMetrics::RecordWorkerStopped(
        ServiceWorkerMetrics::StopStatus::DETACH_BY_REGISTRY);
  }
  OnStoppedInternal(old_status);
}

void ServiceWorkerVersion::OnScriptLoaded() {
  if (IsInstalled(status()))
    UMA_HISTOGRAM_BOOLEAN("ServiceWorker.ScriptLoadSuccess", true);
}

void ServiceWorkerVersion::OnScriptLoadFailed() {
  if (IsInstalled(status()))
    UMA_HISTOGRAM_BOOLEAN("ServiceWorker.ScriptLoadSuccess", false);
}

void ServiceWorkerVersion::OnReportException(
    const base::string16& error_message,
    int line_number,
    int column_number,
    const GURL& source_url) {
  FOR_EACH_OBSERVER(
      Listener,
      listeners_,
      OnErrorReported(
          this, error_message, line_number, column_number, source_url));
}

void ServiceWorkerVersion::OnReportConsoleMessage(int source_identifier,
                                                  int message_level,
                                                  const base::string16& message,
                                                  int line_number,
                                                  const GURL& source_url) {
  FOR_EACH_OBSERVER(Listener,
                    listeners_,
                    OnReportConsoleMessage(this,
                                           source_identifier,
                                           message_level,
                                           message,
                                           line_number,
                                           source_url));
}

bool ServiceWorkerVersion::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(ServiceWorkerVersion, message)
    IPC_MESSAGE_HANDLER(ServiceWorkerHostMsg_GetClients,
                        OnGetClients)
    IPC_MESSAGE_HANDLER(ServiceWorkerHostMsg_ActivateEventFinished,
                        OnActivateEventFinished)
    IPC_MESSAGE_HANDLER(ServiceWorkerHostMsg_InstallEventFinished,
                        OnInstallEventFinished)
    IPC_MESSAGE_HANDLER(ServiceWorkerHostMsg_FetchEventFinished,
                        OnFetchEventFinished)
    IPC_MESSAGE_HANDLER(ServiceWorkerHostMsg_NotificationClickEventFinished,
                        OnNotificationClickEventFinished)
    IPC_MESSAGE_HANDLER(ServiceWorkerHostMsg_PushEventFinished,
                        OnPushEventFinished)
    IPC_MESSAGE_HANDLER(ServiceWorkerHostMsg_GeofencingEventFinished,
                        OnGeofencingEventFinished)
    IPC_MESSAGE_HANDLER(ServiceWorkerHostMsg_OpenWindow,
                        OnOpenWindow)
    IPC_MESSAGE_HANDLER(ServiceWorkerHostMsg_SetCachedMetadata,
                        OnSetCachedMetadata)
    IPC_MESSAGE_HANDLER(ServiceWorkerHostMsg_ClearCachedMetadata,
                        OnClearCachedMetadata)
    IPC_MESSAGE_HANDLER(ServiceWorkerHostMsg_PostMessageToClient,
                        OnPostMessageToClient)
    IPC_MESSAGE_HANDLER(ServiceWorkerHostMsg_FocusClient,
                        OnFocusClient)
    IPC_MESSAGE_HANDLER(ServiceWorkerHostMsg_NavigateClient, OnNavigateClient)
    IPC_MESSAGE_HANDLER(ServiceWorkerHostMsg_SkipWaiting,
                        OnSkipWaiting)
    IPC_MESSAGE_HANDLER(ServiceWorkerHostMsg_ClaimClients,
                        OnClaimClients)
    IPC_MESSAGE_HANDLER(ServiceWorkerHostMsg_Pong, OnPongFromWorker)
    IPC_MESSAGE_HANDLER(ServiceWorkerHostMsg_RegisterForeignFetchScopes,
                        OnRegisterForeignFetchScopes)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void ServiceWorkerVersion::OnStartSentAndScriptEvaluated(
    ServiceWorkerStatusCode status) {
  if (status != SERVICE_WORKER_OK) {
    scoped_refptr<ServiceWorkerVersion> protect(this);
    RunCallbacks(this, &start_callbacks_,
                 DeduceStartWorkerFailureReason(status));
  }
}

void ServiceWorkerVersion::DispatchInstallEventAfterStartWorker(
    const StatusCallback& callback) {
  DCHECK_EQ(RUNNING, running_status())
      << "Worker stopped too soon after it was started.";

  int request_id = AddRequest(callback, &install_requests_, REQUEST_INSTALL);
  ServiceWorkerStatusCode status = embedded_worker_->SendMessage(
      ServiceWorkerMsg_InstallEvent(request_id));
  if (status != SERVICE_WORKER_OK) {
    install_requests_.Remove(request_id);
    RunSoon(base::Bind(callback, status));
  }
}

void ServiceWorkerVersion::DispatchActivateEventAfterStartWorker(
    const StatusCallback& callback) {
  DCHECK_EQ(RUNNING, running_status())
      << "Worker stopped too soon after it was started.";

  int request_id = AddRequest(callback, &activate_requests_, REQUEST_ACTIVATE);
  ServiceWorkerStatusCode status =
      embedded_worker_->SendMessage(ServiceWorkerMsg_ActivateEvent(request_id));
  if (status != SERVICE_WORKER_OK) {
    activate_requests_.Remove(request_id);
    RunSoon(base::Bind(callback, status));
  }
}

void ServiceWorkerVersion::OnGetClients(
    int request_id,
    const ServiceWorkerClientQueryOptions& options) {
  TRACE_EVENT_ASYNC_BEGIN2(
      "ServiceWorker", "ServiceWorkerVersion::OnGetClients", request_id,
      "client_type", options.client_type, "include_uncontrolled",
      options.include_uncontrolled);

  ServiceWorkerClients clients;
  if (controllee_map_.empty() && !options.include_uncontrolled) {
    OnGetClientsFinished(request_id, &clients);
    return;
  }

  // For Window clients we want to query the info on the UI thread first.
  if (options.client_type == blink::WebServiceWorkerClientTypeWindow ||
      options.client_type == blink::WebServiceWorkerClientTypeAll) {
    GetWindowClients(request_id, options);
    return;
  }

  GetNonWindowClients(request_id, options, &clients);
  OnGetClientsFinished(request_id, &clients);
}

void ServiceWorkerVersion::OnGetClientsFinished(int request_id,
                                                ServiceWorkerClients* clients) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  TRACE_EVENT_ASYNC_END1("ServiceWorker", "ServiceWorkerVersion::OnGetClients",
                         request_id, "The number of clients", clients->size());

  if (running_status() != RUNNING)
    return;
  // Sort clients so that the most recently active tab is in the front.
  std::sort(clients->begin(), clients->end(), ServiceWorkerClientInfoSortMRU());
  embedded_worker_->SendMessage(
      ServiceWorkerMsg_DidGetClients(request_id, *clients));
}

void ServiceWorkerVersion::OnActivateEventFinished(
    int request_id,
    blink::WebServiceWorkerEventResult result) {
  DCHECK(ACTIVATING == status() ||
         REDUNDANT == status()) << status();
  TRACE_EVENT0("ServiceWorker",
               "ServiceWorkerVersion::OnActivateEventFinished");

  PendingRequest<StatusCallback>* request =
      activate_requests_.Lookup(request_id);
  if (!request) {
    NOTREACHED() << "Got unexpected message: " << request_id;
    return;
  }
  ServiceWorkerStatusCode rv = SERVICE_WORKER_OK;
  if (result == blink::WebServiceWorkerEventResultRejected)
    rv = SERVICE_WORKER_ERROR_ACTIVATE_WORKER_FAILED;

  UMA_HISTOGRAM_MEDIUM_TIMES("ServiceWorker.ActivateEvent.Time",
                             base::TimeTicks::Now() - request->start_time);

  scoped_refptr<ServiceWorkerVersion> protect(this);
  request->callback.Run(rv);
  RemoveCallbackAndStopIfRedundant(&activate_requests_, request_id);
}

void ServiceWorkerVersion::OnInstallEventFinished(
    int request_id,
    blink::WebServiceWorkerEventResult result) {
  // Status is REDUNDANT if the worker was doomed while handling the install
  // event, and finished handling before being terminated.
  DCHECK(status() == INSTALLING || status() == REDUNDANT) << status();
  TRACE_EVENT0("ServiceWorker",
               "ServiceWorkerVersion::OnInstallEventFinished");

  PendingRequest<StatusCallback>* request =
      install_requests_.Lookup(request_id);
  if (!request) {
    NOTREACHED() << "Got unexpected message: " << request_id;
    return;
  }
  ServiceWorkerStatusCode status = SERVICE_WORKER_OK;
  if (result == blink::WebServiceWorkerEventResultRejected)
    status = SERVICE_WORKER_ERROR_INSTALL_WORKER_FAILED;

  UMA_HISTOGRAM_MEDIUM_TIMES("ServiceWorker.InstallEvent.Time",
                             base::TimeTicks::Now() - request->start_time);

  scoped_refptr<ServiceWorkerVersion> protect(this);
  request->callback.Run(status);
  RemoveCallbackAndStopIfRedundant(&install_requests_, request_id);
}

void ServiceWorkerVersion::OnFetchEventFinished(
    int request_id,
    ServiceWorkerFetchEventResult result,
    const ServiceWorkerResponse& response) {
  TRACE_EVENT1("ServiceWorker",
               "ServiceWorkerVersion::OnFetchEventFinished",
               "Request id", request_id);
  PendingRequest<FetchCallback>* request = fetch_requests_.Lookup(request_id);
  if (!request) {
    NOTREACHED() << "Got unexpected message: " << request_id;
    return;
  }

  // TODO(kinuko): Record other event statuses too.
  const bool handled = (result == SERVICE_WORKER_FETCH_EVENT_RESULT_RESPONSE);
  metrics_->RecordEventHandledStatus(ServiceWorkerMetrics::EVENT_TYPE_FETCH,
                                     handled);

  ServiceWorkerMetrics::RecordFetchEventTime(
      result, base::TimeTicks::Now() - request->start_time);

  scoped_refptr<ServiceWorkerVersion> protect(this);
  request->callback.Run(SERVICE_WORKER_OK, result, response);
  RemoveCallbackAndStopIfRedundant(&fetch_requests_, request_id);
}

void ServiceWorkerVersion::OnSyncEventFinished(
    int request_id,
    ServiceWorkerEventStatus status) {
  TRACE_EVENT1("ServiceWorker",
               "ServiceWorkerVersion::OnSyncEventFinished",
               "Request id", request_id);
  PendingRequest<StatusCallback>* request = sync_requests_.Lookup(request_id);
  if (!request) {
    NOTREACHED() << "Got unexpected message: " << request_id;
    return;
  }

  scoped_refptr<ServiceWorkerVersion> protect(this);
  request->callback.Run(mojo::ConvertTo<ServiceWorkerStatusCode>(status));
  RemoveCallbackAndStopIfRedundant(&sync_requests_, request_id);
}

void ServiceWorkerVersion::OnNotificationClickEventFinished(
    int request_id) {
  TRACE_EVENT1("ServiceWorker",
               "ServiceWorkerVersion::OnNotificationClickEventFinished",
               "Request id", request_id);
  PendingRequest<StatusCallback>* request =
      notification_click_requests_.Lookup(request_id);
  if (!request) {
    NOTREACHED() << "Got unexpected message: " << request_id;
    return;
  }

  UMA_HISTOGRAM_MEDIUM_TIMES("ServiceWorker.NotificationClickEvent.Time",
                             base::TimeTicks::Now() - request->start_time);

  scoped_refptr<ServiceWorkerVersion> protect(this);
  request->callback.Run(SERVICE_WORKER_OK);
  RemoveCallbackAndStopIfRedundant(&notification_click_requests_, request_id);
}

void ServiceWorkerVersion::OnPushEventFinished(
    int request_id,
    blink::WebServiceWorkerEventResult result) {
  TRACE_EVENT1("ServiceWorker",
               "ServiceWorkerVersion::OnPushEventFinished",
               "Request id", request_id);
  PendingRequest<StatusCallback>* request = push_requests_.Lookup(request_id);
  if (!request) {
    NOTREACHED() << "Got unexpected message: " << request_id;
    return;
  }
  ServiceWorkerStatusCode status = SERVICE_WORKER_OK;
  if (result == blink::WebServiceWorkerEventResultRejected)
    status = SERVICE_WORKER_ERROR_EVENT_WAITUNTIL_REJECTED;

  UMA_HISTOGRAM_MEDIUM_TIMES("ServiceWorker.PushEvent.Time",
                             base::TimeTicks::Now() - request->start_time);

  scoped_refptr<ServiceWorkerVersion> protect(this);
  request->callback.Run(status);
  RemoveCallbackAndStopIfRedundant(&push_requests_, request_id);
}

void ServiceWorkerVersion::OnGeofencingEventFinished(int request_id) {
  TRACE_EVENT1("ServiceWorker",
               "ServiceWorkerVersion::OnGeofencingEventFinished",
               "Request id",
               request_id);
  PendingRequest<StatusCallback>* request =
      geofencing_requests_.Lookup(request_id);
  if (!request) {
    NOTREACHED() << "Got unexpected message: " << request_id;
    return;
  }

  scoped_refptr<ServiceWorkerVersion> protect(this);
  request->callback.Run(SERVICE_WORKER_OK);
  RemoveCallbackAndStopIfRedundant(&geofencing_requests_, request_id);
}

void ServiceWorkerVersion::OnServicePortConnectEventFinished(
    int request_id,
    ServicePortConnectResult result,
    const mojo::String& name,
    const mojo::String& data) {
  TRACE_EVENT1("ServiceWorker",
               "ServiceWorkerVersion::OnServicePortConnectEventFinished",
               "Request id", request_id);
  PendingRequest<ServicePortConnectCallback>* request =
      service_port_connect_requests_.Lookup(request_id);
  if (!request) {
    NOTREACHED() << "Got unexpected message: " << request_id;
    return;
  }

  scoped_refptr<ServiceWorkerVersion> protect(this);
  request->callback.Run(SERVICE_WORKER_OK,
                        result == SERVICE_PORT_CONNECT_RESULT_ACCEPT,
                        name.To<base::string16>(), data.To<base::string16>());
  RemoveCallbackAndStopIfRedundant(&service_port_connect_requests_, request_id);
}

void ServiceWorkerVersion::OnOpenWindow(int request_id, GURL url) {
  // Just abort if we are shutting down.
  if (!context_)
    return;

  if (!url.is_valid()) {
    DVLOG(1) << "Received unexpected invalid URL from renderer process.";
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                            base::Bind(&KillEmbeddedWorkerProcess,
                                       embedded_worker_->process_id(),
                                       RESULT_CODE_KILLED_BAD_MESSAGE));
    return;
  }

  // The renderer treats all URLs in the about: scheme as being about:blank.
  // Canonicalize about: URLs to about:blank.
  if (url.SchemeIs(url::kAboutScheme))
    url = GURL(url::kAboutBlankURL);

  // Reject requests for URLs that the process is not allowed to access. It's
  // possible to receive such requests since the renderer-side checks are
  // slightly different. For example, the view-source scheme will not be
  // filtered out by Blink.
  if (!ChildProcessSecurityPolicyImpl::GetInstance()->CanRequestURL(
          embedded_worker_->process_id(), url)) {
    embedded_worker_->SendMessage(ServiceWorkerMsg_OpenWindowError(
        request_id, url.spec() + " cannot be opened."));
    return;
  }

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&OpenWindowOnUI,
                 url,
                 script_url_,
                 embedded_worker_->process_id(),
                 make_scoped_refptr(context_->wrapper()),
                 base::Bind(&ServiceWorkerVersion::DidOpenWindow,
                            weak_factory_.GetWeakPtr(),
                            request_id)));
}

void ServiceWorkerVersion::DidOpenWindow(int request_id,
                                         int render_process_id,
                                         int render_frame_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (running_status() != RUNNING)
    return;

  if (render_process_id == ChildProcessHost::kInvalidUniqueID &&
      render_frame_id == MSG_ROUTING_NONE) {
    embedded_worker_->SendMessage(ServiceWorkerMsg_OpenWindowError(
        request_id, "Something went wrong while trying to open the window."));
    return;
  }

  for (auto it =
           context_->GetClientProviderHostIterator(script_url_.GetOrigin());
       !it->IsAtEnd(); it->Advance()) {
    ServiceWorkerProviderHost* provider_host = it->GetProviderHost();
    if (provider_host->process_id() != render_process_id ||
        provider_host->frame_id() != render_frame_id) {
      continue;
    }
    provider_host->GetWindowClientInfo(base::Bind(
        &ServiceWorkerVersion::OnOpenWindowFinished, weak_factory_.GetWeakPtr(),
        request_id, provider_host->client_uuid()));
    return;
  }

  // If here, it means that no provider_host was found, in which case, the
  // renderer should still be informed that the window was opened.
  OnOpenWindowFinished(request_id, std::string(), ServiceWorkerClientInfo());
}

void ServiceWorkerVersion::OnOpenWindowFinished(
    int request_id,
    const std::string& client_uuid,
    const ServiceWorkerClientInfo& client_info) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (running_status() != RUNNING)
    return;

  ServiceWorkerClientInfo client(client_info);

  // If the |client_info| is empty, it means that the opened window wasn't
  // controlled but the action still succeeded. The renderer process is
  // expecting an empty client in such case.
  if (!client.IsEmpty())
    client.client_uuid = client_uuid;

  embedded_worker_->SendMessage(ServiceWorkerMsg_OpenWindowResponse(
      request_id, client));
}

void ServiceWorkerVersion::OnSetCachedMetadata(const GURL& url,
                                               const std::vector<char>& data) {
  int64 callback_id = base::TimeTicks::Now().ToInternalValue();
  TRACE_EVENT_ASYNC_BEGIN1("ServiceWorker",
                           "ServiceWorkerVersion::OnSetCachedMetadata",
                           callback_id, "URL", url.spec());
  script_cache_map_.WriteMetadata(
      url, data, base::Bind(&ServiceWorkerVersion::OnSetCachedMetadataFinished,
                            weak_factory_.GetWeakPtr(), callback_id));
}

void ServiceWorkerVersion::OnSetCachedMetadataFinished(int64 callback_id,
                                                       int result) {
  TRACE_EVENT_ASYNC_END1("ServiceWorker",
                         "ServiceWorkerVersion::OnSetCachedMetadata",
                         callback_id, "result", result);
  FOR_EACH_OBSERVER(Listener, listeners_, OnCachedMetadataUpdated(this));
}

void ServiceWorkerVersion::OnClearCachedMetadata(const GURL& url) {
  int64 callback_id = base::TimeTicks::Now().ToInternalValue();
  TRACE_EVENT_ASYNC_BEGIN1("ServiceWorker",
                           "ServiceWorkerVersion::OnClearCachedMetadata",
                           callback_id, "URL", url.spec());
  script_cache_map_.ClearMetadata(
      url, base::Bind(&ServiceWorkerVersion::OnClearCachedMetadataFinished,
                      weak_factory_.GetWeakPtr(), callback_id));
}

void ServiceWorkerVersion::OnClearCachedMetadataFinished(int64 callback_id,
                                                         int result) {
  TRACE_EVENT_ASYNC_END1("ServiceWorker",
                         "ServiceWorkerVersion::OnClearCachedMetadata",
                         callback_id, "result", result);
  FOR_EACH_OBSERVER(Listener, listeners_, OnCachedMetadataUpdated(this));
}

void ServiceWorkerVersion::OnPostMessageToClient(
    const std::string& client_uuid,
    const base::string16& message,
    const std::vector<TransferredMessagePort>& sent_message_ports) {
  if (!context_)
    return;
  TRACE_EVENT1("ServiceWorker",
               "ServiceWorkerVersion::OnPostMessageToDocument",
               "Client id", client_uuid);
  ServiceWorkerProviderHost* provider_host =
      context_->GetProviderHostByClientID(client_uuid);
  if (!provider_host) {
    // The client may already have been closed, just ignore.
    return;
  }
  if (provider_host->document_url().GetOrigin() != script_url_.GetOrigin()) {
    // The client does not belong to the same origin as this ServiceWorker,
    // possibly due to timing issue or bad message.
    return;
  }
  provider_host->PostMessage(this, message, sent_message_ports);
}

void ServiceWorkerVersion::OnFocusClient(int request_id,
                                         const std::string& client_uuid) {
  if (!context_)
    return;
  TRACE_EVENT2("ServiceWorker",
               "ServiceWorkerVersion::OnFocusClient",
               "Request id", request_id,
               "Client id", client_uuid);
  ServiceWorkerProviderHost* provider_host =
      context_->GetProviderHostByClientID(client_uuid);
  if (!provider_host) {
    // The client may already have been closed, just ignore.
    return;
  }
  if (provider_host->document_url().GetOrigin() != script_url_.GetOrigin()) {
    // The client does not belong to the same origin as this ServiceWorker,
    // possibly due to timing issue or bad message.
    return;
  }
  provider_host->Focus(base::Bind(&ServiceWorkerVersion::OnFocusClientFinished,
                                  weak_factory_.GetWeakPtr(), request_id,
                                  client_uuid));
}

void ServiceWorkerVersion::OnFocusClientFinished(
    int request_id,
    const std::string& client_uuid,
    const ServiceWorkerClientInfo& client) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (running_status() != RUNNING)
    return;

  ServiceWorkerClientInfo client_info(client);
  client_info.client_uuid = client_uuid;

  embedded_worker_->SendMessage(ServiceWorkerMsg_FocusClientResponse(
      request_id, client_info));
}

void ServiceWorkerVersion::OnNavigateClient(int request_id,
                                            const std::string& client_uuid,
                                            const GURL& url) {
  if (!context_)
    return;

  TRACE_EVENT2("ServiceWorker", "ServiceWorkerVersion::OnNavigateClient",
               "Request id", request_id, "Client id", client_uuid);

  if (!url.is_valid() || !base::IsValidGUID(client_uuid)) {
    DVLOG(1) << "Received unexpected invalid URL/UUID from renderer process.";
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&KillEmbeddedWorkerProcess, embedded_worker_->process_id(),
                   RESULT_CODE_KILLED_BAD_MESSAGE));
    return;
  }

  // Reject requests for URLs that the process is not allowed to access. It's
  // possible to receive such requests since the renderer-side checks are
  // slightly different. For example, the view-source scheme will not be
  // filtered out by Blink.
  if (!ChildProcessSecurityPolicyImpl::GetInstance()->CanRequestURL(
          embedded_worker_->process_id(), url)) {
    embedded_worker_->SendMessage(
        ServiceWorkerMsg_NavigateClientError(request_id, url));
    return;
  }

  ServiceWorkerProviderHost* provider_host =
      context_->GetProviderHostByClientID(client_uuid);
  if (!provider_host || provider_host->active_version() != this) {
    embedded_worker_->SendMessage(
        ServiceWorkerMsg_NavigateClientError(request_id, url));
    return;
  }

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&NavigateClientOnUI, url, script_url_,
                 provider_host->process_id(), provider_host->frame_id(),
                 base::Bind(&ServiceWorkerVersion::DidNavigateClient,
                            weak_factory_.GetWeakPtr(), request_id)));
}

void ServiceWorkerVersion::DidNavigateClient(int request_id,
                                             int render_process_id,
                                             int render_frame_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (running_status() != RUNNING)
    return;

  if (render_process_id == ChildProcessHost::kInvalidUniqueID &&
      render_frame_id == MSG_ROUTING_NONE) {
    embedded_worker_->SendMessage(
        ServiceWorkerMsg_NavigateClientError(request_id, GURL()));
    return;
  }

  for (auto it =
           context_->GetClientProviderHostIterator(script_url_.GetOrigin());
       !it->IsAtEnd(); it->Advance()) {
    ServiceWorkerProviderHost* provider_host = it->GetProviderHost();
    if (provider_host->process_id() != render_process_id ||
        provider_host->frame_id() != render_frame_id) {
      continue;
    }
    provider_host->GetWindowClientInfo(base::Bind(
        &ServiceWorkerVersion::OnNavigateClientFinished,
        weak_factory_.GetWeakPtr(), request_id, provider_host->client_uuid()));
    return;
  }

  OnNavigateClientFinished(request_id, std::string(),
                           ServiceWorkerClientInfo());
}

void ServiceWorkerVersion::OnNavigateClientFinished(
    int request_id,
    const std::string& client_uuid,
    const ServiceWorkerClientInfo& client_info) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (running_status() != RUNNING)
    return;

  ServiceWorkerClientInfo client(client_info);

  // If the |client_info| is empty, it means that the navigated client wasn't
  // controlled but the action still succeeded. The renderer process is
  // expecting an empty client in such case.
  if (!client.IsEmpty())
    client.client_uuid = client_uuid;

  embedded_worker_->SendMessage(
      ServiceWorkerMsg_NavigateClientResponse(request_id, client));
}

void ServiceWorkerVersion::OnSkipWaiting(int request_id) {
  skip_waiting_ = true;
  if (status_ != INSTALLED)
    return DidSkipWaiting(request_id);

  if (!context_)
    return;
  ServiceWorkerRegistration* registration =
      context_->GetLiveRegistration(registration_id_);
  if (!registration)
    return;
  pending_skip_waiting_requests_.push_back(request_id);
  if (pending_skip_waiting_requests_.size() == 1)
    registration->ActivateWaitingVersionWhenReady();
}

void ServiceWorkerVersion::DidSkipWaiting(int request_id) {
  if (running_status() == STARTING || running_status() == RUNNING)
    embedded_worker_->SendMessage(ServiceWorkerMsg_DidSkipWaiting(request_id));
}

void ServiceWorkerVersion::OnClaimClients(int request_id) {
  if (status_ != ACTIVATING && status_ != ACTIVATED) {
    embedded_worker_->SendMessage(ServiceWorkerMsg_ClaimClientsError(
        request_id, blink::WebServiceWorkerError::ErrorTypeState,
        base::ASCIIToUTF16(kClaimClientsStateErrorMesage)));
    return;
  }
  if (context_) {
    if (ServiceWorkerRegistration* registration =
            context_->GetLiveRegistration(registration_id_)) {
      registration->ClaimClients();
      embedded_worker_->SendMessage(
          ServiceWorkerMsg_DidClaimClients(request_id));
      return;
    }
  }

  embedded_worker_->SendMessage(ServiceWorkerMsg_ClaimClientsError(
      request_id, blink::WebServiceWorkerError::ErrorTypeAbort,
      base::ASCIIToUTF16(kClaimClientsShutdownErrorMesage)));
}

void ServiceWorkerVersion::OnPongFromWorker() {
  ping_controller_->OnPongReceived();
}

void ServiceWorkerVersion::OnRegisterForeignFetchScopes(
    const std::vector<GURL>& sub_scopes) {
  DCHECK(status() == INSTALLING || status() == REDUNDANT) << status();
  // Renderer should have already verified all these urls are inside the
  // worker's scope, but verify again here on the browser process side.
  GURL origin = scope_.GetOrigin();
  std::string scope_path = scope_.path();
  for (const GURL& url : sub_scopes) {
    if (!url.is_valid() || url.GetOrigin() != origin ||
        !base::StartsWith(url.path(), scope_path,
                          base::CompareCase::SENSITIVE)) {
      DVLOG(1) << "Received unexpected invalid URL from renderer process.";
      BrowserThread::PostTask(
          BrowserThread::UI, FROM_HERE,
          base::Bind(&KillEmbeddedWorkerProcess, embedded_worker_->process_id(),
                     RESULT_CODE_KILLED_BAD_MESSAGE));
      return;
    }
  }
  foreign_fetch_scopes_.insert(foreign_fetch_scopes_.end(), sub_scopes.begin(),
                               sub_scopes.end());
}

void ServiceWorkerVersion::DidEnsureLiveRegistrationForStartWorker(
    const StatusCallback& callback,
    ServiceWorkerStatusCode status,
    const scoped_refptr<ServiceWorkerRegistration>& protect) {
  if (status != SERVICE_WORKER_OK) {
    RecordStartWorkerResult(status);
    RunSoon(base::Bind(callback, SERVICE_WORKER_ERROR_START_WORKER_FAILED));
    return;
  }
  if (is_redundant()) {
    RecordStartWorkerResult(SERVICE_WORKER_ERROR_REDUNDANT);
    RunSoon(base::Bind(callback, SERVICE_WORKER_ERROR_REDUNDANT));
    return;
  }

  MarkIfStale();

  switch (running_status()) {
    case RUNNING:
      RunSoon(base::Bind(callback, SERVICE_WORKER_OK));
      return;
    case STOPPING:
    case STOPPED:
    case STARTING:
      if (start_callbacks_.empty()) {
        start_callbacks_.push_back(
            base::Bind(&ServiceWorkerVersion::RecordStartWorkerResult,
                       weak_factory_.GetWeakPtr()));
      }
      // Keep the live registration while starting the worker.
      start_callbacks_.push_back(
          base::Bind(&RunStartWorkerCallback, callback, protect));
      StartWorkerInternal();
      return;
  }
}

void ServiceWorkerVersion::StartWorkerInternal() {
  if (!metrics_)
    metrics_.reset(new Metrics(this));

  if (!timeout_timer_.IsRunning())
    StartTimeoutTimer();
  if (running_status() == STOPPED) {
    embedded_worker_->Start(
        version_id_, scope_, script_url_,
        base::Bind(&ServiceWorkerVersion::OnStartSentAndScriptEvaluated,
                   weak_factory_.GetWeakPtr()));
  }
}

void ServiceWorkerVersion::GetWindowClients(
    int request_id,
    const ServiceWorkerClientQueryOptions& options) {
  DCHECK(options.client_type == blink::WebServiceWorkerClientTypeWindow ||
         options.client_type == blink::WebServiceWorkerClientTypeAll);

  std::vector<base::Tuple<int, int, std::string>> clients_info;
  if (!options.include_uncontrolled) {
    for (auto& controllee : controllee_map_)
      AddWindowClient(controllee.second, &clients_info);
  } else {
    for (auto it =
             context_->GetClientProviderHostIterator(script_url_.GetOrigin());
         !it->IsAtEnd(); it->Advance()) {
      AddWindowClient(it->GetProviderHost(), &clients_info);
    }
  }

  if (clients_info.empty()) {
    DidGetWindowClients(request_id, options,
                        make_scoped_ptr(new ServiceWorkerClients));
    return;
  }

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&OnGetWindowClientsFromUI, clients_info, script_url_,
                 base::Bind(&ServiceWorkerVersion::DidGetWindowClients,
                            weak_factory_.GetWeakPtr(), request_id, options)));
}

void ServiceWorkerVersion::DidGetWindowClients(
    int request_id,
    const ServiceWorkerClientQueryOptions& options,
    scoped_ptr<ServiceWorkerClients> clients) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (options.client_type == blink::WebServiceWorkerClientTypeAll)
    GetNonWindowClients(request_id, options, clients.get());
  OnGetClientsFinished(request_id, clients.get());
}

void ServiceWorkerVersion::GetNonWindowClients(
    int request_id,
    const ServiceWorkerClientQueryOptions& options,
    ServiceWorkerClients* clients) {
  if (!options.include_uncontrolled) {
    for (auto& controllee : controllee_map_) {
      AddNonWindowClient(controllee.second, options, clients);
    }
  } else {
    for (auto it =
             context_->GetClientProviderHostIterator(script_url_.GetOrigin());
         !it->IsAtEnd(); it->Advance()) {
      AddNonWindowClient(it->GetProviderHost(), options, clients);
    }
  }
}

void ServiceWorkerVersion::StartTimeoutTimer() {
  DCHECK(!timeout_timer_.IsRunning());

  if (embedded_worker_->devtools_attached()) {
    // Don't record the startup time metric once DevTools is attached.
    ClearTick(&start_time_);
    skip_recording_startup_time_ = true;
  } else {
    RestartTick(&start_time_);
    skip_recording_startup_time_ = false;
  }

  // The worker is starting up and not yet idle.
  ClearTick(&idle_time_);

  // Ping will be activated in OnScriptLoaded.
  ping_controller_->Deactivate();

  timeout_timer_.Start(FROM_HERE,
                       base::TimeDelta::FromSeconds(kTimeoutTimerDelaySeconds),
                       this, &ServiceWorkerVersion::OnTimeoutTimer);
}

void ServiceWorkerVersion::StopTimeoutTimer() {
  timeout_timer_.Stop();
  ClearTick(&idle_time_);

  // Trigger update if worker is stale.
  if (!in_dtor_ && !stale_time_.is_null()) {
    ClearTick(&stale_time_);
    if (!update_timer_.IsRunning())
      ScheduleUpdate();
  }
}

void ServiceWorkerVersion::OnTimeoutTimer() {
  DCHECK(running_status() == STARTING || running_status() == RUNNING ||
         running_status() == STOPPING)
      << running_status();

  MarkIfStale();

  // Stopping the worker hasn't finished within a certain period.
  if (GetTickDuration(stop_time_) >
      base::TimeDelta::FromSeconds(kStopWorkerTimeoutSeconds)) {
    DCHECK_EQ(STOPPING, running_status());
    if (IsInstalled(status())) {
      ServiceWorkerMetrics::RecordWorkerStopped(
          ServiceWorkerMetrics::StopStatus::TIMEOUT);
    }
    ReportError(SERVICE_WORKER_ERROR_TIMEOUT, "DETACH_STALLED_IN_STOPPING");

    // Detach the worker. Remove |this| as a listener first; otherwise
    // OnStoppedInternal might try to restart before the new worker
    // is created.
    embedded_worker_->RemoveListener(this);
    embedded_worker_->Detach();
    embedded_worker_ = context_->embedded_worker_registry()->CreateWorker();
    embedded_worker_->AddListener(this);

    // Call OnStoppedInternal to fail callbacks and possibly restart.
    OnStoppedInternal(EmbeddedWorkerInstance::STOPPING);
    return;
  }

  // Trigger update if worker is stale and we waited long enough for it to go
  // idle.
  if (GetTickDuration(stale_time_) >
      base::TimeDelta::FromMinutes(kRequestTimeoutMinutes)) {
    ClearTick(&stale_time_);
    if (!update_timer_.IsRunning())
      ScheduleUpdate();
  }

  // Starting a worker hasn't finished within a certain period.
  if (GetTickDuration(start_time_) >
      base::TimeDelta::FromMinutes(kStartWorkerTimeoutMinutes)) {
    DCHECK(running_status() == STARTING || running_status() == STOPPING)
        << running_status();
    scoped_refptr<ServiceWorkerVersion> protect(this);
    RunCallbacks(this, &start_callbacks_, SERVICE_WORKER_ERROR_TIMEOUT);
    if (running_status() == STARTING)
      embedded_worker_->Stop();
    return;
  }

  // Requests have not finished within a certain period.
  bool request_timed_out = false;
  while (!requests_.empty()) {
    RequestInfo info = requests_.front();
    if (GetTickDuration(info.time) <
        base::TimeDelta::FromMinutes(kRequestTimeoutMinutes))
      break;
    if (MaybeTimeOutRequest(info)) {
      request_timed_out = true;
      UMA_HISTOGRAM_ENUMERATION("ServiceWorker.RequestTimeouts.Count",
                                info.type, NUM_REQUEST_TYPES);
    }
    requests_.pop();
  }
  if (request_timed_out && running_status() != STOPPING)
    embedded_worker_->Stop();

  // For the timeouts below, there are no callbacks to timeout so there is
  // nothing more to do if the worker is already stopping.
  if (running_status() == STOPPING)
    return;

  // The worker has been idle for longer than a certain period.
  if (GetTickDuration(idle_time_) >
      base::TimeDelta::FromSeconds(kIdleWorkerTimeoutSeconds)) {
    StopWorkerIfIdle();
    return;
  }

  // Check ping status.
  ping_controller_->CheckPingStatus();
}

ServiceWorkerStatusCode ServiceWorkerVersion::PingWorker() {
  DCHECK(running_status() == STARTING || running_status() == RUNNING);
  return embedded_worker_->SendMessage(ServiceWorkerMsg_Ping());
}

void ServiceWorkerVersion::OnPingTimeout() {
  DCHECK(running_status() == STARTING || running_status() == RUNNING);
  // TODO(falken): Show a message to the developer that the SW was stopped due
  // to timeout (crbug.com/457968). Also, change the error code to
  // SERVICE_WORKER_ERROR_TIMEOUT.
  StopWorkerIfIdle();
}

void ServiceWorkerVersion::StopWorkerIfIdle() {
  if (HasInflightRequests() && !ping_controller_->IsTimedOut())
    return;
  if (running_status() == STOPPED || running_status() == STOPPING ||
      !stop_callbacks_.empty()) {
    return;
  }

  embedded_worker_->StopIfIdle();
}

bool ServiceWorkerVersion::HasInflightRequests() const {
  return !activate_requests_.IsEmpty() || !install_requests_.IsEmpty() ||
         !fetch_requests_.IsEmpty() || !sync_requests_.IsEmpty() ||
         !notification_click_requests_.IsEmpty() || !push_requests_.IsEmpty() ||
         !geofencing_requests_.IsEmpty() ||
         !service_port_connect_requests_.IsEmpty() ||
         !streaming_url_request_jobs_.empty();
}

void ServiceWorkerVersion::RecordStartWorkerResult(
    ServiceWorkerStatusCode status) {
  base::TimeTicks start_time = start_time_;
  ClearTick(&start_time_);

  ServiceWorkerMetrics::RecordStartWorkerStatus(status,
                                                IsInstalled(prestart_status_));

  if (status == SERVICE_WORKER_OK && !start_time.is_null() &&
      !skip_recording_startup_time_) {
    ServiceWorkerMetrics::RecordStartWorkerTime(GetTickDuration(start_time),
                                                IsInstalled(prestart_status_));
  }

  if (status != SERVICE_WORKER_ERROR_TIMEOUT)
    return;
  EmbeddedWorkerInstance::StartingPhase phase =
      EmbeddedWorkerInstance::NOT_STARTING;
  EmbeddedWorkerInstance::Status running_status = embedded_worker_->status();
  // Build an artifical JavaScript exception to show in the ServiceWorker
  // log for developers; it's not user-facing so it's not a localized resource.
  std::string message = "ServiceWorker startup timed out. ";
  if (running_status != EmbeddedWorkerInstance::STARTING) {
    message.append("The worker had unexpected status: ");
    message.append(EmbeddedWorkerInstance::StatusToString(running_status));
  } else {
    phase = embedded_worker_->starting_phase();
    message.append("The worker was in startup phase: ");
    message.append(EmbeddedWorkerInstance::StartingPhaseToString(phase));
  }
  message.append(".");
  OnReportException(base::UTF8ToUTF16(message), -1, -1, GURL());
  DVLOG(1) << message;
  UMA_HISTOGRAM_ENUMERATION("ServiceWorker.StartWorker.TimeoutPhase",
                            phase,
                            EmbeddedWorkerInstance::STARTING_PHASE_MAX_VALUE);
}

template <typename IDMAP>
void ServiceWorkerVersion::RemoveCallbackAndStopIfRedundant(IDMAP* callbacks,
                                                            int request_id) {
  RestartTick(&idle_time_);
  callbacks->Remove(request_id);
  if (is_redundant()) {
    // The stop should be already scheduled, but try to stop immediately, in
    // order to release worker resources soon.
    StopWorkerIfIdle();
  }
}

template <typename CallbackType>
int ServiceWorkerVersion::AddRequest(
    const CallbackType& callback,
    IDMap<PendingRequest<CallbackType>, IDMapOwnPointer>* callback_map,
    RequestType request_type) {
  base::TimeTicks now = base::TimeTicks::Now();
  int request_id =
      callback_map->Add(new PendingRequest<CallbackType>(callback, now));
  requests_.push(RequestInfo(request_id, request_type, now));
  return request_id;
}

bool ServiceWorkerVersion::MaybeTimeOutRequest(const RequestInfo& info) {
  switch (info.type) {
    case REQUEST_ACTIVATE:
      return RunIDMapCallback(&activate_requests_, info.id,
                              SERVICE_WORKER_ERROR_TIMEOUT);
    case REQUEST_INSTALL:
      return RunIDMapCallback(&install_requests_, info.id,
                              SERVICE_WORKER_ERROR_TIMEOUT);
    case REQUEST_FETCH:
      return RunIDMapCallback(
          &fetch_requests_, info.id, SERVICE_WORKER_ERROR_TIMEOUT,
          /* The other args are ignored for non-OK status. */
          SERVICE_WORKER_FETCH_EVENT_RESULT_FALLBACK, ServiceWorkerResponse());
    case REQUEST_SYNC:
      return RunIDMapCallback(&sync_requests_, info.id,
                              SERVICE_WORKER_ERROR_TIMEOUT);
    case REQUEST_NOTIFICATION_CLICK:
      return RunIDMapCallback(&notification_click_requests_, info.id,
                              SERVICE_WORKER_ERROR_TIMEOUT);
    case REQUEST_PUSH:
      return RunIDMapCallback(&push_requests_, info.id,
                              SERVICE_WORKER_ERROR_TIMEOUT);
    case REQUEST_GEOFENCING:
      return RunIDMapCallback(&geofencing_requests_, info.id,
                              SERVICE_WORKER_ERROR_TIMEOUT);
    case REQUEST_SERVICE_PORT_CONNECT:
      return RunIDMapCallback(&service_port_connect_requests_, info.id,
                              SERVICE_WORKER_ERROR_TIMEOUT,
                              false /* accept_connection */, base::string16(),
                              base::string16());
    case NUM_REQUEST_TYPES:
      break;
  }
  NOTREACHED() << "Got unexpected request type: " << info.type;
  return false;
}

void ServiceWorkerVersion::SetAllRequestTimes(const base::TimeTicks& ticks) {
  std::queue<RequestInfo> new_requests;
  while (!requests_.empty()) {
    RequestInfo info = requests_.front();
    info.time = ticks;
    new_requests.push(info);
    requests_.pop();
  }
  requests_ = new_requests;
}

ServiceWorkerStatusCode ServiceWorkerVersion::DeduceStartWorkerFailureReason(
    ServiceWorkerStatusCode default_code) {
  if (ping_controller_->IsTimedOut())
    return SERVICE_WORKER_ERROR_TIMEOUT;

  if (start_worker_status_ != SERVICE_WORKER_OK)
    return start_worker_status_;

  const net::URLRequestStatus& main_script_status =
      script_cache_map()->main_script_status();
  if (main_script_status.status() != net::URLRequestStatus::SUCCESS) {
    switch (main_script_status.error()) {
      case net::ERR_INSECURE_RESPONSE:
      case net::ERR_UNSAFE_REDIRECT:
        return SERVICE_WORKER_ERROR_SECURITY;
      case net::ERR_ABORTED:
        return SERVICE_WORKER_ERROR_ABORT;
      default:
        return SERVICE_WORKER_ERROR_NETWORK;
    }
  }

  return default_code;
}

void ServiceWorkerVersion::MarkIfStale() {
  if (!context_)
    return;
  if (update_timer_.IsRunning() || !stale_time_.is_null())
    return;
  ServiceWorkerRegistration* registration =
      context_->GetLiveRegistration(registration_id_);
  if (!registration || registration->active_version() != this)
    return;
  base::TimeDelta time_since_last_check =
      base::Time::Now() - registration->last_update_check();
  if (time_since_last_check >
      base::TimeDelta::FromHours(kServiceWorkerScriptMaxCacheAgeInHours))
    RestartTick(&stale_time_);
}

void ServiceWorkerVersion::FoundRegistrationForUpdate(
    ServiceWorkerStatusCode status,
    const scoped_refptr<ServiceWorkerRegistration>& registration) {
  if (!context_)
    return;

  const scoped_refptr<ServiceWorkerVersion> protect = this;
  if (is_update_scheduled_) {
    context_->UnprotectVersion(version_id_);
    is_update_scheduled_ = false;
  }

  if (status != SERVICE_WORKER_OK || registration->active_version() != this)
    return;
  context_->UpdateServiceWorker(registration.get(),
                                false /* force_bypass_cache */);
}

void ServiceWorkerVersion::OnStoppedInternal(
    EmbeddedWorkerInstance::Status old_status) {
  DCHECK_EQ(STOPPED, running_status());
  scoped_refptr<ServiceWorkerVersion> protect;
  if (!in_dtor_)
    protect = this;

  DCHECK(metrics_);
  metrics_.reset();

  bool should_restart = !is_redundant() && !start_callbacks_.empty() &&
                        (old_status != EmbeddedWorkerInstance::STARTING) &&
                        !in_dtor_ && !ping_controller_->IsTimedOut();

  ClearTick(&stop_time_);
  StopTimeoutTimer();

  // Fire all stop callbacks.
  RunCallbacks(this, &stop_callbacks_, SERVICE_WORKER_OK);

  if (!should_restart) {
    // Let all start callbacks fail.
    RunCallbacks(this, &start_callbacks_,
                 DeduceStartWorkerFailureReason(
                     SERVICE_WORKER_ERROR_START_WORKER_FAILED));
  }

  // Let all message callbacks fail (this will also fire and clear all
  // callbacks for events).
  // TODO(kinuko): Consider if we want to add queue+resend mechanism here.
  RunIDMapCallbacks(&activate_requests_,
                    SERVICE_WORKER_ERROR_ACTIVATE_WORKER_FAILED);
  RunIDMapCallbacks(&install_requests_,
                    SERVICE_WORKER_ERROR_INSTALL_WORKER_FAILED);
  RunIDMapCallbacks(&fetch_requests_, SERVICE_WORKER_ERROR_FAILED,
                    SERVICE_WORKER_FETCH_EVENT_RESULT_FALLBACK,
                    ServiceWorkerResponse());
  RunIDMapCallbacks(&sync_requests_, SERVICE_WORKER_ERROR_FAILED);
  RunIDMapCallbacks(&notification_click_requests_, SERVICE_WORKER_ERROR_FAILED);
  RunIDMapCallbacks(&push_requests_, SERVICE_WORKER_ERROR_FAILED);
  RunIDMapCallbacks(&geofencing_requests_, SERVICE_WORKER_ERROR_FAILED);

  // Close all mojo services. This will also fire and clear all callbacks
  // for messages that are still outstanding for those services.
  OnServicePortDispatcherConnectionError();

  OnBackgroundSyncDispatcherConnectionError();

  // TODO(falken): Call SWURLRequestJob::ClearStream here?
  streaming_url_request_jobs_.clear();

  FOR_EACH_OBSERVER(Listener, listeners_, OnRunningStateChanged(this));

  if (should_restart)
    StartWorkerInternal();
}

void ServiceWorkerVersion::OnServicePortDispatcherConnectionError() {
  RunIDMapCallbacks(&service_port_connect_requests_,
                    SERVICE_WORKER_ERROR_FAILED, false, base::string16(),
                    base::string16());
  service_port_dispatcher_.reset();
}

void ServiceWorkerVersion::OnBackgroundSyncDispatcherConnectionError() {
  RunIDMapCallbacks(&sync_requests_, SERVICE_WORKER_ERROR_FAILED);
  background_sync_dispatcher_.reset();
}

void ServiceWorkerVersion::OnBeginEvent() {
  if (should_exclude_from_uma_ || running_status() != RUNNING ||
      idle_time_.is_null()) {
    return;
  }
  ServiceWorkerMetrics::RecordTimeBetweenEvents(base::TimeTicks::Now() -
                                                idle_time_);
}

}  // namespace content
