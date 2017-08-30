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
class ServiceWorkerContentSettingsProxyImpl;
class ServiceWorkerContextCore;

// This gives an interface to control one EmbeddedWorker instance, which
// may be 'in-waiting' or running in one of the child processes added by
// AddProcessReference().
//
// Owned by ServiceWorkerVersion. Lives on the IO thread.
class CONTENT_EXPORT EmbeddedWorkerInstance
    : public mojom::EmbeddedWorkerInstanceHost {
 public:
  class DevToolsProxy;
  typedef base::Callback<void(ServiceWorkerStatusCode)> StatusCallback;

  // This enum is used in UMA histograms. Append-only.
  enum StartingPhase {
    NOT_STARTING = 0,
    ALLOCATING_PROCESS = 1,
    // REGISTERING_TO_DEVTOOLS = 2,  // Obsolete
    SENT_START_WORKER = 3,
    SCRIPT_DOWNLOADING = 4,
    SCRIPT_LOADED = 5,
    SCRIPT_EVALUATED = 6,
    // THREAD_STARTED happens after SCRIPT_LOADED and before SCRIPT_EVALUATED
    THREAD_STARTED = 7,
    // Script read happens after SENT_START_WORKER and before SCRIPT_LOADED
    // (installed scripts only)
    SCRIPT_READ_STARTED = 8,
    SCRIPT_READ_FINISHED = 9,
    SCRIPT_STREAMING = 10,
    // Add new values here.
    STARTING_PHASE_MAX_VALUE,
  };

  using ProviderInfoGetter =
      base::OnceCallback<mojom::ServiceWorkerProviderInfoForStartWorkerPtr(
          int /* process_id */)>;

  class Listener {
   public:
    virtual ~Listener() {}

    virtual void OnStarting() {}
    virtual void OnProcessAllocated() {}
    virtual void OnRegisteredToDevToolsManager() {}
    virtual void OnStartWorkerMessageSent() {}
    virtual void OnThreadStarted() {}
    virtual void OnStarted() {}

    // Called when status changed to STOPPING. The renderer has been sent a Stop
    // IPC message and OnStopped() will be called upon successful completion.
    virtual void OnStopping() {}

    // Called when status changed to STOPPED. Usually, this is called upon
    // receiving an ACK from renderer that the worker context terminated.
    // OnStopped() is also called if Stop() aborted an ongoing start attempt
    // even before the Start IPC message was sent to the renderer.  In this
    // case, OnStopping() is not called; the worker is "stopped" immediately
    // (the Start IPC is never sent).
    virtual void OnStopped(EmbeddedWorkerStatus old_status) {}

    // Called when the browser-side IPC endpoint for communication with the
    // worker died. When this is called, status is STOPPED.
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
  // to start the worker. If the worker is already installed,
  // |installed_scripts_info| holds information about its scripts; otherwise,
  // it is null.
  // |provider_info_getter| is called when this instance
  // allocates a process and is ready to send a StartWorker message.
  void Start(std::unique_ptr<EmbeddedWorkerStartParams> params,
             ProviderInfoGetter provider_info_getter,
             mojom::ServiceWorkerEventDispatcherRequest dispatcher_request,
             mojom::ServiceWorkerInstalledScriptsInfoPtr installed_scripts_info,
             const StatusCallback& callback);

  // Stops the worker. It is invalid to call this when the worker is not in
  // STARTING or RUNNING status.
  //
  // Stop() typically sends a Stop IPC to the renderer, and this instance enters
  // STOPPING status, with Listener::OnStopped() called upon completion. It can
  // synchronously complete if this instance is STARTING but the Start IPC
  // message has not yet been sent. In that case, the start procedure is
  // aborted, and this instance enters STOPPED status.
  void Stop();

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

  // Forces this instance into STOPPED status and releases any state about the
  // running worker. Called when connection with the renderer died or the
  // renderer is unresponsive.  Essentially, it throws away any information
  // about the renderer-side worker, and frees this instance up to start a new
  // worker.
  void Detach();

  base::WeakPtr<EmbeddedWorkerInstance> AsWeakPtr();

 private:
  typedef base::ObserverList<Listener> ListenerList;
  class StartTask;
  class WorkerProcessHandle;
  friend class EmbeddedWorkerRegistry;
  friend class EmbeddedWorkerInstanceTest;
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
      std::unique_ptr<DevToolsProxy> devtools_proxy,
      bool wait_for_debugger);

  // Sends StartWorker message via Mojo.
  ServiceWorkerStatusCode SendStartWorker(
      std::unique_ptr<EmbeddedWorkerStartParams> params);

  // Called back from StartTask after a start worker message is sent.
  void OnStartWorkerMessageSent(bool is_script_streaming);

  // Implements mojom::EmbeddedWorkerInstanceHost.
  // These functions all run on the IO thread.
  void OnReadyForInspection() override;
  void OnScriptLoaded() override;
  // Notifies the corresponding provider host that the thread has started and is
  // ready to receive messages.
  void OnThreadStarted(int thread_id) override;
  void OnScriptLoadFailed() override;
  // Fires the callback passed to Start().
  void OnScriptEvaluated(bool success) override;
  // Changes the internal worker status from STARTING to RUNNING.
  void OnStarted(mojom::EmbeddedWorkerStartTimingPtr start_timing) override;
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
  mojom::EmbeddedWorkerInstanceClientAssociatedPtr client_;

  // Binding for EmbeddedWorkerInstanceHost, runs on IO thread.
  mojo::AssociatedBinding<EmbeddedWorkerInstanceHost> instance_host_binding_;

  // |pending_dispatcher_request_| and |pending_installed_scripts_info_| are
  // parameters of the StartWorker message. These are called "pending" because
  // they are not used directly by this class and are just transferred to the
  // renderer in SendStartWorker().
  // TODO(shimazu): Remove |pending_dispatcher_request_| and
  // |pending_installed_scripts_info_| when EmbeddedWorkerStartParams is
  // changed to a mojo struct and we put them in EmbeddedWorkerStartParams.
  mojom::ServiceWorkerEventDispatcherRequest pending_dispatcher_request_;
  mojom::ServiceWorkerInstalledScriptsInfoPtr pending_installed_scripts_info_;

  // This is set at Start and used on SendStartWorker.
  ProviderInfoGetter provider_info_getter_;

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

  std::unique_ptr<ServiceWorkerContentSettingsProxyImpl> content_settings_;
  base::WeakPtrFactory<EmbeddedWorkerInstance> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(EmbeddedWorkerInstance);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_EMBEDDED_WORKER_INSTANCE_H_
