// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_VERSION_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_VERSION_H_

#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/gtest_prod_util.h"
#include "base/id_map.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/observer_list.h"
#include "base/timer/timer.h"
#include "content/browser/service_worker/embedded_worker_instance.h"
#include "content/browser/service_worker/service_worker_cache_listener.h"
#include "content/browser/service_worker/service_worker_script_cache_map.h"
#include "content/common/content_export.h"
#include "content/common/service_worker/service_worker_status_code.h"
#include "content/common/service_worker/service_worker_types.h"
#include "third_party/WebKit/public/platform/WebGeofencingEventType.h"
#include "third_party/WebKit/public/platform/WebServiceWorkerEventResult.h"

// Windows headers will redefine SendMessage.
#ifdef SendMessage
#undef SendMessage
#endif

class GURL;

namespace blink {
struct WebCircularGeofencingRegion;
}

namespace net {
class HttpResponseInfo;
}

namespace content {

class EmbeddedWorkerRegistry;
class ServiceWorkerContextCore;
class ServiceWorkerProviderHost;
class ServiceWorkerRegistration;
class ServiceWorkerURLRequestJob;
class ServiceWorkerVersionInfo;
struct NavigatorConnectClient;
struct PlatformNotificationData;
struct ServiceWorkerClientInfo;
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
  typedef base::Callback<void(ServiceWorkerStatusCode, bool)>
      CrossOriginConnectCallback;

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

  class Listener {
   public:
    virtual void OnWorkerStarted(ServiceWorkerVersion* version) {}
    virtual void OnWorkerStopped(ServiceWorkerVersion* version) {}
    virtual void OnVersionStateChanged(ServiceWorkerVersion* version) {}
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
    // Fires when a version transitions from having a controllee to not.
    virtual void OnNoControllees(ServiceWorkerVersion* version) {}
    virtual void OnCachedMetadataUpdated(ServiceWorkerVersion* version) {}

   protected:
    virtual ~Listener() {}
  };

  ServiceWorkerVersion(
      ServiceWorkerRegistration* registration,
      const GURL& script_url,
      int64 version_id,
      base::WeakPtr<ServiceWorkerContextCore> context);

  int64 version_id() const { return version_id_; }
  int64 registration_id() const { return registration_id_; }
  const GURL& script_url() const { return script_url_; }
  const GURL& scope() const { return scope_; }
  RunningStatus running_status() const {
    return static_cast<RunningStatus>(embedded_worker_->status());
  }
  ServiceWorkerVersionInfo GetInfo();
  Status status() const { return status_; }

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

  // Starts an embedded worker for this version.
  // |pause_after_download| notifies worker to pause after download finished
  // which could be resumed by EmbeddedWorkerInstance::ResumeAfterDownload.
  // This returns OK (success) if the worker is already running.
  void StartWorker(bool pause_after_download,
                   const StatusCallback& callback);

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

  // Sends sync event to the associated embedded worker and asynchronously calls
  // |callback| when it errors out or it gets a response from the worker to
  // notify completion.
  //
  // This must be called when the status() is ACTIVATED.
  void DispatchSyncEvent(const StatusCallback& callback);

  // Sends notificationclick event to the associated embedded worker and
  // asynchronously calls |callback| when it errors out or it gets a response
  // from the worker to notify completion.
  //
  // This must be called when the status() is ACTIVATED.
  void DispatchNotificationClickEvent(
      const StatusCallback& callback,
      const std::string& notification_id,
      const PlatformNotificationData& notification_data);

  // Sends push event to the associated embedded worker and asynchronously calls
  // |callback| when it errors out or it gets a response from the worker to
  // notify completion.
  //
  // This must be called when the status() is ACTIVATED.
  void DispatchPushEvent(const StatusCallback& callback,
                         const std::string& data);

  // Sends geofencing event to the associated embedded worker and asynchronously
  // calls |callback| when it errors out or it gets a response from the worker
  // to notify completion.
  //
  // This must be called when the status() is ACTIVATED.
  void DispatchGeofencingEvent(
      const StatusCallback& callback,
      blink::WebGeofencingEventType event_type,
      const std::string& region_id,
      const blink::WebCircularGeofencingRegion& region);

