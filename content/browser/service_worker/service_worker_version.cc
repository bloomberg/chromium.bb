// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_version.h"

#include "base/command_line.h"
#include "base/memory/ref_counted.h"
#include "base/metrics/histogram_macros.h"
#include "base/stl_util.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/message_port_message_filter.h"
#include "content/browser/message_port_service.h"
#include "content/browser/service_worker/embedded_worker_instance.h"
#include "content/browser/service_worker/embedded_worker_registry.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/service_worker/service_worker_registration.h"
#include "content/browser/service_worker/service_worker_utils.h"
#include "content/browser/storage_partition_impl.h"
#include "content/common/service_worker/service_worker_messages.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/child_process_host.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/result_codes.h"
#include "net/http/http_response_info.h"

namespace content {

using StatusCallback = ServiceWorkerVersion::StatusCallback;
using GetClientsCallback =
    base::Callback<void(const std::vector<ServiceWorkerClientInfo>&)>;

namespace {

// Delay between the timeout timer firing.
const int kTimeoutTimerDelaySeconds = 30;

// Time to wait until stopping an idle worker.
const int kIdleWorkerTimeoutSeconds = 30;

// Default delay for scheduled update.
const int kUpdateDelaySeconds = 1;

// Timeout for waiting for a response to a ping.
const int kPingTimeoutSeconds = 30;

// Timeout for the worker to start.
const int kStartWorkerTimeoutMinutes = 5;

// If the SW was destructed while starting up, how many seconds it
// had to start up for this to be considered a timeout occurrence.
const int kDestructedStartingWorkerTimeoutThresholdSeconds = 5;  // 5 secs.

const char kClaimClientsStateErrorMesage[] =
    "Only the active worker can claim clients.";

const char kClaimClientsShutdownErrorMesage[] =
    "Failed to claim clients due to Service Worker system shutdown.";

void RunSoon(const base::Closure& callback) {
  if (!callback.is_null())
    base::MessageLoop::current()->PostTask(FROM_HERE, callback);
}

template <typename CallbackArray, typename Arg>
void RunCallbacks(ServiceWorkerVersion* version,
                  CallbackArray* callbacks_ptr,
                  const Arg& arg) {
  CallbackArray callbacks;
  callbacks.swap(*callbacks_ptr);
  scoped_refptr<ServiceWorkerVersion> protect(version);
  for (const auto& callback : callbacks)
    callback.Run(arg);
}

template <typename IDMAP, typename... Params>
void RunIDMapCallbacks(IDMAP* callbacks, const Params&... params) {
  typename IDMAP::iterator iter(callbacks);
  while (!iter.IsAtEnd()) {
    iter.GetCurrentValue()->Run(params...);
    iter.Advance();
  }
  callbacks->Clear();
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

void RunErrorCrossOriginConnectCallback(
    const ServiceWorkerVersion::CrossOriginConnectCallback& callback,
    ServiceWorkerStatusCode status) {
  callback.Run(status, false);
}

using WindowOpenedCallback = base::Callback<void(int, int)>;

// The WindowOpenedObserver class is a WebContentsObserver that will wait for a
// new Window's WebContents to be initialized, run the |callback| passed to its
// constructor then self destroy.
// The callback will receive the process and frame ids. If something went wrong
// those will be (kInvalidUniqueID, MSG_ROUTING_NONE).
// The callback will be called in the IO thread.
class WindowOpenedObserver : public WebContentsObserver {
 public:
  WindowOpenedObserver(WebContents* web_contents,
                       const WindowOpenedCallback& callback)
    : WebContentsObserver(web_contents),
      callback_(callback)
  {}

  void DidCommitProvisionalLoadForFrame(
      RenderFrameHost* render_frame_host,
      const GURL& validated_url,
      ui::PageTransition transition_type) override {
    DCHECK(web_contents());

    if (render_frame_host != web_contents()->GetMainFrame())
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

  const WindowOpenedCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(WindowOpenedObserver);
};

void DidOpenURL(const WindowOpenedCallback& callback,
                WebContents* web_contents) {
  DCHECK(web_contents);

  new WindowOpenedObserver(web_contents, callback);
}

void OpenWindowOnUI(
    const GURL& url,
    const GURL& script_url,
    int process_id,
    const scoped_refptr<ServiceWorkerContextWrapper>& context_wrapper,
    const WindowOpenedCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  BrowserContext* browser_context = context_wrapper->storage_partition()
      ? context_wrapper->storage_partition()->browser_context()
      : nullptr;
  // We are shutting down.
  if (!browser_context)
    return;

  RenderProcessHost* render_process_host =
      RenderProcessHost::FromID(process_id);
  if (render_process_host->IsIsolatedGuest()) {
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
  if (render_process_host->GetHandle() != base::kNullProcessHandle)
    render_process_host->ReceivedBadMessage();
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

void OnGetClientsFromUI(
    // The tuple contains process_id, frame_id, client_id.
    const std::vector<Tuple<int,int,int>>& clients_info,
    const GURL& script_url,
    const GetClientsCallback& callback) {
  std::vector<ServiceWorkerClientInfo> clients;

  for (const auto& it : clients_info) {
    ServiceWorkerClientInfo info =
        ServiceWorkerProviderHost::GetClientInfoOnUI(get<0>(it), get<1>(it));

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
      return;

    info.client_id = get<2>(it);
    clients.push_back(info);
  }

  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
                          base::Bind(callback, clients));
}

}  // namespace

ServiceWorkerVersion::ServiceWorkerVersion(
    ServiceWorkerRegistration* registration,
    const GURL& script_url,
    int64 version_id,
    base::WeakPtr<ServiceWorkerContextCore> context)
    : version_id_(version_id),
      registration_id_(kInvalidServiceWorkerVersionId),
      script_url_(script_url),
      status_(NEW),
      context_(context),
      script_cache_map_(this, context),
      ping_timed_out_(false),
      is_doomed_(false),
      skip_waiting_(false),
      weak_factory_(this) {
  DCHECK(context_);
  DCHECK(registration);
  if (registration) {
    registration_id_ = registration->id();
    scope_ = registration->pattern();
  }
  context_->AddLiveVersion(this);
  embedded_worker_ = context_->embedded_worker_registry()->CreateWorker();
  embedded_worker_->AddListener(this);
}

ServiceWorkerVersion::~ServiceWorkerVersion() {
  // The user may have closed the tab waiting for SW to start up.
  if (start_worker_timeout_timer_.IsRunning() &&
      GetTickDuration(start_timing_) >
          base::TimeDelta::FromSeconds(
              kDestructedStartingWorkerTimeoutThresholdSeconds)) {
    RecordStartWorkerResult(SERVICE_WORKER_ERROR_TIMEOUT);
  }

  embedded_worker_->RemoveListener(this);
  if (context_)
    context_->RemoveLiveVersion(version_id_);
  // EmbeddedWorker's dtor sends StopWorker if it's still running.
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
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  return ServiceWorkerVersionInfo(
      running_status(),
      status(),
      script_url(),
      version_id(),
      embedded_worker()->process_id(),
      embedded_worker()->thread_id(),
      embedded_worker()->worker_devtools_agent_route_id());
}

void ServiceWorkerVersion::StartWorker(const StatusCallback& callback) {
  StartWorker(false, callback);
}

void ServiceWorkerVersion::StartWorker(
    bool pause_after_download,
    const StatusCallback& callback) {
  if (is_doomed()) {
    RunSoon(base::Bind(callback, SERVICE_WORKER_ERROR_START_WORKER_FAILED));
    return;
  }
  switch (running_status()) {
    case RUNNING:
      RunSoon(base::Bind(callback, SERVICE_WORKER_OK));
      return;
    case STOPPING:
    case STOPPED:
    case STARTING:
      if (!start_worker_timeout_timer_.IsRunning())
        ScheduleStartWorkerTimeout();
      start_callbacks_.push_back(callback);
      if (running_status() == STOPPED) {
        DCHECK(!cache_listener_.get());
        cache_listener_.reset(new ServiceWorkerCacheListener(this, context_));
        embedded_worker_->Start(
            version_id_, scope_, script_url_, pause_after_download,
            base::Bind(&ServiceWorkerVersion::OnStartSentAndScriptEvaluated,
                       weak_factory_.GetWeakPtr()));
      }
      return;
  }
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
  if (update_timer_.IsRunning()) {
    update_timer_.Reset();
    return;
  }
  update_timer_.Start(
      FROM_HERE, base::TimeDelta::FromSeconds(kUpdateDelaySeconds),
      base::Bind(&ServiceWorkerVersion::StartUpdate,
                 weak_factory_.GetWeakPtr()));
}

void ServiceWorkerVersion::DeferScheduledUpdate() {
  if (update_timer_.IsRunning())
    update_timer_.Reset();
}

void ServiceWorkerVersion::StartUpdate() {
  update_timer_.Stop();
  if (!context_)
    return;
  ServiceWorkerRegistration* registration =
      context_->GetLiveRegistration(registration_id_);
  if (!registration || !registration->GetNewestVersion())
    return;
  context_->UpdateServiceWorker(registration);
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

  int request_id = fetch_callbacks_.Add(new FetchCallback(fetch_callback));
  ServiceWorkerStatusCode status = embedded_worker_->SendMessage(
      ServiceWorkerMsg_FetchEvent(request_id, request));
  if (status != SERVICE_WORKER_OK) {
    fetch_callbacks_.Remove(request_id);
    RunSoon(base::Bind(&RunErrorFetchCallback,
                       fetch_callback,
                       SERVICE_WORKER_ERROR_FAILED));
  }
}

void ServiceWorkerVersion::DispatchSyncEvent(const StatusCallback& callback) {
  DCHECK_EQ(ACTIVATED, status()) << status();

  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableServiceWorkerSync)) {
    callback.Run(SERVICE_WORKER_ERROR_ABORT);
    return;
  }

  if (running_status() != RUNNING) {
    // Schedule calling this method after starting the worker.
    StartWorker(base::Bind(&RunTaskAfterStartWorker,
                           weak_factory_.GetWeakPtr(), callback,
                           base::Bind(&self::DispatchSyncEvent,
                                      weak_factory_.GetWeakPtr(),
                                      callback)));
    return;
  }

  int request_id = sync_callbacks_.Add(new StatusCallback(callback));
  ServiceWorkerStatusCode status = embedded_worker_->SendMessage(
      ServiceWorkerMsg_SyncEvent(request_id));
  if (status != SERVICE_WORKER_OK) {
    sync_callbacks_.Remove(request_id);
    RunSoon(base::Bind(callback, status));
  }
}

void ServiceWorkerVersion::DispatchNotificationClickEvent(
    const StatusCallback& callback,
    const std::string& notification_id,
    const PlatformNotificationData& notification_data) {
  DCHECK_EQ(ACTIVATED, status()) << status();
  if (running_status() != RUNNING) {
    // Schedule calling this method after starting the worker.
    StartWorker(base::Bind(&RunTaskAfterStartWorker,
                           weak_factory_.GetWeakPtr(), callback,
                           base::Bind(&self::DispatchNotificationClickEvent,
                                      weak_factory_.GetWeakPtr(),
                                      callback, notification_id,
                                      notification_data)));
    return;
  }

  int request_id =
      notification_click_callbacks_.Add(new StatusCallback(callback));
  ServiceWorkerStatusCode status = embedded_worker_->SendMessage(
      ServiceWorkerMsg_NotificationClickEvent(request_id,
                                              notification_id,
                                              notification_data));
  if (status != SERVICE_WORKER_OK) {
    notification_click_callbacks_.Remove(request_id);
    RunSoon(base::Bind(callback, status));
  }
}

void ServiceWorkerVersion::DispatchPushEvent(const StatusCallback& callback,
                                             const std::string& data) {
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

  int request_id = push_callbacks_.Add(new StatusCallback(callback));
  ServiceWorkerStatusCode status = embedded_worker_->SendMessage(
      ServiceWorkerMsg_PushEvent(request_id, data));
  if (status != SERVICE_WORKER_OK) {
    push_callbacks_.Remove(request_id);
    RunSoon(base::Bind(callback, status));
  }
}

void ServiceWorkerVersion::DispatchGeofencingEvent(
    const StatusCallback& callback,
    blink::WebGeofencingEventType event_type,
    const std::string& region_id,
    const blink::WebCircularGeofencingRegion& region) {
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

  int request_id = geofencing_callbacks_.Add(new StatusCallback(callback));
  ServiceWorkerStatusCode status =
      embedded_worker_->SendMessage(ServiceWorkerMsg_GeofencingEvent(
          request_id, event_type, region_id, region));
  if (status != SERVICE_WORKER_OK) {
    geofencing_callbacks_.Remove(request_id);
    RunSoon(base::Bind(callback, status));
  }
}

void ServiceWorkerVersion::DispatchCrossOriginConnectEvent(
    const CrossOriginConnectCallback& callback,
    const NavigatorConnectClient& client) {
  DCHECK_EQ(ACTIVATED, status()) << status();

  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableExperimentalWebPlatformFeatures)) {
    callback.Run(SERVICE_WORKER_ERROR_ABORT, false);
    return;
  }

