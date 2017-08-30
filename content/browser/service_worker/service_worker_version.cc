// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_version.h"

#include <stddef.h>

#include <limits>
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
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/default_clock.h"
#include "base/time/default_tick_clock.h"
#include "content/browser/bad_message.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/service_worker/embedded_worker_registry.h"
#include "content/browser/service_worker/service_worker_client_utils.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/service_worker/service_worker_installed_scripts_sender.h"
#include "content/browser/service_worker/service_worker_registration.h"
#include "content/common/origin_trials/trial_token_validator.h"
#include "content/common/service_worker/embedded_worker_messages.h"
#include "content/common/service_worker/embedded_worker_start_params.h"
#include "content/common/service_worker/service_worker_messages.h"
#include "content/common/service_worker/service_worker_utils.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/browser_side_navigation_policy.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/common/result_codes.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_response_info.h"
#include "third_party/WebKit/public/platform/modules/serviceworker/service_worker_error_type.mojom.h"
#include "third_party/WebKit/public/web/WebConsoleMessage.h"

namespace content {

using StatusCallback = ServiceWorkerVersion::StatusCallback;

namespace {

// Timeout for an installed worker to start.
constexpr base::TimeDelta kStartInstalledWorkerTimeout =
    base::TimeDelta::FromSeconds(60);

// Timeout for a request to be handled.
constexpr base::TimeDelta kRequestTimeout = base::TimeDelta::FromMinutes(5);

// Time to wait until stopping an idle worker.
constexpr base::TimeDelta kIdleWorkerTimeout = base::TimeDelta::FromSeconds(30);

// Timeout for waiting for a response to a ping.
constexpr base::TimeDelta kPingTimeout = base::TimeDelta::FromSeconds(30);

// Default delay for scheduled update.
constexpr base::TimeDelta kUpdateDelay = base::TimeDelta::FromSeconds(1);

const char kClaimClientsStateErrorMesage[] =
    "Only the active worker can claim clients.";

const char kClaimClientsShutdownErrorMesage[] =
    "Failed to claim clients due to Service Worker system shutdown.";

const char kNotRespondingErrorMesage[] = "Service Worker is not responding.";
const char kForceUpdateInfoMessage[] =
    "Service Worker was updated because \"Update on reload\" was "
    "checked in the DevTools Application panel.";

void RunSoon(base::OnceClosure callback) {
  if (!callback.is_null())
    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                  std::move(callback));
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

void RunStartWorkerCallback(
    const StatusCallback& callback,
    scoped_refptr<ServiceWorkerRegistration> protect,
    ServiceWorkerStatusCode status) {
  callback.Run(status);
}

// A callback adapter to start a |task| after StartWorker.
void RunTaskAfterStartWorker(base::WeakPtr<ServiceWorkerVersion> version,
                             const StatusCallback& error_callback,
                             base::OnceClosure task,
                             ServiceWorkerStatusCode status) {
  if (status != SERVICE_WORKER_OK) {
    if (!error_callback.is_null())
      error_callback.Run(status);
    return;
  }
  if (version->running_status() != EmbeddedWorkerStatus::RUNNING) {
    // We've tried to start the worker (and it has succeeded), but
    // it looks it's not running yet.
    NOTREACHED() << "The worker's not running after successful StartWorker";
    if (!error_callback.is_null())
      error_callback.Run(SERVICE_WORKER_ERROR_START_WORKER_FAILED);
    return;
  }
  std::move(task).Run();
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

std::string VersionStatusToString(ServiceWorkerVersion::Status status) {
  switch (status) {
    case ServiceWorkerVersion::NEW:
      return "new";
    case ServiceWorkerVersion::INSTALLING:
      return "installing";
    case ServiceWorkerVersion::INSTALLED:
      return "installed";
    case ServiceWorkerVersion::ACTIVATING:
      return "activating";
    case ServiceWorkerVersion::ACTIVATED:
      return "activated";
    case ServiceWorkerVersion::REDUNDANT:
      return "redundant";
  }
  NOTREACHED() << status;
  return std::string();
}

const int kInvalidTraceId = -1;

int NextTraceId() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  static int trace_id = 0;
  if (trace_id == std::numeric_limits<int>::max())
    trace_id = 0;
  else
    ++trace_id;
  DCHECK_NE(kInvalidTraceId, trace_id);
  return trace_id;
}

void OnEventDispatcherConnectionError(
    base::WeakPtr<EmbeddedWorkerInstance> embedded_worker) {
  if (!embedded_worker)
    return;

  switch (embedded_worker->status()) {
    case EmbeddedWorkerStatus::STARTING:
    case EmbeddedWorkerStatus::RUNNING:
      // In this case the disconnection might be happening because of sudden
      // renderer shutdown like crash.
      embedded_worker->Detach();
      break;
    case EmbeddedWorkerStatus::STOPPING:
    case EmbeddedWorkerStatus::STOPPED:
      // Do nothing
      break;
  }
}

mojom::ServiceWorkerProviderInfoForStartWorkerPtr
CompleteProviderHostPreparation(
    ServiceWorkerVersion* version,
    std::unique_ptr<ServiceWorkerProviderHost> provider_host,
    base::WeakPtr<ServiceWorkerContextCore> context,
    int process_id) {
  // Caller should ensure |context| is alive when completing StartWorker
  // preparation.
  DCHECK(context);
  auto info =
      provider_host->CompleteStartWorkerPreparation(process_id, version);
  context->AddProviderHost(std::move(provider_host));
  return info;
}

}  // namespace

constexpr base::TimeDelta ServiceWorkerVersion::kTimeoutTimerDelay;
constexpr base::TimeDelta ServiceWorkerVersion::kStartNewWorkerTimeout;
constexpr base::TimeDelta ServiceWorkerVersion::kStopWorkerTimeout;

void ServiceWorkerVersion::RestartTick(base::TimeTicks* time) const {
  *time = tick_clock_->NowTicks();
}

bool ServiceWorkerVersion::RequestExpired(
    const base::TimeTicks& expiration) const {
  if (expiration.is_null())
    return false;
  return tick_clock_->NowTicks() >= expiration;
}

base::TimeDelta ServiceWorkerVersion::GetTickDuration(
    const base::TimeTicks& time) const {
  if (time.is_null())
    return base::TimeDelta();
  return tick_clock_->NowTicks() - time;
}

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
    if (version_->GetTickDuration(ping_time_) > kPingTimeout) {
      ping_state_ = PING_TIMED_OUT;
      version_->OnPingTimeout();
      return;
    }

    // Check if we want to send a next ping.
    if (ping_state_ != PINGING || !ping_time_.is_null())
      return;

    version_->PingWorker();
    version_->RestartTick(&ping_time_);
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
    int64_t version_id,
    base::WeakPtr<ServiceWorkerContextCore> context)
    : version_id_(version_id),
      registration_id_(registration->id()),
      script_url_(script_url),
      scope_(registration->pattern()),
      fetch_handler_existence_(FetchHandlerExistence::UNKNOWN),
      site_for_uma_(ServiceWorkerMetrics::SiteFromURL(scope_)),
      context_(context),
      script_cache_map_(this, context),
      tick_clock_(base::MakeUnique<base::DefaultTickClock>()),
      clock_(base::MakeUnique<base::DefaultClock>()),
      ping_controller_(new PingController(this)),
      weak_factory_(this) {
  DCHECK_NE(kInvalidServiceWorkerVersionId, version_id);
  DCHECK(context_);
  DCHECK(registration);
  DCHECK(script_url_.is_valid());
  embedded_worker_ = context_->embedded_worker_registry()->CreateWorker();
  embedded_worker_->AddListener(this);
  context_->AddLiveVersion(this);
}