  // Sends a cross origin connect event to the associated embedded worker and
  // asynchronously calls |callback| with the response from the worker.
  //
  // This must be called when the status() is ACTIVATED.
  void DispatchCrossOriginConnectEvent(
      const CrossOriginConnectCallback& callback,
      const NavigatorConnectClient& client);

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

  // Dooms this version to have REDUNDANT status and its resources deleted.  If
  // the version is controlling a page, these changes will happen when the
  // version no longer controls any pages.
  void Doom();
  bool is_doomed() const { return is_doomed_; }

  bool skip_waiting() const { return skip_waiting_; }

  void SetDevToolsAttached(bool attached);

  // Sets the HttpResponseInfo used to load the main script.
  // This HttpResponseInfo will be used for all responses sent back from the
  // service worker, as the effective security of these responses is equivalent
  // to that of the ServiceWorker.
  void SetMainScriptHttpResponseInfo(const net::HttpResponseInfo& http_info);
  const net::HttpResponseInfo* GetMainScriptHttpResponseInfo();

 private:
  class GetClientDocumentsCallback;

  friend class base::RefCounted<ServiceWorkerVersion>;
  friend class ServiceWorkerURLRequestJobTest;
  FRIEND_TEST_ALL_PREFIXES(ServiceWorkerControlleeRequestHandlerTest,
                           ActivateWaitingVersion);
  FRIEND_TEST_ALL_PREFIXES(ServiceWorkerVersionTest, ScheduleStopWorker);
  FRIEND_TEST_ALL_PREFIXES(ServiceWorkerVersionTest, KeepAlive);
  FRIEND_TEST_ALL_PREFIXES(ServiceWorkerVersionTest, ListenerAvailability);
  FRIEND_TEST_ALL_PREFIXES(ServiceWorkerFailToStartTest, Timeout);
  FRIEND_TEST_ALL_PREFIXES(ServiceWorkerVersionBrowserTest,
                           TimeoutStartingWorker);
  FRIEND_TEST_ALL_PREFIXES(ServiceWorkerVersionBrowserTest,
                           TimeoutWorkerInEvent);
  friend class ServiceWorkerVersionBrowserTest;

  typedef ServiceWorkerVersion self;
  typedef std::map<ServiceWorkerProviderHost*, int> ControlleeMap;
  typedef IDMap<ServiceWorkerProviderHost> ControlleeByIDMap;

  ~ServiceWorkerVersion() override;

  // EmbeddedWorkerInstance::Listener overrides:
  void OnScriptLoaded() override;
  void OnStarted() override;
  void OnStopped(EmbeddedWorkerInstance::Status old_status) override;
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
  void OnGetClientDocuments(int request_id);
  void OnActivateEventFinished(int request_id,
                               blink::WebServiceWorkerEventResult result);
  void OnInstallEventFinished(int request_id,
                              blink::WebServiceWorkerEventResult result);
  void OnFetchEventFinished(int request_id,
                            ServiceWorkerFetchEventResult result,
                            const ServiceWorkerResponse& response);
  void OnSyncEventFinished(int request_id);
  void OnNotificationClickEventFinished(int request_id);
  void OnPushEventFinished(int request_id,
                           blink::WebServiceWorkerEventResult result);
  void OnGeofencingEventFinished(int request_id);
  void OnCrossOriginConnectEventFinished(int request_id,
                                         bool accept_connection);
  void OnOpenWindow(int request_id, const GURL& url);
  void DidOpenWindow(int request_id,
                     int render_process_id,
                     int render_frame_id);
  void OnOpenWindowFinished(int request_id,
                            int client_id,
                            const ServiceWorkerClientInfo& client_info);

  void OnSetCachedMetadata(const GURL& url, const std::vector<char>& data);
  void OnSetCachedMetadataFinished(int64 callback_id, int result);
  void OnClearCachedMetadata(const GURL& url);
  void OnClearCachedMetadataFinished(int64 callback_id, int result);

  void OnPostMessageToDocument(
      int client_id,
      const base::string16& message,
      const std::vector<TransferredMessagePort>& sent_message_ports);
  void OnFocusClient(int request_id, int client_id);
  void OnSkipWaiting(int request_id);
  void OnClaimClients(int request_id);
  void OnPongFromWorker();

  void OnFocusClientFinished(int request_id,
                             int client_id,
                             const ServiceWorkerClientInfo& client);

