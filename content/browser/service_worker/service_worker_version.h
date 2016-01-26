// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_VERSION_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_VERSION_H_

#include <stdint.h>

#include <functional>
#include <map>
#include <queue>
#include <set>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/containers/scoped_ptr_hash_map.h"
#include "base/gtest_prod_util.h"
#include "base/id_map.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/observer_list.h"
#include "base/thread_task_runner_handle.h"
#include "base/timer/timer.h"
#include "content/browser/service_worker/embedded_worker_instance.h"
#include "content/browser/service_worker/service_worker_metrics.h"
#include "content/browser/service_worker/service_worker_script_cache_map.h"
#include "content/common/content_export.h"
#include "content/common/service_worker/service_worker_status_code.h"
#include "content/common/service_worker/service_worker_types.h"
#include "content/public/common/service_registry.h"
#include "ipc/ipc_message.h"
#include "third_party/WebKit/public/platform/modules/serviceworker/WebServiceWorkerEventResult.h"

// Windows headers will redefine SendMessage.
#ifdef SendMessage
#undef SendMessage
#endif

class GURL;

namespace net {
class HttpResponseInfo;
}

namespace content {

class EmbeddedWorkerRegistry;
class ServiceWorkerContextCore;
class ServiceWorkerProviderHost;
class ServiceWorkerRegistration;
class ServiceWorkerURLRequestJob;
struct NavigatorConnectClient;
struct ServiceWorkerClientInfo;
struct ServiceWorkerVersionInfo;
struct TransferredMessagePort;

// This class corresponds to a specific version of a ServiceWorker
// script for a given pattern. When a script is upgraded, there may be
// more than one ServiceWorkerVersion "running" at a time, but only
// one of them is activated. This class connects the actual script with a
// running worker.
class CONTENT_EXPORT ServiceWorkerVersion
    : NON_EXPORTED_BASE(public base::RefCounted<ServiceWorkerVersion>),
      public EmbeddedWorkerInstance::Listener {
 public:
  typedef base::Callback<void(ServiceWorkerStatusCode)> StatusCallback;
  typedef base::Callback<void(ServiceWorkerStatusCode,
                              ServiceWorkerFetchEventResult,
                              const ServiceWorkerResponse&)> FetchCallback;

  enum RunningStatus {
    STOPPED = EmbeddedWorkerInstance::STOPPED,
    STARTING = EmbeddedWorkerInstance::STARTING,
    RUNNING = EmbeddedWorkerInstance::RUNNING,
    STOPPING = EmbeddedWorkerInstance::STOPPING,
  };

  // Current version status; some of the status (e.g. INSTALLED and ACTIVATED)
  // should be persisted unlike running status.
  enum Status {
    NEW,         // The version is just created.
    INSTALLING,  // Install event is dispatched and being handled.
    INSTALLED,   // Install event is finished and is ready to be activated.
    ACTIVATING,  // Activate event is dispatched and being handled.
    ACTIVATED,   // Activation is finished and can run as activated.
    REDUNDANT,   // The version is no longer running as activated, due to
                 // unregistration or replace.
  };

  // Behavior when a request times out.
  enum TimeoutBehavior {
    KILL_ON_TIMEOUT,     // Kill the worker if this request times out.
    CONTINUE_ON_TIMEOUT  // Keep the worker alive, only abandon the request that
                         // timed out.
  };

  class Listener {
   public:
    virtual void OnRunningStateChanged(ServiceWorkerVersion* version) {}
    virtual void OnVersionStateChanged(ServiceWorkerVersion* version) {}
    virtual void OnMainScriptHttpResponseInfoSet(
        ServiceWorkerVersion* version) {}
    virtual void OnErrorReported(ServiceWorkerVersion* version,
                                 const base::string16& error_message,
                                 int line_number,
                                 int column_number,
                                 const GURL& source_url) {}
    virtual void OnReportConsoleMessage(ServiceWorkerVersion* version,
                                        int source_identifier,
                                        int message_level,
                                        const base::string16& message,
                                        int line_number,
                                        const GURL& source_url) {}
    virtual void OnControlleeAdded(ServiceWorkerVersion* version,
                                   ServiceWorkerProviderHost* provider_host) {}
    virtual void OnControlleeRemoved(ServiceWorkerVersion* version,
                                     ServiceWorkerProviderHost* provider_host) {
    }
    // Fires when a version transitions from having a controllee to not.
    virtual void OnNoControllees(ServiceWorkerVersion* version) {}
    virtual void OnCachedMetadataUpdated(ServiceWorkerVersion* version) {}

   protected:
    virtual ~Listener() {}
  };

  ServiceWorkerVersion(ServiceWorkerRegistration* registration,
                       const GURL& script_url,
                       int64_t version_id,
                       base::WeakPtr<ServiceWorkerContextCore> context);

  int64_t version_id() const { return version_id_; }
  int64_t registration_id() const { return registration_id_; }
  const GURL& script_url() const { return script_url_; }
  const GURL& scope() const { return scope_; }
  RunningStatus running_status() const {
    return static_cast<RunningStatus>(embedded_worker_->status());
  }
  ServiceWorkerVersionInfo GetInfo();
  Status status() const { return status_; }

  const std::vector<GURL>& foreign_fetch_scopes() const {
    return foreign_fetch_scopes_;
  }
  void set_foreign_fetch_scopes(const std::vector<GURL>& scopes) {
    foreign_fetch_scopes_ = scopes;
  }

  // This sets the new status and also run status change callbacks
  // if there're any (see RegisterStatusChangeCallback).
  void SetStatus(Status status);

  // Registers status change callback. (This is for one-off observation,
  // the consumer needs to re-register if it wants to continue observing
  // status changes)
  void RegisterStatusChangeCallback(const base::Closure& callback);

  // Starts an embedded worker for this version.
  // This returns OK (success) if the worker is already running.
  void StartWorker(const StatusCallback& callback);

  // Stops an embedded worker for this version.
  // This returns OK (success) if the worker is already stopped.
  void StopWorker(const StatusCallback& callback);

  // Schedules an update to be run 'soon'.
  void ScheduleUpdate();

  // If an update is scheduled but not yet started, this resets the timer
  // delaying the start time by a 'small' amount.
  void DeferScheduledUpdate();

  // Starts an update now.
  void StartUpdate();

  // Starts the worker if it isn't already running, and calls |task| when the
  // worker is running, or |error_callback| if starting the worker failed.
  // If the worker is already running, |task| is executed synchronously (before
  // this method returns).
  void RunAfterStartWorker(const base::Closure& task,
                           const StatusCallback& error_callback);

  // Call this while the worker is running before dispatching an event to the
  // worker. This informs ServiceWorkerVersion about the event in progress.
  // Returns a request id, which should later be passed to FinishRequest when
  // the event finished.
  // The |error_callback| is called if either ServiceWorkerVersion decides the
  // event is taking too long, or if for some reason the worker stops or is
  // killed before the request finishes.
  int StartRequest(ServiceWorkerMetrics::EventType event_type,
                   const StatusCallback& error_callback);

  // Same as StartRequest, but allows the caller to specify a custom timeout for
  // the event, as well as the behavior for when the request times out.
  int StartRequestWithCustomTimeout(ServiceWorkerMetrics::EventType event_type,
                                    const StatusCallback& error_callback,
                                    const base::TimeDelta& timeout,
                                    TimeoutBehavior timeout_behavior);

  // Informs ServiceWorkerVersion that an event has finished being dispatched.
  // Returns false if no pending requests with the provided id exist, for
  // example if the request has already timed out.
  bool FinishRequest(int request_id);

  // Connects to a specific mojo service exposed by the (running) service
  // worker. If a connection to a service for the same Interface already exists
  // this will return that existing connection. The |request_id| must be a value
  // previously returned by StartRequest. If the connection to the service
  // fails or closes before the request finished, the error callback associated
  // with |request_id| is called.
  // Only call GetMojoServiceForRequest once for a specific |request_id|.
  template <typename Interface>
  base::WeakPtr<Interface> GetMojoServiceForRequest(int request_id);

  // Dispatches an event. If dispatching the event fails, the error callback
  // associated with the |request_id| is called. Any messages sent back in
  // response to this event are passed on to the response |callback|.
  // ResponseMessage is the type of the IPC message that is used for the
  // response, and its first argument MUST be the request_id.
  // This must be called when the worker is running.
  template <typename ResponseMessage, typename ResponseCallbackType>
  void DispatchEvent(int request_id,
                     const IPC::Message& message,
                     const ResponseCallbackType& callback);

  // For simple events where the full functionality of DispatchEvent is not
  // needed, this method can be used instead. The ResponseMessage must consist
  // of just a request_id and a blink::WebServiceWorkerEventResult field. The
  // result is converted to a ServiceWorkerStatusCode and passed to the error
  // handler associated with the request. Additionally this methods calls
  // FinishRequest before passing the reply to the callback.
  template <typename ResponseMessage>
  void DispatchSimpleEvent(int request_id, const IPC::Message& message);

  // Sends a message event to the associated embedded worker.
  void DispatchMessageEvent(
      const base::string16& message,
      const std::vector<TransferredMessagePort>& sent_message_ports,
      const StatusCallback& callback);

  // Sends install event to the associated embedded worker and asynchronously
  // calls |callback| when it errors out or it gets a response from the worker
  // to notify install completion.
  //
  // This must be called when the status() is NEW. Calling this changes
  // the version's status to INSTALLING.
  // Upon completion, the version's status will be changed to INSTALLED
  // on success, or back to NEW on failure.
  void DispatchInstallEvent(const StatusCallback& callback);

  // Sends activate event to the associated embedded worker and asynchronously
  // calls |callback| when it errors out or it gets a response from the worker
  // to notify activation completion.
  //
  // This must be called when the status() is INSTALLED. Calling this changes
  // the version's status to ACTIVATING.
  // Upon completion, the version's status will be changed to ACTIVATED
  // on success, or back to INSTALLED on failure.
  void DispatchActivateEvent(const StatusCallback& callback);

  // Sends fetch event to the associated embedded worker and calls
  // |callback| with the response from the worker.
  //
  // This must be called when the status() is ACTIVATED. Calling this in other
  // statuses will result in an error SERVICE_WORKER_ERROR_FAILED.
  void DispatchFetchEvent(const ServiceWorkerFetchRequest& request,
                          const base::Closure& prepare_callback,
                          const FetchCallback& fetch_callback);

  // Sends a cross origin message event to the associated embedded worker and
  // asynchronously calls |callback| when the message was sent (or failed to
  // sent).
  // It is the responsibility of the code calling this method to make sure that
  // any transferred message ports are put on hold while potentially a process
  // for the service worker is spun up.
  //
  // This must be called when the status() is ACTIVATED.
  void DispatchCrossOriginMessageEvent(
      const NavigatorConnectClient& client,
      const base::string16& message,
      const std::vector<TransferredMessagePort>& sent_message_ports,
      const StatusCallback& callback);

  // Adds and removes |provider_host| as a controllee of this ServiceWorker.
  // A potential controllee is a host having the version as its .installing
  // or .waiting version.
  void AddControllee(ServiceWorkerProviderHost* provider_host);
  void RemoveControllee(ServiceWorkerProviderHost* provider_host);

  // Returns if it has controllee.
  bool HasControllee() const { return !controllee_map_.empty(); }
  std::map<std::string, ServiceWorkerProviderHost*> controllee_map() {
    return controllee_map_;
  }

  base::WeakPtr<ServiceWorkerContextCore> context() const { return context_; }

  // Adds and removes |request_job| as a dependent job not to stop the
  // ServiceWorker while |request_job| is reading the stream of the fetch event
  // response from the ServiceWorker.
  void AddStreamingURLRequestJob(const ServiceWorkerURLRequestJob* request_job);
  void RemoveStreamingURLRequestJob(
      const ServiceWorkerURLRequestJob* request_job);

  // Adds and removes Listeners.
  void AddListener(Listener* listener);
  void RemoveListener(Listener* listener);

  ServiceWorkerScriptCacheMap* script_cache_map() { return &script_cache_map_; }
  EmbeddedWorkerInstance* embedded_worker() { return embedded_worker_.get(); }

  // Reports the error message to |listeners_|.
  void ReportError(ServiceWorkerStatusCode status,
                   const std::string& status_message);

  // Sets the status code to pass to StartWorker callbacks if start fails.
  void SetStartWorkerStatusCode(ServiceWorkerStatusCode status);

  // Sets this version's status to REDUNDANT and deletes its resources.
  // The version must not have controllees.
  void Doom();
  bool is_redundant() const { return status_ == REDUNDANT; }

  bool skip_waiting() const { return skip_waiting_; }
  void set_skip_waiting(bool skip_waiting) { skip_waiting_ = skip_waiting; }

  bool force_bypass_cache_for_scripts() const {
    return force_bypass_cache_for_scripts_;
  }
  void set_force_bypass_cache_for_scripts(bool force_bypass_cache_for_scripts) {
    force_bypass_cache_for_scripts_ = force_bypass_cache_for_scripts;
  }

  bool skip_script_comparison() const { return skip_script_comparison_; }
  void set_skip_script_comparison(bool skip_script_comparison) {
    skip_script_comparison_ = skip_script_comparison;
  }

  void SetDevToolsAttached(bool attached);

  // Sets the HttpResponseInfo used to load the main script.
  // This HttpResponseInfo will be used for all responses sent back from the
  // service worker, as the effective security of these responses is equivalent
  // to that of the ServiceWorker.
  void SetMainScriptHttpResponseInfo(const net::HttpResponseInfo& http_info);
  const net::HttpResponseInfo* GetMainScriptHttpResponseInfo();

  // Simulate ping timeout. Should be used for tests-only.
  void SimulatePingTimeoutForTesting();

  bool IsDisabled() const;

 private:
  friend class base::RefCounted<ServiceWorkerVersion>;
  friend class ServiceWorkerMetrics;
  friend class ServiceWorkerReadFromCacheJobTest;
  friend class ServiceWorkerStallInStoppingTest;
  friend class ServiceWorkerURLRequestJobTest;
  friend class ServiceWorkerVersionBrowserTest;
  friend class ServiceWorkerVersionTest;

  FRIEND_TEST_ALL_PREFIXES(ServiceWorkerControlleeRequestHandlerTest,
                           ActivateWaitingVersion);
  FRIEND_TEST_ALL_PREFIXES(ServiceWorkerVersionTest, IdleTimeout);
  FRIEND_TEST_ALL_PREFIXES(ServiceWorkerVersionTest, SetDevToolsAttached);
  FRIEND_TEST_ALL_PREFIXES(ServiceWorkerVersionTest, StaleUpdate_FreshWorker);
  FRIEND_TEST_ALL_PREFIXES(ServiceWorkerVersionTest,
                           StaleUpdate_NonActiveWorker);
  FRIEND_TEST_ALL_PREFIXES(ServiceWorkerVersionTest, StaleUpdate_StartWorker);
  FRIEND_TEST_ALL_PREFIXES(ServiceWorkerVersionTest, StaleUpdate_RunningWorker);
  FRIEND_TEST_ALL_PREFIXES(ServiceWorkerVersionTest,
                           StaleUpdate_DoNotDeferTimer);
  FRIEND_TEST_ALL_PREFIXES(ServiceWorkerWaitForeverInFetchTest, RequestTimeout);
  FRIEND_TEST_ALL_PREFIXES(ServiceWorkerVersionTest, RequestTimeout);
  FRIEND_TEST_ALL_PREFIXES(ServiceWorkerFailToStartTest, Timeout);
  FRIEND_TEST_ALL_PREFIXES(ServiceWorkerVersionBrowserTest,
                           TimeoutStartingWorker);
  FRIEND_TEST_ALL_PREFIXES(ServiceWorkerVersionBrowserTest,
                           TimeoutWorkerInEvent);
  FRIEND_TEST_ALL_PREFIXES(ServiceWorkerStallInStoppingTest, DetachThenStart);
  FRIEND_TEST_ALL_PREFIXES(ServiceWorkerStallInStoppingTest, DetachThenRestart);
  FRIEND_TEST_ALL_PREFIXES(ServiceWorkerVersionTest,
                           RegisterForeignFetchScopes);
  FRIEND_TEST_ALL_PREFIXES(ServiceWorkerVersionTest, RequestCustomizedTimeout);
  FRIEND_TEST_ALL_PREFIXES(ServiceWorkerVersionTest,
                           RequestCustomizedTimeoutKill);
  FRIEND_TEST_ALL_PREFIXES(ServiceWorkerWaitForeverInFetchTest,
                           MixedRequestTimeouts);

  class Metrics;
  class PingController;

  enum RequestType {
    REQUEST_ACTIVATE,
    REQUEST_INSTALL,
    REQUEST_FETCH,
    REQUEST_CUSTOM,
    NUM_REQUEST_TYPES
  };

  struct RequestInfo {
    RequestInfo(int id,
                RequestType type,
                ServiceWorkerMetrics::EventType event_type,
                const base::TimeTicks& expiration,
                TimeoutBehavior timeout_behavior);
    ~RequestInfo();
    bool operator>(const RequestInfo& other) const;
    int id;
    RequestType type;
    ServiceWorkerMetrics::EventType event_type;
    base::TimeTicks expiration;
    TimeoutBehavior timeout_behavior;
  };

  template <typename CallbackType>
  struct PendingRequest {
    PendingRequest(const CallbackType& callback,
                   const base::TimeTicks& time,
                   ServiceWorkerMetrics::EventType event_type);
    ~PendingRequest() {}

    CallbackType callback;
    base::TimeTicks start_time;
    ServiceWorkerMetrics::EventType event_type;
    // Name of the mojo service this request is associated with. Used to call
    // the callback when a connection closes with outstanding requests.
    // Compared as pointer, so should only contain static strings. Typically
    // this would be Interface::Name_ for some mojo interface.
    const char* mojo_service = nullptr;
    scoped_ptr<EmbeddedWorkerInstance::Listener> listener;
  };

  // Base class to enable storing a list of mojo interface pointers for
  // arbitrary interfaces. The destructor is also responsible for calling the
  // error callbacks for any outstanding requests using this service.
  class CONTENT_EXPORT BaseMojoServiceWrapper {
   public:
    BaseMojoServiceWrapper(ServiceWorkerVersion* worker,
                           const char* service_name);
    virtual ~BaseMojoServiceWrapper();

   private:
    ServiceWorkerVersion* worker_;
    const char* service_name_;

    DISALLOW_COPY_AND_ASSIGN(BaseMojoServiceWrapper);
  };

  // Wrapper around a mojo::InterfacePtr, which passes out WeakPtr's to the
  // interface.
  template <typename Interface>
  class MojoServiceWrapper : public BaseMojoServiceWrapper {
   public:
    MojoServiceWrapper(ServiceWorkerVersion* worker,
                       mojo::InterfacePtr<Interface> interface)
        : BaseMojoServiceWrapper(worker, Interface::Name_),
          interface_(std::move(interface)),
          weak_ptr_factory_(interface_.get()) {}

    base::WeakPtr<Interface> GetWeakPtr() {
      return weak_ptr_factory_.GetWeakPtr();
    }

   private:
    mojo::InterfacePtr<Interface> interface_;
    base::WeakPtrFactory<Interface> weak_ptr_factory_;
  };

  typedef ServiceWorkerVersion self;
  using ServiceWorkerClients = std::vector<ServiceWorkerClientInfo>;
  using RequestInfoPriorityQueue =
      std::priority_queue<RequestInfo,
                          std::vector<RequestInfo>,
                          std::greater<RequestInfo>>;

  // EmbeddedWorkerInstance Listener implementation which calls a callback
  // on receiving a particular IPC message. ResponseMessage is the type of
  // the IPC message to listen for, while CallbackType should be a callback
  // with same arguments as the IPC message.
  // Additionally only calls the callback for messages with a specific request
  // id, which must be the first argument of the IPC message.
  template <typename ResponseMessage, typename CallbackType>
  class EventResponseHandler : public EmbeddedWorkerInstance::Listener {
   public:
    EventResponseHandler(EmbeddedWorkerInstance* worker,
                         int request_id,
                         const CallbackType& callback)
        : worker_(worker), request_id_(request_id), callback_(callback) {
      worker_->AddListener(this);
    }
    ~EventResponseHandler() override { worker_->RemoveListener(this); }
    bool OnMessageReceived(const IPC::Message& message) override;

   private:
    EmbeddedWorkerInstance* const worker_;
    const int request_id_;
    const CallbackType callback_;
  };

  // The timeout timer interval.
  static const int kTimeoutTimerDelaySeconds;
  // Timeout for an installed worker to start.
  static const int kStartInstalledWorkerTimeoutSeconds;
  // Timeout for a new worker to start.
  static const int kStartNewWorkerTimeoutMinutes;
  // Timeout for a request to be handled.
  static const int kRequestTimeoutMinutes;
  // Timeout for the worker to stop.
  static const int kStopWorkerTimeoutSeconds;

  ~ServiceWorkerVersion() override;

  // EmbeddedWorkerInstance::Listener overrides:
  void OnThreadStarted() override;
  void OnStarting() override;
  void OnStarted() override;
  void OnStopping() override;
  void OnStopped(EmbeddedWorkerInstance::Status old_status) override;
  void OnDetached(EmbeddedWorkerInstance::Status old_status) override;
  void OnScriptLoaded() override;
  void OnScriptLoadFailed() override;
  void OnReportException(const base::string16& error_message,
                         int line_number,
                         int column_number,
                         const GURL& source_url) override;
  void OnReportConsoleMessage(int source_identifier,
                              int message_level,
                              const base::string16& message,
                              int line_number,
                              const GURL& source_url) override;
  bool OnMessageReceived(const IPC::Message& message) override;

  void OnStartSentAndScriptEvaluated(ServiceWorkerStatusCode status);

  void DispatchInstallEventAfterStartWorker(const StatusCallback& callback);
  void DispatchActivateEventAfterStartWorker(const StatusCallback& callback);

  void DispatchMessageEventInternal(
      const base::string16& message,
      const std::vector<TransferredMessagePort>& sent_message_ports,
      const StatusCallback& callback);

  // Message handlers.

  // This corresponds to the spec's matchAll(options) steps.
  void OnGetClients(int request_id,
                    const ServiceWorkerClientQueryOptions& options);

  void OnActivateEventFinished(int request_id,
                               blink::WebServiceWorkerEventResult result);
  void OnInstallEventFinished(int request_id,
                              blink::WebServiceWorkerEventResult result);
  void OnFetchEventFinished(int request_id,
                            ServiceWorkerFetchEventResult result,
                            const ServiceWorkerResponse& response);
  void OnSimpleEventResponse(int request_id,
                             blink::WebServiceWorkerEventResult result);
  void OnOpenWindow(int request_id, GURL url);
  void OnOpenWindowFinished(int request_id,
                            ServiceWorkerStatusCode status,
                            const std::string& client_uuid,
                            const ServiceWorkerClientInfo& client_info);

  void OnSetCachedMetadata(const GURL& url, const std::vector<char>& data);
  void OnSetCachedMetadataFinished(int64_t callback_id, int result);
  void OnClearCachedMetadata(const GURL& url);
  void OnClearCachedMetadataFinished(int64_t callback_id, int result);

  void OnPostMessageToClient(
      const std::string& client_uuid,
      const base::string16& message,
      const std::vector<TransferredMessagePort>& sent_message_ports);
  void OnFocusClient(int request_id, const std::string& client_uuid);
  void OnNavigateClient(int request_id,
                        const std::string& client_uuid,
                        const GURL& url);
  void OnNavigateClientFinished(int request_id,
                                ServiceWorkerStatusCode status,
                                const std::string& client_uuid,
                                const ServiceWorkerClientInfo& client);
  void OnSkipWaiting(int request_id);
  void OnClaimClients(int request_id);
  void OnPongFromWorker();

  void OnFocusClientFinished(int request_id,
                             const std::string& client_uuid,
                             const ServiceWorkerClientInfo& client);

  void OnRegisterForeignFetchScopes(const std::vector<GURL>& sub_scopes);

  void DidEnsureLiveRegistrationForStartWorker(
      const StatusCallback& callback,
      ServiceWorkerStatusCode status,
      const scoped_refptr<ServiceWorkerRegistration>& registration);
  void StartWorkerInternal();

  void DidSkipWaiting(int request_id);

  void OnGetClientsFinished(int request_id, ServiceWorkerClients* clients);

  // The timeout timer periodically calls OnTimeoutTimer, which stops the worker
  // if it is excessively idle or unresponsive to ping.
  void StartTimeoutTimer();
  void StopTimeoutTimer();
  void OnTimeoutTimer();
  void SetTimeoutTimerInterval(base::TimeDelta interval);

  // Called by PingController for ping protocol.
  ServiceWorkerStatusCode PingWorker();
  void OnPingTimeout();

  // Stops the worker if it is idle (has no in-flight requests) or timed out
  // ping.
  void StopWorkerIfIdle();
  bool HasInflightRequests() const;

  // RecordStartWorkerResult is added as a start callback by StartTimeoutTimer
  // and records metrics about startup.
  void RecordStartWorkerResult(ServiceWorkerStatusCode status);

  template <typename IDMAP>
  void RemoveCallbackAndStopIfRedundant(IDMAP* callbacks, int request_id);

  template <typename CallbackType>
  int AddRequest(
      const CallbackType& callback,
      IDMap<PendingRequest<CallbackType>, IDMapOwnPointer>* callback_map,
      RequestType request_type,
      ServiceWorkerMetrics::EventType event_type);

  template <typename CallbackType>
  int AddRequestWithExpiration(
      const CallbackType& callback,
      IDMap<PendingRequest<CallbackType>, IDMapOwnPointer>* callback_map,
      RequestType request_type,
      ServiceWorkerMetrics::EventType event_type,
      base::TimeTicks expiration,
      TimeoutBehavior timeout_behavior);

  bool MaybeTimeOutRequest(const RequestInfo& info);
  void SetAllRequestExpirations(const base::TimeTicks& expiration);

  // Returns the reason the embedded worker failed to start, using information
  // inaccessible to EmbeddedWorkerInstance. Returns |default_code| if it can't
  // deduce a reason.
  ServiceWorkerStatusCode DeduceStartWorkerFailureReason(
      ServiceWorkerStatusCode default_code);

  // Sets |stale_time_| if this worker is stale, causing an update to eventually
  // occur once the worker stops or is running too long.
  void MarkIfStale();

  void FoundRegistrationForUpdate(
      ServiceWorkerStatusCode status,
      const scoped_refptr<ServiceWorkerRegistration>& registration);

  void OnStoppedInternal(EmbeddedWorkerInstance::Status old_status);

  // Called when the remote side of a connection to a mojo service is lost.
  void OnMojoConnectionError(const char* service_name);

  // Called at the beginning of each Dispatch*Event function: records
  // the time elapsed since idle (generally the time since the previous
  // event ended).
  void OnBeginEvent();

  const int64_t version_id_;
  const int64_t registration_id_;
  const GURL script_url_;
  const GURL scope_;
  std::vector<GURL> foreign_fetch_scopes_;

  Status status_ = NEW;
  scoped_ptr<EmbeddedWorkerInstance> embedded_worker_;
  std::vector<StatusCallback> start_callbacks_;
  std::vector<StatusCallback> stop_callbacks_;
  std::vector<base::Closure> status_change_callbacks_;

  // Message callbacks. (Update HasInflightRequests() too when you update this
  // list.)
  IDMap<PendingRequest<StatusCallback>, IDMapOwnPointer> activate_requests_;
  IDMap<PendingRequest<StatusCallback>, IDMapOwnPointer> install_requests_;
  IDMap<PendingRequest<FetchCallback>, IDMapOwnPointer> fetch_requests_;
  IDMap<PendingRequest<StatusCallback>, IDMapOwnPointer> custom_requests_;

  // Stores all open connections to mojo services. Maps the service name to
  // the actual interface pointer. When a connection is closed it is removed
  // from this map.
  // mojo_services_[Interface::Name_] is assumed to always contain a
  // MojoServiceWrapper<Interface> instance.
  base::ScopedPtrHashMap<const char*, scoped_ptr<BaseMojoServiceWrapper>>
      mojo_services_;

  std::set<const ServiceWorkerURLRequestJob*> streaming_url_request_jobs_;

  std::map<std::string, ServiceWorkerProviderHost*> controllee_map_;
  // Will be null while shutting down.
  base::WeakPtr<ServiceWorkerContextCore> context_;
  base::ObserverList<Listener> listeners_;
  ServiceWorkerScriptCacheMap script_cache_map_;
  base::OneShotTimer update_timer_;

  // Starts running in StartWorker and continues until the worker is stopped.
  base::RepeatingTimer timeout_timer_;
  // Holds the time the worker last started being considered idle.
  base::TimeTicks idle_time_;
  // Holds the time that the outstanding StartWorker() request started.
  base::TimeTicks start_time_;
  // Holds the time the worker entered STOPPING status.
  base::TimeTicks stop_time_;
  // Holds the time the worker was detected as stale and needs updating. We try
  // to update once the worker stops, but will also update if it stays alive too
  // long.
  base::TimeTicks stale_time_;

  // New requests are added to |requests_| along with their entry in a callback
  // map. Requests are sorted by their expiration time (soonest to expire on top
  // of the priority queue). The timeout timer periodically checks |requests_|
  // for entries that should time out or have already been fulfilled (i.e.,
  // removed from the callback map).
  RequestInfoPriorityQueue requests_;

  bool skip_waiting_ = false;
  bool skip_recording_startup_time_ = false;
  bool force_bypass_cache_for_scripts_ = false;
  bool skip_script_comparison_ = false;
  bool is_update_scheduled_ = false;
  bool in_dtor_ = false;

  std::vector<int> pending_skip_waiting_requests_;
  scoped_ptr<net::HttpResponseInfo> main_script_http_info_;

  // The status when StartWorker was invoked. Used for UMA.
  Status prestart_status_ = NEW;
  // If not OK, the reason that StartWorker failed. Used for
  // running |start_callbacks_|.
  ServiceWorkerStatusCode start_worker_status_ = SERVICE_WORKER_OK;

  scoped_ptr<PingController> ping_controller_;
  scoped_ptr<Metrics> metrics_;
  const bool should_exclude_from_uma_ = false;

  base::WeakPtrFactory<ServiceWorkerVersion> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerVersion);
};