ServiceWorkerVersion::~ServiceWorkerVersion() {
  in_dtor_ = true;

  // Record UMA if the worker was trying to start. One way we get here is if the
  // user closed the tab before the SW could start up.
  if (!start_callbacks_.empty()) {
    // RecordStartWorkerResult must be the first element of start_callbacks_.
    StatusCallback record_start_worker_result = start_callbacks_[0];
    start_callbacks_.clear();
    record_start_worker_result.Run(SERVICE_WORKER_ERROR_ABORT);
  }

  if (context_)
    context_->RemoveLiveVersion(version_id_);

  if (running_status() == EmbeddedWorkerStatus::STARTING ||
      running_status() == EmbeddedWorkerStatus::RUNNING) {
    embedded_worker_->Stop();
  }
  embedded_worker_->RemoveListener(this);
}

void ServiceWorkerVersion::SetNavigationPreloadState(
    const NavigationPreloadState& state) {
  navigation_preload_state_ = state;
}

void ServiceWorkerVersion::SetStatus(Status status) {
  if (status_ == status)
    return;

  TRACE_EVENT2("ServiceWorker", "ServiceWorkerVersion::SetStatus", "Script URL",
               script_url_.spec(), "New Status", VersionStatusToString(status));

  // |fetch_handler_existence_| must be set before setting the status to
  // INSTALLED,
  // ACTIVATING or ACTIVATED.
  DCHECK(fetch_handler_existence_ != FetchHandlerExistence::UNKNOWN ||
         !(status == INSTALLED || status == ACTIVATING || status == ACTIVATED));

  status_ = status;
  if (skip_waiting_) {
    if (status == INSTALLED) {
      RestartTick(&skip_waiting_time_);
    } else if (status == ACTIVATED) {
      ClearTick(&skip_waiting_time_);
      for (int request_id : pending_skip_waiting_requests_)
        DidSkipWaiting(request_id);
      pending_skip_waiting_requests_.clear();
    }
  }

  // OnVersionStateChanged() invokes updates of the status using state
  // change IPC at ServiceWorkerHandle (for JS-land on renderer process) and
  // ServiceWorkerContextCore (for devtools and serviceworker-internals).
  // This should be done before using the new status by
  // |status_change_callbacks_| which sends the IPC for resolving the .ready
  // property.
  // TODO(shimazu): Clarify the dependency of OnVersionStateChanged and
  // |status_change_callbacks_|
  for (auto& observer : listeners_)
    observer.OnVersionStateChanged(this);

  std::vector<base::OnceClosure> callbacks;
  callbacks.swap(status_change_callbacks_);
  for (auto& callback : callbacks)
    std::move(callback).Run();

  if (status == INSTALLED)
    embedded_worker_->OnWorkerVersionInstalled();
  else if (status == REDUNDANT)
    embedded_worker_->OnWorkerVersionDoomed();
}

void ServiceWorkerVersion::RegisterStatusChangeCallback(
    base::OnceClosure callback) {
  status_change_callbacks_.push_back(std::move(callback));
}

ServiceWorkerVersionInfo ServiceWorkerVersion::GetInfo() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  ServiceWorkerVersionInfo info(
      running_status(), status(), fetch_handler_existence(), script_url(),
      registration_id(), version_id(), embedded_worker()->process_id(),
      embedded_worker()->thread_id(),
      embedded_worker()->worker_devtools_agent_route_id());
  for (const auto& controllee : controllee_map_) {
    const ServiceWorkerProviderHost* host = controllee.second;
    info.clients.insert(std::make_pair(
        host->client_uuid(),
        ServiceWorkerVersionInfo::ClientInfo(
            host->process_id(), host->route_id(), host->web_contents_getter(),
            host->provider_type())));
  }
  if (!main_script_http_info_)
    return info;
  info.script_response_time = main_script_http_info_->response_time;
  if (main_script_http_info_->headers)
    main_script_http_info_->headers->GetLastModifiedValue(
        &info.script_last_modified);
  return info;
}

void ServiceWorkerVersion::set_fetch_handler_existence(
    FetchHandlerExistence existence) {
  DCHECK_EQ(fetch_handler_existence_, FetchHandlerExistence::UNKNOWN);
  DCHECK_NE(existence, FetchHandlerExistence::UNKNOWN);
  fetch_handler_existence_ = existence;
  if (site_for_uma_ != ServiceWorkerMetrics::Site::OTHER)
    return;
  if (existence == FetchHandlerExistence::EXISTS)
    site_for_uma_ = ServiceWorkerMetrics::Site::WITH_FETCH_HANDLER;
  else
    site_for_uma_ = ServiceWorkerMetrics::Site::WITHOUT_FETCH_HANDLER;
}

void ServiceWorkerVersion::StartWorker(ServiceWorkerMetrics::EventType purpose,
                                       const StatusCallback& callback) {
  TRACE_EVENT_INSTANT2(
      "ServiceWorker", "ServiceWorkerVersion::StartWorker (instant)",
      TRACE_EVENT_SCOPE_THREAD, "Script", script_url_.spec(), "Purpose",
      ServiceWorkerMetrics::EventTypeToString(purpose));

  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  const bool is_browser_startup_complete =
      GetContentClient()->browser()->IsBrowserStartupComplete();
  if (!context_) {
    RecordStartWorkerResult(purpose, status_, kInvalidTraceId,
                            is_browser_startup_complete,
                            SERVICE_WORKER_ERROR_ABORT);
    RunSoon(base::BindOnce(callback, SERVICE_WORKER_ERROR_ABORT));
    return;
  }
  if (is_redundant()) {
    RecordStartWorkerResult(purpose, status_, kInvalidTraceId,
                            is_browser_startup_complete,
                            SERVICE_WORKER_ERROR_REDUNDANT);
    RunSoon(base::BindOnce(callback, SERVICE_WORKER_ERROR_REDUNDANT));
    return;
  }

  // Check that the worker is allowed to start on the given scope. Since this
  // worker might not be used for a specific tab, pass a null callback as
  // WebContents getter.
  // resource_context() can return null in unit tests.
  if (context_->wrapper()->resource_context() &&
      !GetContentClient()->browser()->AllowServiceWorker(
          scope_, scope_, context_->wrapper()->resource_context(),
          base::Callback<WebContents*(void)>())) {
    RecordStartWorkerResult(purpose, status_, kInvalidTraceId,
                            is_browser_startup_complete,
                            SERVICE_WORKER_ERROR_DISALLOWED);
    RunSoon(base::BindOnce(callback, SERVICE_WORKER_ERROR_DISALLOWED));
    return;
  }

  // Ensure the live registration during starting worker so that the worker can
  // get associated with it in
  // ServiceWorkerProviderHost::CompleteStartWorkerPreparation.
  context_->storage()->FindRegistrationForId(
      registration_id_, scope_.GetOrigin(),
      base::Bind(&ServiceWorkerVersion::DidEnsureLiveRegistrationForStartWorker,
                 weak_factory_.GetWeakPtr(), purpose, status_,
                 is_browser_startup_complete, callback));
}