  if (running_status() != RUNNING) {
    // Schedule calling this method after starting the worker.
    StartWorker(
        base::Bind(&RunTaskAfterStartWorker, weak_factory_.GetWeakPtr(),
                   base::Bind(&RunErrorCrossOriginConnectCallback, callback),
                   base::Bind(&self::DispatchCrossOriginConnectEvent,
                              weak_factory_.GetWeakPtr(), callback, client)));
    return;
  }

  int request_id = cross_origin_connect_callbacks_.Add(
      new CrossOriginConnectCallback(callback));
  ServiceWorkerStatusCode status = embedded_worker_->SendMessage(
      ServiceWorkerMsg_CrossOriginConnectEvent(request_id, client));
  if (status != SERVICE_WORKER_OK) {
    cross_origin_connect_callbacks_.Remove(request_id);
    RunSoon(base::Bind(callback, status, false));
  }
}

void ServiceWorkerVersion::DispatchCrossOriginMessageEvent(
    const NavigatorConnectClient& client,
    const base::string16& message,
    const std::vector<TransferredMessagePort>& sent_message_ports,
    const StatusCallback& callback) {
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
  DCHECK(!ContainsKey(controllee_map_, provider_host));
  int controllee_id = controllee_by_id_.Add(provider_host);
  // IDMap<>'s last index is kInvalidServiceWorkerClientId.
  CHECK(controllee_id != kInvalidServiceWorkerClientId);
  controllee_map_[provider_host] = controllee_id;
  // Keep the worker alive a bit longer right after a new controllee is added.
  RestartTick(&idle_time_);
}