template <typename Interface>
base::WeakPtr<Interface> ServiceWorkerVersion::GetMojoServiceForRequest(
    int request_id) {
  DCHECK_EQ(RUNNING, running_status());
  PendingRequest<StatusCallback>* request = custom_requests_.Lookup(request_id);
  DCHECK(request) << "Invalid request id";
  DCHECK(!request->mojo_service)
      << "Request is already associated with a mojo service";

  MojoServiceWrapper<Interface>* service =
      static_cast<MojoServiceWrapper<Interface>*>(
          mojo_services_.get(Interface::Name_));
  if (!service) {
    mojo::InterfacePtr<Interface> interface;
    embedded_worker_->GetServiceRegistry()->ConnectToRemoteService(
        mojo::GetProxy(&interface));
    interface.set_connection_error_handler(
        base::Bind(&ServiceWorkerVersion::OnMojoConnectionError,
                   weak_factory_.GetWeakPtr(), Interface::Name_));
    service = new MojoServiceWrapper<Interface>(this, std::move(interface));
    mojo_services_.add(Interface::Name_, make_scoped_ptr(service));
  }
  request->mojo_service = Interface::Name_;
  return service->GetWeakPtr();
}

template <typename ResponseMessage, typename ResponseCallbackType>
void ServiceWorkerVersion::DispatchEvent(int request_id,
                                         const IPC::Message& message,
                                         const ResponseCallbackType& callback) {
  DCHECK_EQ(RUNNING, running_status());
  PendingRequest<StatusCallback>* request = custom_requests_.Lookup(request_id);
  DCHECK(request) << "Invalid request id";
  DCHECK(!request->listener) << "Request already dispatched an IPC event";

  ServiceWorkerStatusCode status = embedded_worker_->SendMessage(message);
  if (status != SERVICE_WORKER_OK) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(request->callback, status));
    custom_requests_.Remove(request_id);
  } else {
    request->listener.reset(
        new EventResponseHandler<ResponseMessage, ResponseCallbackType>(
            embedded_worker(), request_id, callback));
  }
}

template <typename ResponseMessage>
void ServiceWorkerVersion::DispatchSimpleEvent(int request_id,
                                               const IPC::Message& message) {
  DispatchEvent<ResponseMessage>(
      request_id, message,
      base::Bind(&ServiceWorkerVersion::OnSimpleEventResponse, this));
}

template <typename ResponseMessage, typename CallbackType>
bool ServiceWorkerVersion::EventResponseHandler<ResponseMessage, CallbackType>::
    OnMessageReceived(const IPC::Message& message) {
  if (message.type() != ResponseMessage::ID)
    return false;
  int received_request_id;
  bool result = base::PickleIterator(message).ReadInt(&received_request_id);
  if (!result || received_request_id != request_id_)
    return false;

  CallbackType protect(callback_);
  // Essentially same code as what IPC_MESSAGE_FORWARD expands to.
  void* param = nullptr;
  if (!ResponseMessage::Dispatch(&message, &callback_, this, param,
                                 &CallbackType::Run))
    message.set_dispatch_error();

  // At this point |this| can have been deleted, so don't do anything other
  // than returning.

  return true;
}

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_VERSION_H_