void ServiceWorkerVersion::StopWorker(const StatusCallback& callback) {
  TRACE_EVENT_INSTANT2("ServiceWorker",
                       "ServiceWorkerVersion::StopWorker (instant)",
                       TRACE_EVENT_SCOPE_THREAD, "Script", script_url_.spec(),
                       "Status", VersionStatusToString(status_));

  switch (running_status()) {
    case EmbeddedWorkerStatus::STARTING:
    case EmbeddedWorkerStatus::RUNNING:
      embedded_worker_->Stop();
      if (running_status() == EmbeddedWorkerStatus::STOPPED) {
        RunSoon(base::BindOnce(callback, SERVICE_WORKER_OK));
        return;
      }
      stop_callbacks_.push_back(callback);
      return;
    case EmbeddedWorkerStatus::STOPPING:
      stop_callbacks_.push_back(callback);
      return;
    case EmbeddedWorkerStatus::STOPPED:
      RunSoon(base::BindOnce(callback, SERVICE_WORKER_OK));
      return;
  }
  NOTREACHED();
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
  update_timer_.Start(FROM_HERE, kUpdateDelay,
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

int ServiceWorkerVersion::StartRequest(
    ServiceWorkerMetrics::EventType event_type,
    const StatusCallback& error_callback) {
  return StartRequestWithCustomTimeout(event_type, error_callback,
                                       kRequestTimeout, KILL_ON_TIMEOUT);
}

int ServiceWorkerVersion::StartRequestWithCustomTimeout(
    ServiceWorkerMetrics::EventType event_type,
    const StatusCallback& error_callback,
    const base::TimeDelta& timeout,
    TimeoutBehavior timeout_behavior) {
  DCHECK_EQ(EmbeddedWorkerStatus::RUNNING, running_status())
      << "Can only start a request with a running worker.";
  DCHECK(event_type == ServiceWorkerMetrics::EventType::INSTALL ||
         event_type == ServiceWorkerMetrics::EventType::ACTIVATE ||
         event_type == ServiceWorkerMetrics::EventType::MESSAGE ||
         status() == ACTIVATED)
      << "Event of type " << static_cast<int>(event_type)
      << " can only be dispatched to an active worker: " << status();

  int request_id = pending_requests_.Add(base::MakeUnique<PendingRequest>(
      error_callback, clock_->Now(), tick_clock_->NowTicks(), event_type));
  TRACE_EVENT_ASYNC_BEGIN2("ServiceWorker", "ServiceWorkerVersion::Request",
                           pending_requests_.Lookup(request_id), "Request id",
                           request_id, "Event type",
                           ServiceWorkerMetrics::EventTypeToString(event_type));
  base::TimeTicks expiration_time = tick_clock_->NowTicks() + timeout;
  timeout_queue_.push(
      RequestInfo(request_id, event_type, expiration_time, timeout_behavior));
  if (expiration_time > max_request_expiration_time_)
    max_request_expiration_time_ = expiration_time;
  return request_id;
}

bool ServiceWorkerVersion::StartExternalRequest(
    const std::string& request_uuid) {
  // It's possible that the renderer is lying or the version started stopping
  // right around the time of the IPC.
  if (running_status() != EmbeddedWorkerStatus::RUNNING)
    return false;

  if (external_request_uuid_to_request_id_.count(request_uuid) > 0u)
    return false;

  int request_id =
      StartRequest(ServiceWorkerMetrics::EventType::EXTERNAL_REQUEST,
                   base::Bind(&ServiceWorkerVersion::CleanUpExternalRequest,
                              this, request_uuid));
  external_request_uuid_to_request_id_[request_uuid] = request_id;
  return true;
}

bool ServiceWorkerVersion::FinishRequest(int request_id,
                                         bool was_handled,
                                         base::Time dispatch_event_time) {
  PendingRequest* request = pending_requests_.Lookup(request_id);
  if (!request)
    return false;
  if (event_recorder_)
    event_recorder_->RecordEventHandledStatus(request->event_type, was_handled);
  ServiceWorkerMetrics::RecordEventDuration(
      request->event_type, tick_clock_->NowTicks() - request->start_time_ticks,
      was_handled);
  ServiceWorkerMetrics::RecordEventDispatchingDelay(
      request->event_type, dispatch_event_time - request->start_time,
      site_for_uma());

  RestartTick(&idle_time_);
  TRACE_EVENT_ASYNC_END1("ServiceWorker", "ServiceWorkerVersion::Request",
                         request, "Handled", was_handled);
  pending_requests_.Remove(request_id);
  if (!HasWork()) {
    for (auto& observer : listeners_)
      observer.OnNoWork(this);
  }

  return true;
}

bool ServiceWorkerVersion::FinishExternalRequest(
    const std::string& request_uuid) {
  // It's possible that the renderer is lying or the version started stopping
  // right around the time of the IPC.
  if (running_status() != EmbeddedWorkerStatus::RUNNING)
    return false;

  RequestUUIDToRequestIDMap::iterator iter =
      external_request_uuid_to_request_id_.find(request_uuid);
  if (iter != external_request_uuid_to_request_id_.end()) {
    int request_id = iter->second;
    external_request_uuid_to_request_id_.erase(iter);
    return FinishRequest(request_id, true, clock_->Now());
  }

  // It is possible that the request was cancelled or timed out before and we
  // won't find it in |external_request_uuid_to_request_id_|.
  // Return true so we don't kill the process.
  return true;
}

ServiceWorkerVersion::SimpleEventCallback
ServiceWorkerVersion::CreateSimpleEventCallback(int request_id) {
  // The weak reference to |this| is safe because storage of the callbacks, the
  // pending responses of the ServiceWorkerEventDispatcher, is owned by |this|.
  return base::Bind(&ServiceWorkerVersion::OnSimpleEventFinished,
                    base::Unretained(this), request_id);
}

void ServiceWorkerVersion::RunAfterStartWorker(
    ServiceWorkerMetrics::EventType purpose,
    base::OnceClosure task,
    const StatusCallback& error_callback) {
  if (running_status() == EmbeddedWorkerStatus::RUNNING) {
    DCHECK(start_callbacks_.empty());
    std::move(task).Run();
    return;
  }
  StartWorker(purpose,
              base::Bind(&RunTaskAfterStartWorker, weak_factory_.GetWeakPtr(),
                         error_callback, base::Passed(std::move(task))));
}

void ServiceWorkerVersion::AddControllee(
    ServiceWorkerProviderHost* provider_host) {
  const std::string& uuid = provider_host->client_uuid();
  CHECK(!provider_host->client_uuid().empty());
  DCHECK(!base::ContainsKey(controllee_map_, uuid));
  controllee_map_[uuid] = provider_host;
  // Keep the worker alive a bit longer right after a new controllee is added.
  RestartTick(&idle_time_);
  ClearTick(&no_controllees_time_);
  for (auto& observer : listeners_)
    observer.OnControlleeAdded(this, provider_host);
}

void ServiceWorkerVersion::RemoveControllee(
    ServiceWorkerProviderHost* provider_host) {
  const std::string& uuid = provider_host->client_uuid();
  DCHECK(base::ContainsKey(controllee_map_, uuid));
  controllee_map_.erase(uuid);
  for (auto& observer : listeners_)
    observer.OnControlleeRemoved(this, provider_host);
  if (!HasControllee()) {
    RestartTick(&no_controllees_time_);
    for (auto& observer : listeners_)
      observer.OnNoControllees(this);
  }
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
  if (!HasWork()) {
    for (auto& observer : listeners_)
      observer.OnNoWork(this);
  }
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

void ServiceWorkerVersion::ReportForceUpdateToDevTools() {
  embedded_worker_->AddMessageToConsole(blink::WebConsoleMessage::kLevelWarning,
                                        kForceUpdateInfoMessage);
}

void ServiceWorkerVersion::SetStartWorkerStatusCode(
    ServiceWorkerStatusCode status) {
  start_worker_status_ = status;
}

void ServiceWorkerVersion::Doom() {
  DCHECK(!HasControllee());
  SetStatus(REDUNDANT);
  if (running_status() == EmbeddedWorkerStatus::STARTING ||
      running_status() == EmbeddedWorkerStatus::RUNNING) {
    if (embedded_worker()->devtools_attached())
      stop_when_devtools_detached_ = true;
    else
      embedded_worker_->Stop();
  }
  if (!context_)
    return;
  std::vector<ServiceWorkerDatabase::ResourceRecord> resources;
  script_cache_map_.GetResources(&resources);
  context_->storage()->PurgeResources(resources);
}

void ServiceWorkerVersion::SetValidOriginTrialTokens(
    const TrialTokenValidator::FeatureToTokensMap& tokens) {
  origin_trial_tokens_ = TrialTokenValidator::GetValidTokens(
      url::Origin(scope()), tokens, clock_->Now());
}

void ServiceWorkerVersion::SetDevToolsAttached(bool attached) {
  embedded_worker()->SetDevToolsAttached(attached);

  if (stop_when_devtools_detached_ && !attached) {
    DCHECK_EQ(REDUNDANT, status());
    if (running_status() == EmbeddedWorkerStatus::STARTING ||
        running_status() == EmbeddedWorkerStatus::RUNNING) {
      embedded_worker_->Stop();
    }
    return;
  }
  if (attached) {
    // TODO(falken): Canceling the timeouts when debugging could cause
    // heisenbugs; we should instead run them as normal show an educational
    // message in DevTools when they occur. crbug.com/470419

    // Don't record the startup time metric once DevTools is attached.
    ClearTick(&start_time_);
    skip_recording_startup_time_ = true;

    // Cancel request timeouts.
    SetAllRequestExpirations(base::TimeTicks());
    return;
  }
  if (!start_callbacks_.empty()) {
    // Reactivate the timer for start timeout.
    DCHECK(timeout_timer_.IsRunning());
    DCHECK(running_status() == EmbeddedWorkerStatus::STARTING ||
           running_status() == EmbeddedWorkerStatus::STOPPING)
        << static_cast<int>(running_status());
    RestartTick(&start_time_);
  }

  // Reactivate request timeouts, setting them all to the same expiration time.
  SetAllRequestExpirations(tick_clock_->NowTicks() + kRequestTimeout);
}

void ServiceWorkerVersion::SetMainScriptHttpResponseInfo(
    const net::HttpResponseInfo& http_info) {
  main_script_http_info_.reset(new net::HttpResponseInfo(http_info));

  // Updates |origin_trial_tokens_| if it is not set yet. This happens when:
  //  1) The worker is a new one.
  //  OR
  //  2) The worker is an existing one but the entry in ServiceWorkerDatabase
  //     was written by old version Chrome (< M56), so |origin_trial_tokens|
  //     wasn't set in the entry.
  if (!origin_trial_tokens_) {
    origin_trial_tokens_ = TrialTokenValidator::GetValidTokensFromHeaders(
        url::Origin(scope()), http_info.headers.get(), clock_->Now());
  }

  for (auto& observer : listeners_)
    observer.OnMainScriptHttpResponseInfoSet(this);
}

void ServiceWorkerVersion::SimulatePingTimeoutForTesting() {
  ping_controller_->SimulateTimeoutForTesting();
}

void ServiceWorkerVersion::SetTickClockForTesting(
    std::unique_ptr<base::TickClock> tick_clock) {
  tick_clock_ = std::move(tick_clock);
}

void ServiceWorkerVersion::SetClockForTesting(
    std::unique_ptr<base::Clock> clock) {
  clock_ = std::move(clock);
}

const net::HttpResponseInfo*
ServiceWorkerVersion::GetMainScriptHttpResponseInfo() {
  return main_script_http_info_.get();
}

ServiceWorkerVersion::RequestInfo::RequestInfo(
    int id,
    ServiceWorkerMetrics::EventType event_type,
    const base::TimeTicks& expiration,
    TimeoutBehavior timeout_behavior)
    : id(id),
      event_type(event_type),
      expiration(expiration),
      timeout_behavior(timeout_behavior) {}

ServiceWorkerVersion::RequestInfo::~RequestInfo() {
}

bool ServiceWorkerVersion::RequestInfo::operator>(
    const RequestInfo& other) const {
  return expiration > other.expiration;
}

ServiceWorkerVersion::PendingRequest::PendingRequest(
    const StatusCallback& callback,
    base::Time time,
    const base::TimeTicks& time_ticks,
    ServiceWorkerMetrics::EventType event_type)
    : error_callback(callback),
      start_time(time),
      start_time_ticks(time_ticks),
      event_type(event_type) {}

ServiceWorkerVersion::PendingRequest::~PendingRequest() {}

void ServiceWorkerVersion::OnThreadStarted() {
  DCHECK_EQ(EmbeddedWorkerStatus::STARTING, running_status());
  DCHECK(provider_host_);
  provider_host_->SetReadyToSendMessagesToWorker(
      embedded_worker()->thread_id());
  // Activate ping/pong now that JavaScript execution will start.
  ping_controller_->Activate();
}

void ServiceWorkerVersion::OnStarting() {
  for (auto& observer : listeners_)
    observer.OnRunningStateChanged(this);
}

void ServiceWorkerVersion::OnStarted() {
  DCHECK_EQ(EmbeddedWorkerStatus::RUNNING, running_status());
  RestartTick(&idle_time_);

  // Fire all start callbacks.
  scoped_refptr<ServiceWorkerVersion> protect(this);
  FinishStartWorker(SERVICE_WORKER_OK);
  for (auto& observer : listeners_)
    observer.OnRunningStateChanged(this);
}

void ServiceWorkerVersion::OnStopping() {
  DCHECK(stop_time_.is_null());
  RestartTick(&stop_time_);
  TRACE_EVENT_ASYNC_BEGIN2("ServiceWorker", "ServiceWorkerVersion::StopWorker",
                           stop_time_.ToInternalValue(), "Script",
                           script_url_.spec(), "Version Status",
                           VersionStatusToString(status_));

  // Shorten the interval so stalling in stopped can be fixed quickly. Once the
  // worker stops, the timer is disabled. The interval will be reset to normal
  // when the worker starts up again.
  SetTimeoutTimerInterval(kStopWorkerTimeout);
  for (auto& observer : listeners_)
    observer.OnRunningStateChanged(this);
}

void ServiceWorkerVersion::OnStopped(EmbeddedWorkerStatus old_status) {
  if (IsInstalled(status())) {
    ServiceWorkerMetrics::RecordWorkerStopped(
        ServiceWorkerMetrics::StopStatus::NORMAL);
  }
  if (!stop_time_.is_null())
    ServiceWorkerMetrics::RecordStopWorkerTime(GetTickDuration(stop_time_));

  OnStoppedInternal(old_status);
}

void ServiceWorkerVersion::OnDetached(EmbeddedWorkerStatus old_status) {
  if (IsInstalled(status())) {
    ServiceWorkerMetrics::RecordWorkerStopped(
        ServiceWorkerMetrics::StopStatus::DETACH_BY_REGISTRY);
  }
  OnStoppedInternal(old_status);
}

void ServiceWorkerVersion::OnScriptLoaded() {
  DCHECK(GetMainScriptHttpResponseInfo());
  if (IsInstalled(status()))
    UMA_HISTOGRAM_BOOLEAN("ServiceWorker.ScriptLoadSuccess", true);
}

void ServiceWorkerVersion::OnScriptLoadFailed() {
  if (IsInstalled(status()))
    UMA_HISTOGRAM_BOOLEAN("ServiceWorker.ScriptLoadSuccess", false);
}

void ServiceWorkerVersion::OnRegisteredToDevToolsManager() {
  for (auto& observer : listeners_)
    observer.OnDevToolsRoutingIdChanged(this);
}

void ServiceWorkerVersion::OnReportException(
    const base::string16& error_message,
    int line_number,
    int column_number,
    const GURL& source_url) {
  for (auto& observer : listeners_) {
    observer.OnErrorReported(this, error_message, line_number, column_number,
                             source_url);
  }
}

void ServiceWorkerVersion::OnReportConsoleMessage(int source_identifier,
                                                  int message_level,
                                                  const base::string16& message,
                                                  int line_number,
                                                  const GURL& source_url) {
  for (auto& observer : listeners_) {
    observer.OnReportConsoleMessage(this, source_identifier, message_level,
                                    message, line_number, source_url);
  }
}

bool ServiceWorkerVersion::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(ServiceWorkerVersion, message)
    IPC_MESSAGE_HANDLER(ServiceWorkerHostMsg_GetClient, OnGetClient)
    IPC_MESSAGE_HANDLER(ServiceWorkerHostMsg_GetClients,
                        OnGetClients)
    IPC_MESSAGE_HANDLER(ServiceWorkerHostMsg_OpenNewTab, OnOpenNewTab)
    IPC_MESSAGE_HANDLER(ServiceWorkerHostMsg_OpenNewPopup, OnOpenNewPopup)
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
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void ServiceWorkerVersion::OnStartSentAndScriptEvaluated(
    ServiceWorkerStatusCode status) {
  if (status != SERVICE_WORKER_OK) {
    scoped_refptr<ServiceWorkerVersion> protect(this);
    FinishStartWorker(DeduceStartWorkerFailureReason(status));
  }
}

void ServiceWorkerVersion::OnGetClient(int request_id,
                                       const std::string& client_uuid) {
  if (!context_)
    return;
  TRACE_EVENT_ASYNC_BEGIN1("ServiceWorker", "ServiceWorkerVersion::OnGetClient",
                           request_id, "client_uuid", client_uuid);
  ServiceWorkerProviderHost* provider_host =
      context_->GetProviderHostByClientID(client_uuid);
  if (!provider_host ||
      provider_host->document_url().GetOrigin() != script_url_.GetOrigin()) {
    // The promise will be resolved to 'undefined'.
    OnGetClientFinished(request_id, ServiceWorkerClientInfo());
    return;
  }
  service_worker_client_utils::GetClient(
      provider_host, base::Bind(&ServiceWorkerVersion::OnGetClientFinished,
                                weak_factory_.GetWeakPtr(), request_id));
}

void ServiceWorkerVersion::OnGetClientFinished(
    int request_id,
    const ServiceWorkerClientInfo& client_info) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  TRACE_EVENT_ASYNC_END1("ServiceWorker", "ServiceWorkerVersion::OnGetClient",
                         request_id, "client_type", client_info.client_type);

  // When Clients.get() is called on the script evaluation phase, the running
  // status can be STARTING here.
  if (running_status() != EmbeddedWorkerStatus::STARTING &&
      running_status() != EmbeddedWorkerStatus::RUNNING) {
    return;
  }

  embedded_worker_->SendMessage(
      ServiceWorkerMsg_DidGetClient(request_id, client_info));
}

void ServiceWorkerVersion::OnGetClients(
    int request_id,
    const ServiceWorkerClientQueryOptions& options) {
  TRACE_EVENT_ASYNC_BEGIN2(
      "ServiceWorker", "ServiceWorkerVersion::OnGetClients", request_id,
      "client_type", options.client_type, "include_uncontrolled",
      options.include_uncontrolled);
  service_worker_client_utils::GetClients(
      weak_factory_.GetWeakPtr(), options,
      base::Bind(&ServiceWorkerVersion::OnGetClientsFinished,
                 weak_factory_.GetWeakPtr(), request_id));
}

void ServiceWorkerVersion::OnGetClientsFinished(
    int request_id,
    std::unique_ptr<ServiceWorkerClients> clients) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  TRACE_EVENT_ASYNC_END1("ServiceWorker", "ServiceWorkerVersion::OnGetClients",
                         request_id, "The number of clients", clients->size());

  // When Clients.matchAll() is called on the script evaluation phase, the
  // running status can be STARTING here.
  if (running_status() != EmbeddedWorkerStatus::STARTING &&
      running_status() != EmbeddedWorkerStatus::RUNNING) {
    return;
  }

  embedded_worker_->SendMessage(
      ServiceWorkerMsg_DidGetClients(request_id, *clients));
}

void ServiceWorkerVersion::OnSimpleEventFinished(
    int request_id,
    ServiceWorkerStatusCode status,
    base::Time dispatch_event_time) {
  PendingRequest* request = pending_requests_.Lookup(request_id);
  // |request| will be null when the request has been timed out.
  if (!request)
    return;
  // Copy error callback before calling FinishRequest.
  StatusCallback callback = request->error_callback;

  FinishRequest(request_id, status == SERVICE_WORKER_OK, dispatch_event_time);

  callback.Run(status);
}

void ServiceWorkerVersion::CountFeature(uint32_t feature) {
  if (!used_features_.insert(feature).second)
    return;
  for (auto provider_host_by_uuid : controllee_map_)
    provider_host_by_uuid.second->CountFeature(feature);
}

bool ServiceWorkerVersion::IsInstalled(ServiceWorkerVersion::Status status) {
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

void ServiceWorkerVersion::OnOpenNewTab(int request_id, const GURL& url) {
  OnOpenWindow(request_id, url, WindowOpenDisposition::NEW_FOREGROUND_TAB);
}

void ServiceWorkerVersion::OnOpenNewPopup(int request_id, const GURL& url) {
  OnOpenWindow(request_id, url, WindowOpenDisposition::NEW_POPUP);
}

void ServiceWorkerVersion::OnOpenWindow(int request_id,
                                        GURL url,
                                        WindowOpenDisposition disposition) {
  // Just abort if we are shutting down.
  if (!context_)
    return;

  if (!url.is_valid()) {
    DVLOG(1) << "Received unexpected invalid URL from renderer process.";
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                            base::BindOnce(&KillEmbeddedWorkerProcess,
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

  service_worker_client_utils::OpenWindow(
      url, script_url_, embedded_worker_->process_id(), context_, disposition,
      base::Bind(&ServiceWorkerVersion::OnOpenWindowFinished,
                 weak_factory_.GetWeakPtr(), request_id));
}

void ServiceWorkerVersion::OnOpenWindowFinished(
    int request_id,
    ServiceWorkerStatusCode status,
    const ServiceWorkerClientInfo& client_info) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (running_status() != EmbeddedWorkerStatus::RUNNING)
    return;

  if (status != SERVICE_WORKER_OK) {
    embedded_worker_->SendMessage(ServiceWorkerMsg_OpenWindowError(
        request_id, "Something went wrong while trying to open the window."));
    return;
  }

  embedded_worker_->SendMessage(
      ServiceWorkerMsg_OpenWindowResponse(request_id, client_info));
}

void ServiceWorkerVersion::OnSetCachedMetadata(const GURL& url,
                                               const std::vector<char>& data) {
  int64_t callback_id = tick_clock_->NowTicks().ToInternalValue();
  TRACE_EVENT_ASYNC_BEGIN1("ServiceWorker",
                           "ServiceWorkerVersion::OnSetCachedMetadata",
                           callback_id, "URL", url.spec());
  script_cache_map_.WriteMetadata(
      url, data, base::Bind(&ServiceWorkerVersion::OnSetCachedMetadataFinished,
                            weak_factory_.GetWeakPtr(), callback_id));
}

void ServiceWorkerVersion::OnSetCachedMetadataFinished(int64_t callback_id,
                                                       int result) {
  TRACE_EVENT_ASYNC_END1("ServiceWorker",
                         "ServiceWorkerVersion::OnSetCachedMetadata",
                         callback_id, "result", result);
  for (auto& observer : listeners_)
    observer.OnCachedMetadataUpdated(this);
}

void ServiceWorkerVersion::OnClearCachedMetadata(const GURL& url) {
  int64_t callback_id = tick_clock_->NowTicks().ToInternalValue();
  TRACE_EVENT_ASYNC_BEGIN1("ServiceWorker",
                           "ServiceWorkerVersion::OnClearCachedMetadata",
                           callback_id, "URL", url.spec());
  script_cache_map_.ClearMetadata(
      url, base::Bind(&ServiceWorkerVersion::OnClearCachedMetadataFinished,
                      weak_factory_.GetWeakPtr(), callback_id));
}

void ServiceWorkerVersion::OnClearCachedMetadataFinished(int64_t callback_id,
                                                         int result) {
  TRACE_EVENT_ASYNC_END1("ServiceWorker",
                         "ServiceWorkerVersion::OnClearCachedMetadata",
                         callback_id, "result", result);
  for (auto& observer : listeners_)
    observer.OnCachedMetadataUpdated(this);
}

void ServiceWorkerVersion::OnPostMessageToClient(
    const std::string& client_uuid,
    const base::string16& message,
    const std::vector<MessagePort>& sent_message_ports) {
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
  provider_host->PostMessageToClient(this, message, sent_message_ports);
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
  if (provider_host->client_type() !=
      blink::kWebServiceWorkerClientTypeWindow) {
    // focus() should be called only for WindowClient. This may happen due to
    // bad message.
    return;
  }

  service_worker_client_utils::FocusWindowClient(
      provider_host, base::Bind(&ServiceWorkerVersion::OnFocusClientFinished,
                                weak_factory_.GetWeakPtr(), request_id));
}

void ServiceWorkerVersion::OnFocusClientFinished(
    int request_id,
    const ServiceWorkerClientInfo& client_info) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (running_status() != EmbeddedWorkerStatus::RUNNING)
    return;

  embedded_worker_->SendMessage(
      ServiceWorkerMsg_FocusClientResponse(request_id, client_info));
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
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                            base::BindOnce(&KillEmbeddedWorkerProcess,
                                           embedded_worker_->process_id(),
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

  service_worker_client_utils::NavigateClient(
      url, script_url_, provider_host->process_id(), provider_host->frame_id(),
      context_, base::Bind(&ServiceWorkerVersion::OnNavigateClientFinished,
                           weak_factory_.GetWeakPtr(), request_id));
}

void ServiceWorkerVersion::OnNavigateClientFinished(
    int request_id,
    ServiceWorkerStatusCode status,
    const ServiceWorkerClientInfo& client_info) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (running_status() != EmbeddedWorkerStatus::RUNNING)
    return;

  if (status != SERVICE_WORKER_OK) {
    embedded_worker_->SendMessage(
        ServiceWorkerMsg_NavigateClientError(request_id, GURL()));
    return;
  }

  embedded_worker_->SendMessage(
      ServiceWorkerMsg_NavigateClientResponse(request_id, client_info));
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
  if (skip_waiting_time_.is_null())
    RestartTick(&skip_waiting_time_);
  pending_skip_waiting_requests_.push_back(request_id);
  if (pending_skip_waiting_requests_.size() == 1)
    registration->ActivateWaitingVersionWhenReady();
}

void ServiceWorkerVersion::DidSkipWaiting(int request_id) {
  if (running_status() == EmbeddedWorkerStatus::STARTING ||
      running_status() == EmbeddedWorkerStatus::RUNNING) {
    embedded_worker_->SendMessage(ServiceWorkerMsg_DidSkipWaiting(request_id));
  }
}

void ServiceWorkerVersion::OnClaimClients(int request_id) {
  if (status_ != ACTIVATING && status_ != ACTIVATED) {
    embedded_worker_->SendMessage(ServiceWorkerMsg_ClaimClientsError(
        request_id, blink::mojom::ServiceWorkerErrorType::kState,
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
      request_id, blink::mojom::ServiceWorkerErrorType::kAbort,
      base::ASCIIToUTF16(kClaimClientsShutdownErrorMesage)));
}

void ServiceWorkerVersion::OnPongFromWorker() {
  ping_controller_->OnPongReceived();
}

void ServiceWorkerVersion::RegisterForeignFetchScopes(
    const std::vector<GURL>& sub_scopes,
    const std::vector<url::Origin>& origins) {
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
      BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                              base::BindOnce(&KillEmbeddedWorkerProcess,
                                             embedded_worker_->process_id(),
                                             RESULT_CODE_KILLED_BAD_MESSAGE));
      return;
    }
  }
  for (const url::Origin& url : origins) {
    if (url.unique()) {
      DVLOG(1) << "Received unexpected unique origin from renderer process.";
      BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                              base::BindOnce(&KillEmbeddedWorkerProcess,
                                             embedded_worker_->process_id(),
                                             RESULT_CODE_KILLED_BAD_MESSAGE));
      return;
    }
  }
  set_foreign_fetch_scopes(sub_scopes);
  set_foreign_fetch_origins(origins);
}