void ServiceWorkerVersion::RemoveControllee(
    ServiceWorkerProviderHost* provider_host) {
  ControlleeMap::iterator found = controllee_map_.find(provider_host);
  DCHECK(found != controllee_map_.end());
  controllee_by_id_.Remove(found->second);
  controllee_map_.erase(found);
  if (HasControllee())
    return;
  FOR_EACH_OBSERVER(Listener, listeners_, OnNoControllees(this));
  if (is_doomed_) {
    DoomInternal();
    return;
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
  if (is_doomed_)
    StopWorkerIfIdle();
}

void ServiceWorkerVersion::AddListener(Listener* listener) {
  listeners_.AddObserver(listener);
}

void ServiceWorkerVersion::RemoveListener(Listener* listener) {
  listeners_.RemoveObserver(listener);
}

void ServiceWorkerVersion::Doom() {
  if (is_doomed_)
    return;
  is_doomed_ = true;
  if (!HasControllee())
    DoomInternal();
}

void ServiceWorkerVersion::SetDevToolsAttached(bool attached) {
  embedded_worker()->set_devtools_attached(attached);
  if (attached) {
    // Set to null time so we don't record the startup time metric.
    ClearTick(&start_timing_);
    return;
  }
  // If devtools is detached try scheduling the timers for stopping the worker
  // now.
  if (!timeout_timer_.IsRunning())
    StartTimeoutTimer();
  if (!start_worker_timeout_timer_.IsRunning() && !start_callbacks_.empty())
    ScheduleStartWorkerTimeout();
}

void ServiceWorkerVersion::SetMainScriptHttpResponseInfo(
    const net::HttpResponseInfo& http_info) {
  main_script_http_info_.reset(new net::HttpResponseInfo(http_info));
}

const net::HttpResponseInfo*
ServiceWorkerVersion::GetMainScriptHttpResponseInfo() {
  return main_script_http_info_.get();
}

void ServiceWorkerVersion::OnScriptLoaded() {
  DCHECK_EQ(STARTING, running_status());
  StartTimeoutTimer();
}

void ServiceWorkerVersion::OnStarted() {
  DCHECK_EQ(RUNNING, running_status());
  DCHECK(cache_listener_.get());
  RestartTick(&idle_time_);

  // Fire all start callbacks.
  scoped_refptr<ServiceWorkerVersion> protect(this);
  RunCallbacks(this, &start_callbacks_, SERVICE_WORKER_OK);
  FOR_EACH_OBSERVER(Listener, listeners_, OnWorkerStarted(this));
}

void ServiceWorkerVersion::OnStopped(
    EmbeddedWorkerInstance::Status old_status) {
  DCHECK_EQ(STOPPED, running_status());
  scoped_refptr<ServiceWorkerVersion> protect(this);

  bool should_restart = !is_doomed() && !start_callbacks_.empty() &&
                        (old_status != EmbeddedWorkerInstance::STARTING);

  StopTimeoutTimer();
  if (ping_timed_out_)
    should_restart = false;

  // Fire all stop callbacks.
  RunCallbacks(this, &stop_callbacks_, SERVICE_WORKER_OK);

  if (!should_restart) {
    // Let all start callbacks fail.
    RunCallbacks(this, &start_callbacks_,
                 SERVICE_WORKER_ERROR_START_WORKER_FAILED);
  }

  // Let all message callbacks fail (this will also fire and clear all
  // callbacks for events).
  // TODO(kinuko): Consider if we want to add queue+resend mechanism here.
  RunIDMapCallbacks(&activate_callbacks_,
                    SERVICE_WORKER_ERROR_ACTIVATE_WORKER_FAILED);
  RunIDMapCallbacks(&install_callbacks_,
                    SERVICE_WORKER_ERROR_INSTALL_WORKER_FAILED);
  RunIDMapCallbacks(&fetch_callbacks_,
                    SERVICE_WORKER_ERROR_FAILED,
                    SERVICE_WORKER_FETCH_EVENT_RESULT_FALLBACK,
                    ServiceWorkerResponse());
  RunIDMapCallbacks(&sync_callbacks_,
                    SERVICE_WORKER_ERROR_FAILED);
  RunIDMapCallbacks(&notification_click_callbacks_,
                    SERVICE_WORKER_ERROR_FAILED);
  RunIDMapCallbacks(&push_callbacks_,
                    SERVICE_WORKER_ERROR_FAILED);
  RunIDMapCallbacks(&geofencing_callbacks_,
                    SERVICE_WORKER_ERROR_FAILED);
  RunIDMapCallbacks(&cross_origin_connect_callbacks_,
                    SERVICE_WORKER_ERROR_FAILED,
                    false);

  streaming_url_request_jobs_.clear();

  FOR_EACH_OBSERVER(Listener, listeners_, OnWorkerStopped(this));

  // There should be no more communication from/to a stopped worker. Deleting
  // the listener prevents any pending completion callbacks from causing
  // messages to be sent to the stopped worker.
  cache_listener_.reset();

  // Restart worker if we have any start callbacks and the worker isn't doomed.
  if (should_restart) {
    start_worker_timeout_timer_.Reset();
    cache_listener_.reset(new ServiceWorkerCacheListener(this, context_));
    embedded_worker_->Start(
        version_id_, scope_, script_url_, false /* pause_after_download */,
        base::Bind(&ServiceWorkerVersion::OnStartSentAndScriptEvaluated,
                   weak_factory_.GetWeakPtr()));
  }
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
    IPC_MESSAGE_HANDLER(ServiceWorkerHostMsg_SyncEventFinished,
                        OnSyncEventFinished)
    IPC_MESSAGE_HANDLER(ServiceWorkerHostMsg_NotificationClickEventFinished,
                        OnNotificationClickEventFinished)
    IPC_MESSAGE_HANDLER(ServiceWorkerHostMsg_PushEventFinished,
                        OnPushEventFinished)
    IPC_MESSAGE_HANDLER(ServiceWorkerHostMsg_GeofencingEventFinished,
                        OnGeofencingEventFinished)
    IPC_MESSAGE_HANDLER(ServiceWorkerHostMsg_CrossOriginConnectEventFinished,
                        OnCrossOriginConnectEventFinished)
    IPC_MESSAGE_HANDLER(ServiceWorkerHostMsg_OpenWindow,
                        OnOpenWindow)
    IPC_MESSAGE_HANDLER(ServiceWorkerHostMsg_SetCachedMetadata,
                        OnSetCachedMetadata)
    IPC_MESSAGE_HANDLER(ServiceWorkerHostMsg_ClearCachedMetadata,
                        OnClearCachedMetadata)
    IPC_MESSAGE_HANDLER(ServiceWorkerHostMsg_PostMessageToDocument,
                        OnPostMessageToDocument)
    IPC_MESSAGE_HANDLER(ServiceWorkerHostMsg_FocusClient,
                        OnFocusClient)
    IPC_MESSAGE_HANDLER(ServiceWorkerHostMsg_SkipWaiting,
                        OnSkipWaiting)
    IPC_MESSAGE_HANDLER(ServiceWorkerHostMsg_ClaimClients,
                        OnClaimClients)
    IPC_MESSAGE_HANDLER(ServiceWorkerHostMsg_Pong, OnPongFromWorker)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void ServiceWorkerVersion::OnStartSentAndScriptEvaluated(
    ServiceWorkerStatusCode status) {
  if (status != SERVICE_WORKER_OK)
    RunCallbacks(this, &start_callbacks_, status);
}

void ServiceWorkerVersion::DispatchInstallEventAfterStartWorker(
    const StatusCallback& callback) {
  DCHECK_EQ(RUNNING, running_status())
      << "Worker stopped too soon after it was started.";

  int request_id = install_callbacks_.Add(new StatusCallback(callback));
  ServiceWorkerStatusCode status = embedded_worker_->SendMessage(
      ServiceWorkerMsg_InstallEvent(request_id));
  if (status != SERVICE_WORKER_OK) {
    install_callbacks_.Remove(request_id);
    RunSoon(base::Bind(callback, status));
  }
}

void ServiceWorkerVersion::DispatchActivateEventAfterStartWorker(
    const StatusCallback& callback) {
  DCHECK_EQ(RUNNING, running_status())
      << "Worker stopped too soon after it was started.";

  int request_id = activate_callbacks_.Add(new StatusCallback(callback));
  ServiceWorkerStatusCode status =
      embedded_worker_->SendMessage(ServiceWorkerMsg_ActivateEvent(request_id));
  if (status != SERVICE_WORKER_OK) {
    activate_callbacks_.Remove(request_id);
    RunSoon(base::Bind(callback, status));
  }
}

void ServiceWorkerVersion::OnGetClients(
    int request_id,
    const ServiceWorkerClientQueryOptions& /* options */) {
  // TODO(kinuko): Handle ClientQueryOptions. (crbug.com/455241, 460415 etc)
  if (controllee_by_id_.IsEmpty()) {
    if (running_status() == RUNNING) {
      embedded_worker_->SendMessage(
          ServiceWorkerMsg_DidGetClients(request_id,
              std::vector<ServiceWorkerClientInfo>()));
    }
    return;
  }

  TRACE_EVENT0("ServiceWorker",
               "ServiceWorkerVersion::OnGetClients");

  std::vector<Tuple<int,int,int>> clients_info;
  for (ControlleeByIDMap::iterator it(&controllee_by_id_); !it.IsAtEnd();
       it.Advance()) {
    int process_id = it.GetCurrentValue()->process_id();
    int frame_id = it.GetCurrentValue()->frame_id();
    int client_id = it.GetCurrentKey();

    clients_info.push_back(MakeTuple(process_id, frame_id, client_id));
  }

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&OnGetClientsFromUI, clients_info, script_url_,
                 base::Bind(&ServiceWorkerVersion::DidGetClients,
                            weak_factory_.GetWeakPtr(),
                            request_id)));

}

