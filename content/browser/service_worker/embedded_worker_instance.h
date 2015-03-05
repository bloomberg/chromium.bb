// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_EMBEDDED_WORKER_INSTANCE_H_
#define CONTENT_BROWSER_SERVICE_WORKER_EMBEDDED_WORKER_INSTANCE_H_

#include <map>
#include <vector>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/callback_forward.h"
#include "base/gtest_prod_util.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/strings/string16.h"
#include "base/time/time.h"
#include "content/common/content_export.h"
#include "content/common/service_worker/service_worker_status_code.h"
#include "url/gurl.h"

// Windows headers will redefine SendMessage.
#ifdef SendMessage
#undef SendMessage
#endif

struct EmbeddedWorkerMsg_StartWorker_Params;

namespace IPC {
class Message;
}

namespace content {

class EmbeddedWorkerRegistry;
class MessagePortMessageFilter;
class ServiceWorkerContextCore;
struct ServiceWorkerFetchRequest;

// This gives an interface to control one EmbeddedWorker instance, which
// may be 'in-waiting' or running in one of the child processes added by
// AddProcessReference().
class CONTENT_EXPORT EmbeddedWorkerInstance {
 public:
  typedef base::Callback<void(ServiceWorkerStatusCode)> StatusCallback;
  enum Status {
    STOPPED,
    STARTING,
    RUNNING,
    STOPPING,
  };

  // This enum is used in UMA histograms, so don't change the order or remove
  // entries.
  enum StartingPhase {
    NOT_STARTING,
    ALLOCATING_PROCESS,
    REGISTERING_TO_DEVTOOLS,
    SENT_START_WORKER,
    SCRIPT_DOWNLOADING,
    SCRIPT_LOADED,
    SCRIPT_EVALUATED,
    STARTING_PHASE_MAX_VALUE,
  };

  class Listener {
   public:
    virtual ~Listener() {}
    virtual void OnScriptLoaded() {}
    virtual void OnStarted() {}
    virtual void OnStopped(Status old_status) {}
    virtual void OnPausedAfterDownload() {}
    virtual void OnReportException(const base::string16& error_message,
                                   int line_number,
                                   int column_number,
                                   const GURL& source_url) {}
    virtual void OnReportConsoleMessage(int source_identifier,
                                        int message_level,
                                        const base::string16& message,
                                        int line_number,
                                        const GURL& source_url) {}
    // These should return false if the message is not handled by this
    // listener. (TODO(kinuko): consider using IPC::Listener interface)
    // TODO(kinuko): Deprecate OnReplyReceived.
    virtual bool OnMessageReceived(const IPC::Message& message) = 0;
  };

  ~EmbeddedWorkerInstance();

  // Starts the worker. It is invalid to call this when the worker is not in
  // STOPPED status. |callback| is invoked after the worker script has been
  // started and evaluated, or when an error occurs.
  void Start(int64 service_worker_version_id,
             const GURL& scope,
             const GURL& script_url,
             bool pause_after_download,
             const StatusCallback& callback);

  // Stops the worker. It is invalid to call this when the worker is
  // not in STARTING or RUNNING status.
  // This returns false if stopping a worker fails immediately, e.g. when
  // IPC couldn't be sent to the worker.
  ServiceWorkerStatusCode Stop();

  // Stops the worker if the worker is not being debugged (i.e. devtools is
  // not attached). This method is called by a stop-worker timer to kill
  // idle workers.
  void StopIfIdle();

  // Sends |message| to the embedded worker running in the child process.
  // It is invalid to call this while the worker is not in STARTING or RUNNING
  // status.
  ServiceWorkerStatusCode SendMessage(const IPC::Message& message);

  void ResumeAfterDownload();

  int embedded_worker_id() const { return embedded_worker_id_; }
  Status status() const { return status_; }
  StartingPhase starting_phase() const {
    DCHECK_EQ(STARTING, status());
    return starting_phase_;
  }
  int process_id() const { return process_id_; }
  int thread_id() const { return thread_id_; }
  int worker_devtools_agent_route_id() const;
  MessagePortMessageFilter* message_port_message_filter() const;

  void AddListener(Listener* listener);
  void RemoveListener(Listener* listener);

  void set_devtools_attached(bool attached) { devtools_attached_ = attached; }
  bool devtools_attached() const { return devtools_attached_; }