void ServiceWorkerVersion::DidEnsureLiveRegistrationForStartWorker(
    ServiceWorkerMetrics::EventType purpose,
    Status prestart_status,
    bool is_browser_startup_complete,
    const StatusCallback& callback,
    ServiceWorkerStatusCode status,
    scoped_refptr<ServiceWorkerRegistration> registration) {
  scoped_refptr<ServiceWorkerRegistration> protect = registration;
  if (status == SERVICE_WORKER_ERROR_NOT_FOUND) {
    // When the registration has already been deleted from the storage but its
    // active worker is still controlling clients, the event should be
    // dispatched on the worker. However, the storage cannot find the
    // registration. To handle the case, check the live registrations here.
    protect = context_->GetLiveRegistration(registration_id_);
    if (protect) {
      DCHECK(protect->is_deleted());
      status = SERVICE_WORKER_OK;
    }
  }
  if (status != SERVICE_WORKER_OK) {
    RecordStartWorkerResult(purpose, prestart_status, kInvalidTraceId,
                            is_browser_startup_complete, status);
    RunSoon(base::BindOnce(callback, SERVICE_WORKER_ERROR_START_WORKER_FAILED));
    return;
  }
  if (is_redundant()) {
    RecordStartWorkerResult(purpose, prestart_status, kInvalidTraceId,
                            is_browser_startup_complete,
                            SERVICE_WORKER_ERROR_REDUNDANT);
    RunSoon(base::BindOnce(callback, SERVICE_WORKER_ERROR_REDUNDANT));
    return;
  }

  MarkIfStale();

  switch (running_status()) {
    case EmbeddedWorkerStatus::RUNNING:
      RunSoon(base::BindOnce(callback, SERVICE_WORKER_OK));
      return;
    case EmbeddedWorkerStatus::STARTING:
      DCHECK(!start_callbacks_.empty());
      break;
    case EmbeddedWorkerStatus::STOPPING:
    case EmbeddedWorkerStatus::STOPPED:
      if (start_callbacks_.empty()) {
        int trace_id = NextTraceId();
        TRACE_EVENT_ASYNC_BEGIN2(
            "ServiceWorker", "ServiceWorkerVersion::StartWorker", trace_id,
            "Script", script_url_.spec(), "Purpose",
            ServiceWorkerMetrics::EventTypeToString(purpose));
        DCHECK(!start_worker_first_purpose_);
        start_worker_first_purpose_ = purpose;
        start_callbacks_.push_back(
            base::Bind(&ServiceWorkerVersion::RecordStartWorkerResult,
                       weak_factory_.GetWeakPtr(), purpose, prestart_status,
                       trace_id, is_browser_startup_complete));
      }
      break;
  }

  // Keep the live registration while starting the worker.
  start_callbacks_.push_back(
      base::Bind(&RunStartWorkerCallback, callback, protect));

  if (running_status() == EmbeddedWorkerStatus::STOPPED)
    StartWorkerInternal();
  DCHECK(timeout_timer_.IsRunning());
}