void ServiceWorkerVersion::OnActivateEventFinished(
    int request_id,
    blink::WebServiceWorkerEventResult result) {
  DCHECK(ACTIVATING == status() ||
         REDUNDANT == status()) << status();
  TRACE_EVENT0("ServiceWorker",
               "ServiceWorkerVersion::OnActivateEventFinished");

  StatusCallback* callback = activate_callbacks_.Lookup(request_id);
  if (!callback) {
    NOTREACHED() << "Got unexpected message: " << request_id;
    return;
  }
  ServiceWorkerStatusCode rv = SERVICE_WORKER_OK;
  if (result == blink::WebServiceWorkerEventResultRejected ||
      status() != ACTIVATING) {
    rv = SERVICE_WORKER_ERROR_ACTIVATE_WORKER_FAILED;
  }

  scoped_refptr<ServiceWorkerVersion> protect(this);
  callback->Run(rv);
  RemoveCallbackAndStopIfDoomed(&activate_callbacks_, request_id);
}

void ServiceWorkerVersion::OnInstallEventFinished(
    int request_id,
    blink::WebServiceWorkerEventResult result) {
  // Status is REDUNDANT if the worker was doomed while handling the install
  // event, and finished handling before being terminated.
  DCHECK(status() == INSTALLING || status() == REDUNDANT) << status();
  TRACE_EVENT0("ServiceWorker",
               "ServiceWorkerVersion::OnInstallEventFinished");

  StatusCallback* callback = install_callbacks_.Lookup(request_id);
  if (!callback) {
    NOTREACHED() << "Got unexpected message: " << request_id;
    return;
  }
  ServiceWorkerStatusCode status = SERVICE_WORKER_OK;
  if (result == blink::WebServiceWorkerEventResultRejected)
    status = SERVICE_WORKER_ERROR_INSTALL_WORKER_FAILED;

  scoped_refptr<ServiceWorkerVersion> protect(this);
  callback->Run(status);
  RemoveCallbackAndStopIfDoomed(&install_callbacks_, request_id);
}