  // Called when the script load request accessed the network.
  void OnNetworkAccessedForScriptLoad();

  static std::string StatusToString(Status status);
  static std::string StartingPhaseToString(StartingPhase phase);

 private:
  typedef ObserverList<Listener> ListenerList;
  class DevToolsProxy;
  friend class EmbeddedWorkerRegistry;
  FRIEND_TEST_ALL_PREFIXES(EmbeddedWorkerInstanceTest, StartAndStop);

  // Constructor is called via EmbeddedWorkerRegistry::CreateWorker().
  // This instance holds a ref of |registry|.
  EmbeddedWorkerInstance(base::WeakPtr<ServiceWorkerContextCore> context,
                         int embedded_worker_id);

  // Called back from ServiceWorkerProcessManager after Start() passes control
  // to the UI thread to acquire a reference to the process.
  static void RunProcessAllocated(
      base::WeakPtr<EmbeddedWorkerInstance> instance,
      base::WeakPtr<ServiceWorkerContextCore> context,
      scoped_ptr<EmbeddedWorkerMsg_StartWorker_Params> params,
      const EmbeddedWorkerInstance::StatusCallback& callback,
      ServiceWorkerStatusCode status,
      int process_id);
  void ProcessAllocated(scoped_ptr<EmbeddedWorkerMsg_StartWorker_Params> params,
                        const StatusCallback& callback,
                        int process_id,
                        ServiceWorkerStatusCode status);
  // Called back after ProcessAllocated() passes control to the UI thread to
  // register to WorkerDevToolsManager.
  void SendStartWorker(scoped_ptr<EmbeddedWorkerMsg_StartWorker_Params> params,
                       const StatusCallback& callback,
                       int worker_devtools_agent_route_id,
                       bool wait_for_debugger);

  // Called back from Registry when the worker instance has ack'ed that
  // it is ready for inspection.
  void OnReadyForInspection();

  // Called back from Registry when the worker instance has ack'ed that
  // it finished loading the script and has started a worker thread.
  void OnScriptLoaded(int thread_id);

  // Called back from Registry when the worker instance has ack'ed that
  // it failed to load the script.
  void OnScriptLoadFailed();

  // Called back from Registry when the worker instance has ack'ed that
  // it finished evaluating the script. This is called before OnStarted.
  void OnScriptEvaluated(bool success);

  // Called back from Registry when the worker instance has ack'ed that its
  // WorkerGlobalScope has actually started and evaluated the script. This is
  // called after OnScriptEvaluated.
  // This will change the internal status from STARTING to RUNNING.
  void OnStarted();

  void OnPausedAfterDownload();

  // Called back from Registry when the worker instance has ack'ed that
  // its WorkerGlobalScope is actually stopped in the child process.
  // This will change the internal status from STARTING or RUNNING to
  // STOPPED.
  void OnStopped();

  // Called back from Registry when the worker instance sends message
  // to the browser (i.e. EmbeddedWorker observers).
  // Returns false if the message is not handled.
  bool OnMessageReceived(const IPC::Message& message);

  // Called back from Registry when the worker instance reports the exception.
  void OnReportException(const base::string16& error_message,
                         int line_number,
                         int column_number,
                         const GURL& source_url);

  // Called back from Registry when the worker instance reports to the console.
  void OnReportConsoleMessage(int source_identifier,
                              int message_level,
                              const base::string16& message,
                              int line_number,
                              const GURL& source_url);

  base::WeakPtr<ServiceWorkerContextCore> context_;
  scoped_refptr<EmbeddedWorkerRegistry> registry_;
  const int embedded_worker_id_;
  Status status_;
  StartingPhase starting_phase_;

  // Current running information. -1 indicates the worker is not running.
  int process_id_;
  int thread_id_;

  // Whether devtools is attached or not.
  bool devtools_attached_;

  // True if the script load request accessed the network. If the script was
  // served from HTTPCache or ServiceWorkerDatabase this value is false.
  bool network_accessed_for_script_;

  StatusCallback start_callback_;
  ListenerList listener_list_;
  scoped_ptr<DevToolsProxy> devtools_proxy_;

  base::TimeTicks start_timing_;

  base::WeakPtrFactory<EmbeddedWorkerInstance> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(EmbeddedWorkerInstance);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_EMBEDDED_WORKER_INSTANCE_H_
