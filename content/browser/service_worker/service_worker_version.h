// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_VERSION_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_VERSION_H_

#include <vector>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/gtest_prod_util.h"
#include "base/id_map.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/observer_list.h"
#include "content/browser/service_worker/embedded_worker_instance.h"
#include "content/common/content_export.h"
#include "content/common/service_worker/service_worker_status_code.h"
#include "content/common/service_worker/service_worker_types.h"

class GURL;

namespace content {

class EmbeddedWorkerRegistry;
class ServiceWorkerContextCore;
class ServiceWorkerProviderHost;
class ServiceWorkerRegistration;
class ServiceWorkerVersionInfo;

// This class corresponds to a specific version of a ServiceWorker
// script for a given pattern. When a script is upgraded, there may be
// more than one ServiceWorkerVersion "running" at a time, but only
// one of them is active. This class connects the actual script with a
// running worker.
//
// is_shutdown_ detects the live-ness of the object itself. If the object is
// shut down, then it is in the process of being deleted from memory.
// This happens when a version is replaced as well as at browser shutdown.
class CONTENT_EXPORT ServiceWorkerVersion
    : NON_EXPORTED_BASE(public base::RefCounted<ServiceWorkerVersion>),
      public EmbeddedWorkerInstance::Observer {
 public:
  typedef base::Callback<void(ServiceWorkerStatusCode)> StatusCallback;
  typedef base::Callback<void(ServiceWorkerStatusCode,
                              const IPC::Message& message)> MessageCallback;
  typedef base::Callback<void(ServiceWorkerStatusCode,
                              ServiceWorkerFetchEventResult,
                              const ServiceWorkerResponse&)> FetchCallback;

  enum RunningStatus {
    STOPPED = EmbeddedWorkerInstance::STOPPED,
    STARTING = EmbeddedWorkerInstance::STARTING,
    RUNNING = EmbeddedWorkerInstance::RUNNING,
    STOPPING = EmbeddedWorkerInstance::STOPPING,
  };

  // Current version status; some of the status (e.g. INSTALLED and ACTIVE)
  // should be persisted unlike running status.
  enum Status {
    NEW,         // The version is just created.
    INSTALLING,  // Install event is dispatched and being handled.
    INSTALLED,   // Install event is finished and is ready to be activated.
    ACTIVATING,  // Activate event is dispatched and being handled.
    ACTIVE,      // Activation is finished and can run as active.
    DEACTIVATED, // The version is no longer running as active, due to
                 // unregistration or replace. (TODO(kinuko): we may need
                 // different states for different termination sequences)
  };

  class Listener {
   public:
    virtual void OnWorkerStarted(ServiceWorkerVersion* version) = 0;
    virtual void OnWorkerStopped(ServiceWorkerVersion* version) = 0;
    virtual void OnVersionStateChanged(ServiceWorkerVersion* version) = 0;
    virtual void OnErrorReported(ServiceWorkerVersion* version,
                                 const base::string16& error_message,
                                 int line_number,
                                 int column_number,
                                 const GURL& source_url) = 0;
  };

  ServiceWorkerVersion(
      ServiceWorkerRegistration* registration,
      int64 version_id,
      base::WeakPtr<ServiceWorkerContextCore> context);

  int64 version_id() const { return version_id_; }
  int64 registration_id() const { return registration_id_; }

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
  // This returns OK (success) if the worker is already stopped.
  void StopWorker(const StatusCallback& callback);

  // Sends an IPC message to the worker.
  // If the worker is not running this first tries to start it by
  // calling StartWorker internally.
  // |callback| can be null if the sender does not need to know if the
  // message is successfully sent or not.
  // (If the sender expects the receiver to respond please use
  // SendMessageAndRegisterCallback instead)
  void SendMessage(const IPC::Message& message, const StatusCallback& callback);

  // Sends an IPC message to the worker and registers |callback| to
  // be notified when a response message is received.
  // The |callback| will be also fired with an error code if the worker
  // is unexpectedly (being) stopped.
  // If the worker is not running this first tries to start it by
  // calling StartWorker internally.
  void SendMessageAndRegisterCallback(const IPC::Message& message,
                                      const MessageCallback& callback);

  // Sends install event to the associated embedded worker and asynchronously
  // calls |callback| when it errors out or it gets response from the worker
  // to notify install completion.
  // |active_version_id| must be a valid positive ID
  // if there's an active (previous) version running.
  //
  // This must be called when the status() is NEW. Calling this changes
  // the version's status to INSTALLING.
  // Upon completion, the version's status will be changed to INSTALLED
  // on success, or back to NEW on failure.
  void DispatchInstallEvent(int active_version_id,
                            const StatusCallback& callback);

  // Sends activate event to the associated embedded worker and asynchronously
  // calls |callback| when it errors out or it gets response from the worker
  // to notify activation completion.
  //
  // This must be called when the status() is INSTALLED. Calling this changes
  // the version's status to ACTIVATING.
  // Upon completion, the version's status will be changed to ACTIVE
  // on success, or back to INSTALLED on failure.
  void DispatchActivateEvent(const StatusCallback& callback);

  // Sends fetch event to the associated embedded worker and calls
  // |callback| with the response from the worker.
  //
  // This must be called when the status() is ACTIVE. Calling this in other
  // statuses will result in an error SERVICE_WORKER_ERROR_FAILED.
  void DispatchFetchEvent(const ServiceWorkerFetchRequest& request,
                          const FetchCallback& callback);

  // Sends sync event to the associated embedded worker and asynchronously calls
  // |callback| when it errors out or it gets response from the worker to notify
  // completion.
  //
  // This must be called when the status() is ACTIVE.
  void DispatchSyncEvent(const StatusCallback& callback);

  // These are expected to be called when a renderer process host for the
  // same-origin as for this ServiceWorkerVersion is created.  The added
  // processes are used to run an in-renderer embedded worker.
  void AddProcessToWorker(int process_id);
  void RemoveProcessFromWorker(int process_id);

  // Returns true if this has at least one process to run.
  bool HasProcessToRun() const;

  // Adds and removes a controllee's |provider_host|.
  void AddControllee(ServiceWorkerProviderHost* provider_host);
  void RemoveControllee(ServiceWorkerProviderHost* provider_host);
  void AddPendingControllee(ServiceWorkerProviderHost* provider_host);
  void RemovePendingControllee(ServiceWorkerProviderHost* provider_host);

  // Returns if it has (non-pending) controllee.
  bool HasControllee() const { return !controllee_providers_.empty(); }

  // Adds and removes Listeners.
  void AddListener(Listener* listener);
  void RemoveListener(Listener* listener);

  EmbeddedWorkerInstance* embedded_worker() { return embedded_worker_.get(); }

  // EmbeddedWorkerInstance::Observer overrides:
  virtual void OnStarted() OVERRIDE;
  virtual void OnStopped() OVERRIDE;
  virtual void OnMessageReceived(int request_id,
                                 const IPC::Message& message) OVERRIDE;
  virtual void OnReportException(const base::string16& error_message,
                                 int line_number,
                                 int column_number,
                                 const GURL& source_url) OVERRIDE;

 private:
  typedef ServiceWorkerVersion self;
  typedef std::set<ServiceWorkerProviderHost*> ProviderHostSet;
  friend class base::RefCounted<ServiceWorkerVersion>;

  virtual ~ServiceWorkerVersion();

  const int64 version_id_;
  int64 registration_id_;
  GURL script_url_;
  GURL scope_;
  Status status_;
  scoped_ptr<EmbeddedWorkerInstance> embedded_worker_;
  std::vector<StatusCallback> start_callbacks_;
  std::vector<StatusCallback> stop_callbacks_;
  std::vector<base::Closure> status_change_callbacks_;
  IDMap<MessageCallback, IDMapOwnPointer> message_callbacks_;
  ProviderHostSet controllee_providers_;
  base::WeakPtr<ServiceWorkerContextCore> context_;
  ObserverList<Listener> listeners_;

  base::WeakPtrFactory<ServiceWorkerVersion> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerVersion);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_VERSION_H_