void ServiceWorkerVersion::OnFetchEventFinished(
    int request_id,
    ServiceWorkerFetchEventResult result,
    const ServiceWorkerResponse& response) {
  TRACE_EVENT1("ServiceWorker",
               "ServiceWorkerVersion::OnFetchEventFinished",
               "Request id", request_id);
  FetchCallback* callback = fetch_callbacks_.Lookup(request_id);
  if (!callback) {
    NOTREACHED() << "Got unexpected message: " << request_id;
    return;
  }

  scoped_refptr<ServiceWorkerVersion> protect(this);
  callback->Run(SERVICE_WORKER_OK, result, response);
  RemoveCallbackAndStopIfDoomed(&fetch_callbacks_, request_id);
}

void ServiceWorkerVersion::OnSyncEventFinished(
    int request_id) {
  TRACE_EVENT1("ServiceWorker",
               "ServiceWorkerVersion::OnSyncEventFinished",
               "Request id", request_id);
  StatusCallback* callback = sync_callbacks_.Lookup(request_id);
  if (!callback) {
    NOTREACHED() << "Got unexpected message: " << request_id;
    return;
  }

  scoped_refptr<ServiceWorkerVersion> protect(this);
  callback->Run(SERVICE_WORKER_OK);
  RemoveCallbackAndStopIfDoomed(&sync_callbacks_, request_id);
}