  void DidSkipWaiting(int request_id);
  void DidClaimClients(int request_id, ServiceWorkerStatusCode status);
  void DidGetClientInfo(int client_id,
                        scoped_refptr<GetClientDocumentsCallback> callback,
                        const ServiceWorkerClientInfo& info);

  // The timeout timer periodically calls OnTimeoutTimer, which stops the worker
  // if it is excessively idle or unresponsive to ping.
  void StartTimeoutTimer();
  void StopTimeoutTimer();
  void OnTimeoutTimer();

  // The ping protocol is for terminating workers that are taking excessively
  // long executing JavaScript (e.g., stuck in while(true) {}). Periodically a
  // ping IPC is sent to the worker context and if we timeout waiting for a
  // pong, the worker is terminated.
  void PingWorker();
  void OnPingTimeout();

  // Stops the worker if it is idle (has no in-flight requests) or timed out
  // ping.
  void StopWorkerIfIdle();
  bool HasInflightRequests() const;

  // ScheduleStartWorkerTimeout is called when attempting to the start the
  // embedded worker. It sets a timer for calling OnStartWorkerTimeout, which
  // invokes start callbacks with ERROR_TIMEOUT. It also adds its own start
  // callback RecordStartWorkerResult which cancels the timer and records
  // metrics about startup.
  void ScheduleStartWorkerTimeout();
  void RecordStartWorkerResult(ServiceWorkerStatusCode status);
  void OnStartWorkerTimeout();

  void DoomInternal();

  template <typename IDMAP>
  void RemoveCallbackAndStopIfDoomed(IDMAP* callbacks, int request_id);

  const int64 version_id_;
  int64 registration_id_;
  GURL script_url_;
  GURL scope_;
  Status status_;
  scoped_ptr<EmbeddedWorkerInstance> embedded_worker_;
  scoped_ptr<ServiceWorkerCacheListener> cache_listener_;
  std::vector<StatusCallback> start_callbacks_;
  std::vector<StatusCallback> stop_callbacks_;
  std::vector<base::Closure> status_change_callbacks_;

  // Message callbacks. (Update HasInflightRequests() too when you update this
  // list.)
  IDMap<StatusCallback, IDMapOwnPointer> activate_callbacks_;
  IDMap<StatusCallback, IDMapOwnPointer> install_callbacks_;
  IDMap<FetchCallback, IDMapOwnPointer> fetch_callbacks_;
  IDMap<StatusCallback, IDMapOwnPointer> sync_callbacks_;
  IDMap<StatusCallback, IDMapOwnPointer> notification_click_callbacks_;
  IDMap<StatusCallback, IDMapOwnPointer> push_callbacks_;
  IDMap<StatusCallback, IDMapOwnPointer> geofencing_callbacks_;
  IDMap<CrossOriginConnectCallback, IDMapOwnPointer>
      cross_origin_connect_callbacks_;

  std::set<const ServiceWorkerURLRequestJob*> streaming_url_request_jobs_;

  ControlleeMap controllee_map_;
  ControlleeByIDMap controllee_by_id_;
  // Will be null while shutting down.
  base::WeakPtr<ServiceWorkerContextCore> context_;
  ObserverList<Listener> listeners_;
  ServiceWorkerScriptCacheMap script_cache_map_;
  base::OneShotTimer<ServiceWorkerVersion> update_timer_;

  // Starts running when the script is loaded (which means it is about to begin
  // evaluation) and continues until the worker is stopped or a stop request is
  // ignored because DevTools is attached.
  base::RepeatingTimer<ServiceWorkerVersion> timeout_timer_;
  // Holds the time the worker last started being considered idle.
  base::TimeTicks idle_time_;
  // Holds the time that an outstanding ping was sent to the worker.
  base::TimeTicks ping_time_;
  // True if the worker was stopped because it did not respond to ping in time.
  bool ping_timed_out_;

  base::OneShotTimer<ServiceWorkerVersion> start_worker_timeout_timer_;
  base::TimeTicks start_timing_;

  bool is_doomed_;
  std::vector<int> pending_skip_waiting_requests_;
  bool skip_waiting_;
  scoped_ptr<net::HttpResponseInfo> main_script_http_info_;

  base::WeakPtrFactory<ServiceWorkerVersion> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerVersion);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_VERSION_H_