void ServiceWorkerVersion::StartWorkerInternal() {
  DCHECK_EQ(EmbeddedWorkerStatus::STOPPED, running_status());
  DCHECK(start_worker_first_purpose_);

  if (!ServiceWorkerMetrics::ShouldExcludeSiteFromHistogram(site_for_uma_)) {
    DCHECK(!event_recorder_);
    event_recorder_ =
        base::MakeUnique<ServiceWorkerMetrics::ScopedEventRecorder>(
            start_worker_first_purpose_.value());
  }
  // We don't clear |start_worker_first_purpose_| here but clear in
  // FinishStartWorker. This is because StartWorkerInternal may be called
  // again from OnStoppedInternal if StopWorker is called before OnStarted.

  StartTimeoutTimer();

  std::unique_ptr<ServiceWorkerProviderHost> pending_provider_host =
      ServiceWorkerProviderHost::PreCreateForController(context());
  provider_host_ = pending_provider_host->AsWeakPtr();

  auto params = base::MakeUnique<EmbeddedWorkerStartParams>();
  params->service_worker_version_id = version_id_;
  params->scope = scope_;
  params->script_url = script_url_;
  params->is_installed = IsInstalled(status_);
  params->pause_after_download = pause_after_download_;

  mojom::ServiceWorkerInstalledScriptsInfoPtr installed_scripts_info;
  if (ServiceWorkerUtils::IsScriptStreamingEnabled() &&
      !pause_after_download_) {
    DCHECK(!installed_scripts_sender_);
    installed_scripts_sender_ =
        base::MakeUnique<ServiceWorkerInstalledScriptsSender>(
            this, script_url(), context());
    installed_scripts_info = installed_scripts_sender_->CreateInfoAndBind();
    installed_scripts_sender_->Start();
  }

  auto event_dispatcher_request = mojo::MakeRequest(&event_dispatcher_);
  // TODO(horo): These CHECKs are for debugging crbug.com/759938.
  CHECK(event_dispatcher_.is_bound());
  CHECK(event_dispatcher_request.is_pending());

  embedded_worker_->Start(
      std::move(params),
      // Unretained is used here because the callback will be owned by
      // |embedded_worker_| whose owner is |this|.
      base::BindOnce(&CompleteProviderHostPreparation, base::Unretained(this),
                     base::Passed(&pending_provider_host), context()),
      std::move(event_dispatcher_request), std::move(installed_scripts_info),
      base::Bind(&ServiceWorkerVersion::OnStartSentAndScriptEvaluated,
                 weak_factory_.GetWeakPtr()));
  event_dispatcher_.set_connection_error_handler(base::BindOnce(
      &OnEventDispatcherConnectionError, embedded_worker_->AsWeakPtr()));
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

  timeout_timer_.Start(FROM_HERE, kTimeoutTimerDelay, this,
                       &ServiceWorkerVersion::OnTimeoutTimer);
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

void ServiceWorkerVersion::SetTimeoutTimerInterval(base::TimeDelta interval) {
  DCHECK(timeout_timer_.IsRunning());
  if (timeout_timer_.GetCurrentDelay() != interval) {
    timeout_timer_.Stop();
    timeout_timer_.Start(FROM_HERE, interval, this,
                         &ServiceWorkerVersion::OnTimeoutTimer);
  }
}

void ServiceWorkerVersion::OnTimeoutTimer() {
  DCHECK(running_status() == EmbeddedWorkerStatus::STARTING ||
         running_status() == EmbeddedWorkerStatus::RUNNING ||
         running_status() == EmbeddedWorkerStatus::STOPPING)
      << static_cast<int>(running_status());

  if (!context_)
    return;

  MarkIfStale();

  // Stopping the worker hasn't finished within a certain period.
  if (GetTickDuration(stop_time_) > kStopWorkerTimeout) {
    DCHECK_EQ(EmbeddedWorkerStatus::STOPPING, running_status());
    if (IsInstalled(status())) {
      ServiceWorkerMetrics::RecordWorkerStopped(
          ServiceWorkerMetrics::StopStatus::TIMEOUT);
    }
    ReportError(SERVICE_WORKER_ERROR_TIMEOUT, "DETACH_STALLED_IN_STOPPING");

    // Detach the worker. Remove |this| as a listener first; otherwise
    // OnStoppedInternal might try to restart before the new worker
    // is created. Also, protect |this|, since swapping out the
    // EmbeddedWorkerInstance could destroy our ServiceWorkerProviderHost
    // which could in turn destroy |this|.
    scoped_refptr<ServiceWorkerVersion> protect_this(this);
    embedded_worker_->RemoveListener(this);
    embedded_worker_->Detach();
    embedded_worker_ = context_->embedded_worker_registry()->CreateWorker();
    embedded_worker_->AddListener(this);

    // Call OnStoppedInternal to fail callbacks and possibly restart.
    OnStoppedInternal(EmbeddedWorkerStatus::STOPPING);
    return;
  }

  // Trigger update if worker is stale and we waited long enough for it to go
  // idle.
  if (GetTickDuration(stale_time_) > kRequestTimeout) {
    ClearTick(&stale_time_);
    if (!update_timer_.IsRunning())
      ScheduleUpdate();
  }

  // Starting a worker hasn't finished within a certain period.
  const base::TimeDelta start_limit = IsInstalled(status())
                                          ? kStartInstalledWorkerTimeout
                                          : kStartNewWorkerTimeout;
  if (GetTickDuration(start_time_) > start_limit) {
    DCHECK(running_status() == EmbeddedWorkerStatus::STARTING ||
           running_status() == EmbeddedWorkerStatus::STOPPING)
        << static_cast<int>(running_status());
    scoped_refptr<ServiceWorkerVersion> protect(this);
    FinishStartWorker(SERVICE_WORKER_ERROR_TIMEOUT);
    if (running_status() == EmbeddedWorkerStatus::STARTING)
      embedded_worker_->Stop();
    return;
  }

  // Requests have not finished before their expiration.
  bool stop_for_timeout = false;
  while (!timeout_queue_.empty()) {
    RequestInfo info = timeout_queue_.top();
    if (!RequestExpired(info.expiration))
      break;
    if (MaybeTimeOutRequest(info)) {
      stop_for_timeout =
          stop_for_timeout || info.timeout_behavior == KILL_ON_TIMEOUT;
      ServiceWorkerMetrics::RecordEventTimeout(info.event_type);
    }
    timeout_queue_.pop();
  }
  if (stop_for_timeout && running_status() != EmbeddedWorkerStatus::STOPPING)
    embedded_worker_->Stop();

  // For the timeouts below, there are no callbacks to timeout so there is
  // nothing more to do if the worker is already stopping.
  if (running_status() == EmbeddedWorkerStatus::STOPPING)
    return;

  // The worker has been idle for longer than a certain period.
  if (GetTickDuration(idle_time_) > kIdleWorkerTimeout) {
    StopWorkerIfIdle();
    return;
  }

  // Check ping status.
  ping_controller_->CheckPingStatus();
}

void ServiceWorkerVersion::PingWorker() {
  DCHECK(running_status() == EmbeddedWorkerStatus::STARTING ||
         running_status() == EmbeddedWorkerStatus::RUNNING);
  // base::Unretained here is safe because event_dispatcher is owned by |this|.
  event_dispatcher()->Ping(base::BindOnce(
      &ServiceWorkerVersion::OnPongFromWorker, base::Unretained(this)));
}

void ServiceWorkerVersion::OnPingTimeout() {
  DCHECK(running_status() == EmbeddedWorkerStatus::STARTING ||
         running_status() == EmbeddedWorkerStatus::RUNNING);
  // TODO(falken): Change the error code to SERVICE_WORKER_ERROR_TIMEOUT.
  embedded_worker_->AddMessageToConsole(blink::WebConsoleMessage::kLevelVerbose,
                                        kNotRespondingErrorMesage);
  StopWorkerIfIdle();
}

void ServiceWorkerVersion::StopWorkerIfIdle() {
  if (HasWork() && !ping_controller_->IsTimedOut())
    return;
  if (running_status() == EmbeddedWorkerStatus::STOPPED ||
      running_status() == EmbeddedWorkerStatus::STOPPING ||
      !stop_callbacks_.empty()) {
    return;
  }

  embedded_worker_->StopIfIdle();
}

bool ServiceWorkerVersion::HasWork() const {
  return !pending_requests_.IsEmpty() || !streaming_url_request_jobs_.empty() ||
         !start_callbacks_.empty();
}

void ServiceWorkerVersion::RecordStartWorkerResult(
    ServiceWorkerMetrics::EventType purpose,
    Status prestart_status,
    int trace_id,
    bool is_browser_startup_complete,
    ServiceWorkerStatusCode status) {
  if (trace_id != kInvalidTraceId) {
    TRACE_EVENT_ASYNC_END1("ServiceWorker", "ServiceWorkerVersion::StartWorker",
                           trace_id, "Status",
                           ServiceWorkerStatusToString(status));
  }
  base::TimeTicks start_time = start_time_;
  ClearTick(&start_time_);

  if (context_ && IsInstalled(prestart_status))
    context_->UpdateVersionFailureCount(version_id_, status);

  if (installed_scripts_sender_) {
    ServiceWorkerMetrics::RecordInstalledScriptsSenderStatus(
        installed_scripts_sender_->finished_reason());
  }
  ServiceWorkerMetrics::RecordStartWorkerStatus(status, purpose,
                                                IsInstalled(prestart_status));

  if (status == SERVICE_WORKER_OK && !start_time.is_null() &&
      !skip_recording_startup_time_) {
    ServiceWorkerMetrics::RecordStartWorkerTime(
        GetTickDuration(start_time), IsInstalled(prestart_status),
        embedded_worker_->start_situation(), purpose);
  }

  if (status != SERVICE_WORKER_ERROR_TIMEOUT)
    return;
  EmbeddedWorkerInstance::StartingPhase phase =
      EmbeddedWorkerInstance::NOT_STARTING;
  EmbeddedWorkerStatus running_status = embedded_worker_->status();
  // Build an artifical JavaScript exception to show in the ServiceWorker
  // log for developers; it's not user-facing so it's not a localized resource.
  std::string message = "ServiceWorker startup timed out. ";
  if (running_status != EmbeddedWorkerStatus::STARTING) {
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

bool ServiceWorkerVersion::MaybeTimeOutRequest(const RequestInfo& info) {
  PendingRequest* request = pending_requests_.Lookup(info.id);
  if (!request)
    return false;

  TRACE_EVENT_ASYNC_END1("ServiceWorker", "ServiceWorkerVersion::Request",
                         request, "Error", "Timeout");
  request->error_callback.Run(SERVICE_WORKER_ERROR_TIMEOUT);
  pending_requests_.Remove(info.id);
  return true;
}

void ServiceWorkerVersion::SetAllRequestExpirations(
    const base::TimeTicks& expiration) {
  RequestInfoPriorityQueue new_requests;
  while (!timeout_queue_.empty()) {
    RequestInfo info = timeout_queue_.top();
    info.expiration = expiration;
    new_requests.push(info);
    timeout_queue_.pop();
  }
  timeout_queue_ = new_requests;
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
      clock_->Now() - registration->last_update_check();
  if (time_since_last_check > kServiceWorkerScriptMaxCacheAge)
    RestartTick(&stale_time_);
}

void ServiceWorkerVersion::FoundRegistrationForUpdate(
    ServiceWorkerStatusCode status,
    scoped_refptr<ServiceWorkerRegistration> registration) {
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

void ServiceWorkerVersion::OnStoppedInternal(EmbeddedWorkerStatus old_status) {
  DCHECK_EQ(EmbeddedWorkerStatus::STOPPED, running_status());
  scoped_refptr<ServiceWorkerVersion> protect;
  if (!in_dtor_)
    protect = this;

  event_recorder_.reset();

  // |start_callbacks_| can be non-empty if a start worker request arrived while
  // the worker was stopping. The worker must be restarted to fulfill the
  // request.
  bool should_restart = !start_callbacks_.empty();
  if (is_redundant() || in_dtor_) {
    // This worker will be destroyed soon.
    should_restart = false;
  } else if (ping_controller_->IsTimedOut()) {
    // This worker is unresponsive and restart may fail.
    should_restart = false;
  } else if (old_status == EmbeddedWorkerStatus::STARTING) {
    // This worker unexpectedly stopped because start failed.  Attempting to
    // restart on start failure could cause an endless loop of start attempts,
    // so don't try to restart now.
    should_restart = false;
  }

  if (!stop_time_.is_null()) {
    TRACE_EVENT_ASYNC_END1("ServiceWorker", "ServiceWorkerVersion::StopWorker",
                           stop_time_.ToInternalValue(), "Restart",
                           should_restart);
    ClearTick(&stop_time_);
  }
  StopTimeoutTimer();

  // Fire all stop callbacks.
  RunCallbacks(this, &stop_callbacks_, SERVICE_WORKER_OK);

  if (!should_restart) {
    // Let all start callbacks fail.
    FinishStartWorker(DeduceStartWorkerFailureReason(
        SERVICE_WORKER_ERROR_START_WORKER_FAILED));
  }

  // Let all message callbacks fail (this will also fire and clear all
  // callbacks for events).
  // TODO(kinuko): Consider if we want to add queue+resend mechanism here.
  base::IDMap<std::unique_ptr<PendingRequest>>::iterator iter(
      &pending_requests_);
  while (!iter.IsAtEnd()) {
    TRACE_EVENT_ASYNC_END1("ServiceWorker", "ServiceWorkerVersion::Request",
                           iter.GetCurrentValue(), "Error", "Worker Stopped");
    iter.GetCurrentValue()->error_callback.Run(SERVICE_WORKER_ERROR_FAILED);
    iter.Advance();
  }
  pending_requests_.Clear();
  external_request_uuid_to_request_id_.clear();
  event_dispatcher_.reset();
  installed_scripts_sender_.reset();

  // TODO(falken): Call SWURLRequestJob::ClearStream here?
  streaming_url_request_jobs_.clear();

  for (auto& observer : listeners_)
    observer.OnRunningStateChanged(this);
  if (should_restart) {
    StartWorkerInternal();
  } else if (!HasWork()) {
    for (auto& observer : listeners_)
      observer.OnNoWork(this);
  }
}

void ServiceWorkerVersion::FinishStartWorker(ServiceWorkerStatusCode status) {
  start_worker_first_purpose_ = base::nullopt;
  RunCallbacks(this, &start_callbacks_, status);
}

void ServiceWorkerVersion::CleanUpExternalRequest(
    const std::string& request_uuid,
    ServiceWorkerStatusCode status) {
  if (status == SERVICE_WORKER_OK)
    return;
  external_request_uuid_to_request_id_.erase(request_uuid);
}

}  // namespace content