void ServiceWorkerVersion::OnNotificationClickEventFinished(
    int request_id) {
  TRACE_EVENT1("ServiceWorker",
               "ServiceWorkerVersion::OnNotificationClickEventFinished",
               "Request id", request_id);
  StatusCallback* callback = notification_click_callbacks_.Lookup(request_id);
  if (!callback) {
    NOTREACHED() << "Got unexpected message: " << request_id;
    return;
  }

  scoped_refptr<ServiceWorkerVersion> protect(this);
  callback->Run(SERVICE_WORKER_OK);
  RemoveCallbackAndStopIfDoomed(&notification_click_callbacks_, request_id);
}

void ServiceWorkerVersion::OnPushEventFinished(
    int request_id,
    blink::WebServiceWorkerEventResult result) {
  TRACE_EVENT1("ServiceWorker",
               "ServiceWorkerVersion::OnPushEventFinished",
               "Request id", request_id);
  StatusCallback* callback = push_callbacks_.Lookup(request_id);
  if (!callback) {
    NOTREACHED() << "Got unexpected message: " << request_id;
    return;
  }
  ServiceWorkerStatusCode status = SERVICE_WORKER_OK;
  if (result == blink::WebServiceWorkerEventResultRejected)
    status = SERVICE_WORKER_ERROR_EVENT_WAITUNTIL_REJECTED;

  scoped_refptr<ServiceWorkerVersion> protect(this);
  callback->Run(status);
  RemoveCallbackAndStopIfDoomed(&push_callbacks_, request_id);
}

void ServiceWorkerVersion::OnGeofencingEventFinished(int request_id) {
  TRACE_EVENT1("ServiceWorker",
               "ServiceWorkerVersion::OnGeofencingEventFinished",
               "Request id",
               request_id);
  StatusCallback* callback = geofencing_callbacks_.Lookup(request_id);
  if (!callback) {
    NOTREACHED() << "Got unexpected message: " << request_id;
    return;
  }

  scoped_refptr<ServiceWorkerVersion> protect(this);
  callback->Run(SERVICE_WORKER_OK);
  RemoveCallbackAndStopIfDoomed(&geofencing_callbacks_, request_id);
}

void ServiceWorkerVersion::OnCrossOriginConnectEventFinished(
    int request_id,
    bool accept_connection) {
  TRACE_EVENT1("ServiceWorker",
               "ServiceWorkerVersion::OnCrossOriginConnectEventFinished",
               "Request id", request_id);
  CrossOriginConnectCallback* callback =
      cross_origin_connect_callbacks_.Lookup(request_id);
  if (!callback) {
    NOTREACHED() << "Got unexpected message: " << request_id;
    return;
  }

  scoped_refptr<ServiceWorkerVersion> protect(this);
  callback->Run(SERVICE_WORKER_OK, accept_connection);
  RemoveCallbackAndStopIfDoomed(&cross_origin_connect_callbacks_, request_id);
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

  for (const auto& it : controllee_map_) {
    const ServiceWorkerProviderHost* provider_host = it.first;
    if (provider_host->process_id() != render_process_id ||
        provider_host->frame_id() != render_frame_id) {
      continue;
    }

    // it.second is the client_id associated with the provider_host.
    provider_host->GetClientInfo(
        base::Bind(&ServiceWorkerVersion::OnOpenWindowFinished,
                   weak_factory_.GetWeakPtr(), request_id, it.second));
    return;
  }

  // If here, it means that no provider_host was found, in which case, the
  // renderer should still be informed that the window was opened.
  OnOpenWindowFinished(request_id, 0, ServiceWorkerClientInfo());
}

