// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_EMBEDDED_WORKER_INSTANCE_H_
#define CONTENT_BROWSER_SERVICE_WORKER_EMBEDDED_WORKER_INSTANCE_H_

#include <stdint.h>

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/callback_forward.h"
#include "base/gtest_prod_util.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/strings/string16.h"
#include "base/time/time.h"
#include "content/browser/service_worker/embedded_worker_status.h"
#include "content/browser/service_worker/service_worker_metrics.h"
#include "content/common/content_export.h"
#include "content/common/service_worker/embedded_worker.mojom.h"
#include "content/common/service_worker/service_worker_event_dispatcher.mojom.h"
#include "content/common/service_worker/service_worker_status_code.h"
#include "mojo/public/cpp/bindings/associated_binding.h"
#include "url/gurl.h"

// Windows headers will redefine SendMessage.
#ifdef SendMessage
#undef SendMessage
#endif

namespace IPC {
class Message;
}

namespace content {

class EmbeddedWorkerRegistry;
struct EmbeddedWorkerStartParams;
class ServiceWorkerContextCore;

// This gives an interface to control one EmbeddedWorker instance, which
// may be 'in-waiting' or running in one of the child processes added by
// AddProcessReference().
class CONTENT_EXPORT EmbeddedWorkerInstance
    : NON_EXPORTED_BASE(public mojom::EmbeddedWorkerInstanceHost) {
 public:
  class DevToolsProxy;
  typedef base::Callback<void(ServiceWorkerStatusCode)> StatusCallback;

  // This enum is used in UMA histograms. Append-only.
  enum StartingPhase {
    NOT_STARTING,
    ALLOCATING_PROCESS,
    REGISTERING_TO_DEVTOOLS,
    SENT_START_WORKER,
    SCRIPT_DOWNLOADING,
    SCRIPT_LOADED,
    SCRIPT_EVALUATED,
    THREAD_STARTED,  // Happens after SCRIPT_LOADED and before SCRIPT_EVALUATED
    // Script read happens after SENT_START_WORKER and before SCRIPT_LOADED
    // (installed scripts only)
    SCRIPT_READ_STARTED,
    SCRIPT_READ_FINISHED,
    // Add new values here.
    STARTING_PHASE_MAX_VALUE,
  };

  class Listener {
   public:
    virtual ~Listener() {}

    virtual void OnStarting() {}
    virtual void OnProcessAllocated() {}
    virtual void OnRegisteredToDevToolsManager() {}
    virtual void OnStartWorkerMessageSent() {}
    virtual void OnThreadStarted() {}
    virtual void OnStarted() {}

    virtual void OnStopping() {}
    // Received ACK from renderer that the worker context terminated.
    virtual void OnStopped(EmbeddedWorkerStatus old_status) {}
    // The browser-side IPC endpoint for communication with the worker died.
    virtual void OnDetached(EmbeddedWorkerStatus old_status) {}
    virtual void OnScriptLoaded() {}
    virtual void OnScriptLoadFailed() {}
    virtual void OnReportException(const base::string16& error_message,
                                   int line_number,
                                   int column_number,
                                   const GURL& source_url) {}
    virtual void OnReportConsoleMessage(int source_identifier,
                                        int message_level,
                                        const base::string16& message,
                                        int line_number,
                                        const GURL& source_url) {}
    // Returns false if the message is not handled by this listener.
    CONTENT_EXPORT virtual bool OnMessageReceived(const IPC::Message& message);
  };

  ~EmbeddedWorkerInstance() override;

  // Starts the worker. It is invalid to call this when the worker is not in
  // STOPPED status. |callback| is invoked after the worker script has been
  // started and evaluated, or when an error occurs.
  // |params| should be populated with service worker version info needed
  // to start the worker.
  void Start(std::unique_ptr<EmbeddedWorkerStartParams> params,
             mojom::ServiceWorkerEventDispatcherRequest dispatcher_request,
             const StatusCallback& callback);

  // Stops the worker. It is invalid to call this when the worker is
  // not in STARTING or RUNNING status.
  // This returns false when StopWorker IPC couldn't be sent to the worker.
  bool Stop();

  // Stops the worker if the worker is not being debugged (i.e. devtools is
  // not attached). This method is called by a stop-worker timer to kill
  // idle workers.
  void StopIfIdle();

  // Sends |message| to the embedded worker running in the child process.
  // It is invalid to call this while the worker is not in STARTING or RUNNING
  // status.
  ServiceWorkerStatusCode SendMessage(const IPC::Message& message);

  // Resumes the worker if it paused after download.
  void ResumeAfterDownload();

  int embedded_worker_id() const { return embedded_worker_id_; }
  EmbeddedWorkerStatus status() const { return status_; }
  StartingPhase starting_phase() const {
    DCHECK_EQ(EmbeddedWorkerStatus::STARTING, status());
    return starting_phase_;
  }
  int restart_count() const { return restart_count_; }
  int process_id() const;
  int thread_id() const { return thread_id_; }
  // This should be called only when the worker instance has a valid process,
  // that is, when |process_id()| returns a valid process id.
  bool is_new_process() const;
  int worker_devtools_agent_route_id() const;

  void AddListener(Listener* listener);
  void RemoveListener(Listener* listener);

  void SetDevToolsAttached(bool attached);
  bool devtools_attached() const { return devtools_attached_; }

  bool network_accessed_for_script() const {
    return network_accessed_for_script_;
  }

  ServiceWorkerMetrics::StartSituation start_situation() const {
    DCHECK(status() == EmbeddedWorkerStatus::STARTING ||
           status() == EmbeddedWorkerStatus::RUNNING);
    return start_situation_;
  }

  // Called when the main script load accessed the network.
  void OnNetworkAccessedForScriptLoad();

  // Called when reading the main script from the service worker script cache
  // begins and ends.
  void OnScriptReadStarted();
  void OnScriptReadFinished();

  // Called when the worker is installed.
  void OnWorkerVersionInstalled();

  // Called when the worker is doomed.
  void OnWorkerVersionDoomed();

  // Called when the net::URLRequestJob to load the service worker script
  // created. Not called for import scripts.
  void OnURLJobCreatedForMainScript();

  // Add message to the devtools console.
  void AddMessageToConsole(blink::WebConsoleMessage::Level level,
                           const std::string& message);

  static std::string StatusToString(EmbeddedWorkerStatus status);
  static std::string StartingPhaseToString(StartingPhase phase);

  void Detach();

  base::WeakPtr<EmbeddedWorkerInstance> AsWeakPtr();

 private:
  typedef base::ObserverList<Listener> ListenerList;
  class StartTask;
  class WorkerProcessHandle;
  friend class EmbeddedWorkerRegistry;
  FRIEND_TEST_ALL_PREFIXES(EmbeddedWorkerInstanceTest, StartAndStop);
  FRIEND_TEST_ALL_PREFIXES(EmbeddedWorkerInstanceTest, DetachDuringStart);
  FRIEND_TEST_ALL_PREFIXES(EmbeddedWorkerInstanceTest, StopDuringStart);

  // Constructor is called via EmbeddedWorkerRegistry::CreateWorker().
  // This instance holds a ref of |registry|.
  EmbeddedWorkerInstance(base::WeakPtr<ServiceWorkerContextCore> context,
                         int embedded_worker_id);

  // Called back from StartTask after a process is allocated on the UI thread.
  void OnProcessAllocated(std::unique_ptr<WorkerProcessHandle> handle,
                          ServiceWorkerMetrics::StartSituation start_situation);

  // Called back from StartTask after the worker is registered to
  // WorkerDevToolsManager.
  void OnRegisteredToDevToolsManager(
      bool is_new_process,
      std::unique_ptr<DevToolsProxy> devtools_proxy,
      bool wait_for_debugger);

  // Sends StartWorker message via Mojo.
  ServiceWorkerStatusCode SendStartWorker(
      std::unique_ptr<EmbeddedWorkerStartParams> params);

  // Called back from StartTask after a start worker message is sent.
  void OnStartWorkerMessageSent();

  // Implements mojom::EmbeddedWorkerInstanceHost.
  // These functions all run on the IO thread.
  void OnReadyForInspection() override;
  void OnScriptLoaded() override;
  // Notifies the corresponding provider host that the thread has started and is
  // ready to receive messages.
  void OnThreadStarted(int thread_id, int provider_id) override;
  void OnScriptLoadFailed() override;
  // Fires the callback passed to Start().
  void OnScriptEvaluated(bool success) override;
  // Changes the internal worker status from STARTING to RUNNING.
  void OnStarted() override;
  // Resets the embedded worker instance to the initial state. This will change
  // the internal status from STARTING or RUNNING to STOPPED.
  void OnStopped() override;
  void OnReportException(const base::string16& error_message,
                         int line_number,
                         int column_number,
                         const GURL& source_url) override;
  void OnReportConsoleMessage(int source_identifier,
                              int message_level,
                              const base::string16& message,
                              int line_number,
                              const GURL& source_url) override;

  // Called when ServiceWorkerDispatcherHost for the worker died while it was
  // running.
  void OnDetached();

  // Called back from Registry when the worker instance sends message
  // to the browser (i.e. EmbeddedWorker observers).
  // Returns false if the message is not handled.
  bool OnMessageReceived(const IPC::Message& message);

  // Resets all running state. After this function is called, |status_| is
  // STOPPED.
  void ReleaseProcess();

  // Called back from StartTask when the startup sequence failed. Calls
  // ReleaseProcess() and invokes |callback| with |status|. May destroy |this|.
  void OnStartFailed(const StatusCallback& callback,
                     ServiceWorkerStatusCode status);

  // Returns the time elapsed since |step_time_| and updates |step_time_|
  // to the current time.
  base::TimeDelta UpdateStepTime();

  base::WeakPtr<ServiceWorkerContextCore> context_;
  scoped_refptr<EmbeddedWorkerRegistry> registry_;

  // Unique within an EmbeddedWorkerRegistry.
  const int embedded_worker_id_;

  EmbeddedWorkerStatus status_;
  StartingPhase starting_phase_;
  int restart_count_;

  // Current running information.
  std::unique_ptr<EmbeddedWorkerInstance::WorkerProcessHandle> process_handle_;
  int thread_id_;

  // |client_| is used to send messages to the renderer process.
  mojom::EmbeddedWorkerInstanceClientPtr client_;

  // Binding for EmbeddedWorkerInstanceHost, runs on IO thread.
  mojo::AssociatedBinding<EmbeddedWorkerInstanceHost> instance_host_binding_;

  // TODO(shimazu): Remove this after EmbeddedWorkerStartParams is changed to
  // a mojo struct.
  mojom::ServiceWorkerEventDispatcherRequest pending_dispatcher_request_;

  // Whether devtools is attached or not.
  bool devtools_attached_;

  // True if the script load request accessed the network. If the script was
  // served from HTTPCache or ServiceWorkerDatabase this value is false.
  bool network_accessed_for_script_;

  ListenerList listener_list_;
  std::unique_ptr<DevToolsProxy> devtools_proxy_;

  std::unique_ptr<StartTask> inflight_start_task_;

  // This is valid only after a process is allocated for the worker.
  ServiceWorkerMetrics::StartSituation start_situation_ =
      ServiceWorkerMetrics::StartSituation::UNKNOWN;

  // Used for UMA. The start time of the current start sequence step.
  base::TimeTicks step_time_;

  base::WeakPtrFactory<EmbeddedWorkerInstance> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(EmbeddedWorkerInstance);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_EMBEDDED_WORKER_INSTANCE_H_