void ServiceWorkerVersion::OnOpenWindowFinished(
    int request_id,
    int client_id,
    const ServiceWorkerClientInfo& client_info) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (running_status() != RUNNING)
    return;

  ServiceWorkerClientInfo client(client_info);

  // If the |client_info| is empty, it means that the opened window wasn't
  // controlled but the action still succeeded. The renderer process is
  // expecting an empty client in such case.
  if (!client.IsEmpty())
    client.client_id = client_id;

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

void ServiceWorkerVersion::OnPostMessageToDocument(
    int client_id,
    const base::string16& message,
    const std::vector<TransferredMessagePort>& sent_message_ports) {
  TRACE_EVENT1("ServiceWorker",
               "ServiceWorkerVersion::OnPostMessageToDocument",
               "Client id", client_id);
  ServiceWorkerProviderHost* provider_host =
      controllee_by_id_.Lookup(client_id);
  if (!provider_host) {
    // The client may already have been closed, just ignore.
    return;
  }
  provider_host->PostMessage(message, sent_message_ports);
}

void ServiceWorkerVersion::OnFocusClient(int request_id, int client_id) {
  TRACE_EVENT2("ServiceWorker",
               "ServiceWorkerVersion::OnFocusClient",
               "Request id", request_id,
               "Client id", client_id);
  ServiceWorkerProviderHost* provider_host =
      controllee_by_id_.Lookup(client_id);
  if (!provider_host) {
    // The client may already have been closed, just ignore.
    return;
  }

  provider_host->Focus(
      base::Bind(&ServiceWorkerVersion::OnFocusClientFinished,
                 weak_factory_.GetWeakPtr(),
                 request_id,
                 client_id));
}

void ServiceWorkerVersion::OnFocusClientFinished(
    int request_id,
    int cliend_id,
    const ServiceWorkerClientInfo& client) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (running_status() != RUNNING)
    return;

  ServiceWorkerClientInfo client_info(client);
  client_info.client_id = cliend_id;

  embedded_worker_->SendMessage(ServiceWorkerMsg_FocusClientResponse(
      request_id, client_info));
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
  StatusCallback callback = base::Bind(&ServiceWorkerVersion::DidClaimClients,
                                       weak_factory_.GetWeakPtr(), request_id);
  if (status_ != ACTIVATING && status_ != ACTIVATED) {
    callback.Run(SERVICE_WORKER_ERROR_STATE);
    return;
  }
  if (!context_) {
    callback.Run(SERVICE_WORKER_ERROR_ABORT);
    return;
  }

  ServiceWorkerRegistration* registration =
      context_->GetLiveRegistration(registration_id_);
  if (!registration) {
    callback.Run(SERVICE_WORKER_ERROR_ABORT);
    return;
  }
  registration->ClaimClients(callback);
}

void ServiceWorkerVersion::OnPongFromWorker() {
  ClearTick(&ping_time_);
}

void ServiceWorkerVersion::DidClaimClients(
    int request_id, ServiceWorkerStatusCode status) {
  if (status == SERVICE_WORKER_ERROR_STATE) {
    embedded_worker_->SendMessage(ServiceWorkerMsg_ClaimClientsError(
        request_id, blink::WebServiceWorkerError::ErrorTypeState,
        base::ASCIIToUTF16(kClaimClientsStateErrorMesage)));
    return;
  }
  if (status == SERVICE_WORKER_ERROR_ABORT) {
    embedded_worker_->SendMessage(ServiceWorkerMsg_ClaimClientsError(
        request_id, blink::WebServiceWorkerError::ErrorTypeAbort,
        base::ASCIIToUTF16(kClaimClientsShutdownErrorMesage)));
    return;
  }
  DCHECK(status == SERVICE_WORKER_OK);
  embedded_worker_->SendMessage(ServiceWorkerMsg_DidClaimClients(request_id));
}

void ServiceWorkerVersion::DidGetClients(
    int request_id,
    const std::vector<ServiceWorkerClientInfo>& clients) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (running_status() != RUNNING)
    return;

  embedded_worker_->SendMessage(
      ServiceWorkerMsg_DidGetClients(request_id, clients));
}

void ServiceWorkerVersion::StartTimeoutTimer() {
  DCHECK(!timeout_timer_.IsRunning());
  ClearTick(&idle_time_);
  ClearTick(&ping_time_);
  ping_timed_out_ = false;
  timeout_timer_.Start(FROM_HERE,
                       base::TimeDelta::FromSeconds(kTimeoutTimerDelaySeconds),
                       this, &ServiceWorkerVersion::OnTimeoutTimer);
}

void ServiceWorkerVersion::StopTimeoutTimer() {
  timeout_timer_.Stop();
}

void ServiceWorkerVersion::OnTimeoutTimer() {
  if (running_status() == STOPPING)
    return;
  DCHECK(running_status() == STARTING || running_status() == RUNNING);
  if (GetTickDuration(idle_time_) >
      base::TimeDelta::FromSeconds(kIdleWorkerTimeoutSeconds)) {
    StopWorkerIfIdle();
    StopTimeoutTimer();
    return;
  }
  if (GetTickDuration(ping_time_) >
      base::TimeDelta::FromSeconds(kPingTimeoutSeconds)) {
    OnPingTimeout();
    StopTimeoutTimer();
    return;
  }

  if (ping_time_.is_null())
    PingWorker();
}

void ServiceWorkerVersion::PingWorker() {
  DCHECK(running_status() == STARTING || running_status() == RUNNING);
  ServiceWorkerStatusCode status =
      embedded_worker_->SendMessage(ServiceWorkerMsg_Ping());
  if (status != SERVICE_WORKER_OK) {
    // TODO(falken): Maybe try resending Ping a few times first?
    ping_timed_out_ =  true;
    StopWorkerIfIdle();
    return;
  }
  RestartTick(&ping_time_);
}

void ServiceWorkerVersion::OnPingTimeout() {
  DCHECK(running_status() == STARTING || running_status() == RUNNING);
  ping_timed_out_ = true;
  // TODO(falken): Show a message to the developer that the SW was stopped due
  // to timeout (crbug.com/457968). Also, change the error code to
  // SERVICE_WORKER_ERROR_TIMEOUT.
  StopWorkerIfIdle();
}

void ServiceWorkerVersion::StopWorkerIfIdle() {
  if (HasInflightRequests() && !ping_timed_out_)
    return;
  if (running_status() == STOPPED || running_status() == STOPPING ||
      !stop_callbacks_.empty()) {
    return;
  }

  // TODO(falken): We may need to handle StopIfIdle failure and
  // forcibly fail pending callbacks so no one is stuck waiting
  // for the worker.
  embedded_worker_->StopIfIdle();
}

bool ServiceWorkerVersion::HasInflightRequests() const {
  return
    !activate_callbacks_.IsEmpty() ||
    !install_callbacks_.IsEmpty() ||
    !fetch_callbacks_.IsEmpty() ||
    !sync_callbacks_.IsEmpty() ||
    !notification_click_callbacks_.IsEmpty() ||
    !push_callbacks_.IsEmpty() ||
    !geofencing_callbacks_.IsEmpty() ||
    !cross_origin_connect_callbacks_.IsEmpty() ||
    !streaming_url_request_jobs_.empty();
}

void ServiceWorkerVersion::ScheduleStartWorkerTimeout() {
  DCHECK(!start_worker_timeout_timer_.IsRunning());
  start_timing_ = embedded_worker_->devtools_attached()
                      ? base::TimeTicks()
                      : base::TimeTicks::Now();
  start_callbacks_.push_back(
      base::Bind(&ServiceWorkerVersion::RecordStartWorkerResult,
                 weak_factory_.GetWeakPtr()));
  start_worker_timeout_timer_.Start(
      FROM_HERE, base::TimeDelta::FromMinutes(kStartWorkerTimeoutMinutes),
      base::Bind(&ServiceWorkerVersion::OnStartWorkerTimeout,
                 weak_factory_.GetWeakPtr()));
}

void ServiceWorkerVersion::OnStartWorkerTimeout() {
  DCHECK(running_status() == STARTING || running_status() == STOPPING)
      << running_status();

  if (embedded_worker_->devtools_attached()) {
    start_worker_timeout_timer_.Stop();
    return;
  }

  scoped_refptr<ServiceWorkerVersion> protect(this);
  RunCallbacks(this, &start_callbacks_, SERVICE_WORKER_ERROR_TIMEOUT);
  if (running_status() == STARTING)
    embedded_worker_->Stop();
}

void ServiceWorkerVersion::RecordStartWorkerResult(
    ServiceWorkerStatusCode status) {
  start_worker_timeout_timer_.Stop();

  UMA_HISTOGRAM_ENUMERATION("ServiceWorker.StartWorker.Status", status,
                            SERVICE_WORKER_ERROR_MAX_VALUE);
  if (status == SERVICE_WORKER_OK && !start_timing_.is_null()) {
    UMA_HISTOGRAM_MEDIUM_TIMES("ServiceWorker.StartWorker.Time",
                               GetTickDuration(start_timing_));
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

void ServiceWorkerVersion::DoomInternal() {
  DCHECK(is_doomed_);
  DCHECK(!HasControllee());
  SetStatus(REDUNDANT);
  StopWorkerIfIdle();
  if (!context_)
    return;
  std::vector<ServiceWorkerDatabase::ResourceRecord> resources;
  script_cache_map_.GetResources(&resources);
  context_->storage()->PurgeResources(resources);
}

template <typename IDMAP>
void ServiceWorkerVersion::RemoveCallbackAndStopIfDoomed(
    IDMAP* callbacks,
    int request_id) {
  callbacks->Remove(request_id);
  if (is_doomed_) {
    // The stop should be already scheduled, but try to stop immediately, in
    // order to release worker resources soon.
    StopWorkerIfIdle();
  }
}

}  // namespace content
