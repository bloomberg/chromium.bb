// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICE_SERVICE_PROCESS_H_
#define CHROME_SERVICE_SERVICE_PROCESS_H_

#include <string>

#include "base/gtest_prod_util.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/threading/sequenced_worker_pool.h"
#include "base/threading/thread.h"
#include "base/synchronization/waitable_event.h"
#include "chrome/service/cloud_print/cloud_print_proxy.h"

class ServiceProcessPrefs;
class ServiceIPCServer;
class ServiceURLRequestContextGetter;
class ServiceProcessState;

namespace base {
class CommandLine;
}

namespace net {
class NetworkChangeNotifier;
}

// The ServiceProcess does not inherit from ChildProcess because this
// process can live independently of the browser process.
// ServiceProcess Design Notes
// https://sites.google.com/a/chromium.org/dev/developers/design-documents/service-processes
class ServiceProcess : public cloud_print::CloudPrintProxy::Client {
 public:
  ServiceProcess();
  virtual ~ServiceProcess();

  // Initialize the ServiceProcess with the message loop that it should run on.
  // ServiceProcess takes ownership of |state|.
  bool Initialize(base::MessageLoopForUI* message_loop,
                  const base::CommandLine& command_line,
                  ServiceProcessState* state);

  bool Teardown();
  // TODO(sanjeevr): Change various parts of the code such as
  // net::ProxyService::CreateSystemProxyConfigService to take in
  // MessageLoopProxy* instead of MessageLoop*. When we have done that, we can
  // remove the io_thread() and file_thread() accessors and replace them with
  // io_message_loop_proxy() and file_message_loop_proxy() respectively.

  // Returns the thread that we perform I/O coordination on (network requests,
  // communication with renderers, etc.
  // NOTE: You should ONLY use this to pass to IPC or other objects which must
  // need a MessageLoop*. If you just want to post a task, use the thread's
  // message_loop_proxy() as it takes care of checking that a thread is still
  // alive, race conditions, lifetime differences etc.
  // If you still must use this, need to check the return value for NULL.
  base::Thread* io_thread() const {
    return io_thread_.get();
  }
  // Returns the thread that we perform random file operations on. For code
  // that wants to do I/O operations (not network requests or even file: URL
  // requests), this is the thread to use to avoid blocking the UI thread.
  base::Thread* file_thread() const {
    return file_thread_.get();
  }

  // A global event object that is signalled when the main thread's message
  // loop exits. This gives background threads a way to observe the main
  // thread shutting down.
  base::WaitableEvent* shutdown_event() {
    return &shutdown_event_;
  }

  // Shutdown the service process. This is likely triggered by a IPC message.
  void Shutdown();

  void SetUpdateAvailable() {
    update_available_ = true;
  }
  bool update_available() const { return update_available_; }

  // Called by the IPC server when a client disconnects. A return value of
  // true indicates that the IPC server should continue listening for new
  // connections.
  bool HandleClientDisconnect();

  cloud_print::CloudPrintProxy* GetCloudPrintProxy();

  // CloudPrintProxy::Client implementation.
  virtual void OnCloudPrintProxyEnabled(bool persist_state) OVERRIDE;
  virtual void OnCloudPrintProxyDisabled(bool persist_state) OVERRIDE;

  ServiceURLRequestContextGetter* GetServiceURLRequestContextGetter();

 private:
  friend class TestServiceProcess;

  // Schedule a call to ShutdownIfNeeded.
  void ScheduleShutdownCheck();

  // Shuts down the process if no services are enabled and no clients are
  // connected.
  void ShutdownIfNeeded();

  // Schedule a call to CloudPrintPolicyCheckIfNeeded.
  void ScheduleCloudPrintPolicyCheck();

  // Launch the browser for a policy check if we're not connected.
  void CloudPrintPolicyCheckIfNeeded();

  // Called exactly ONCE per process instance for each service that gets
  // enabled in this process.
  void OnServiceEnabled();

  // Called exactly ONCE per process instance for each service that gets
  // disabled in this process (note that shutdown != disabled).
  void OnServiceDisabled();

  // Terminate forces the service process to quit.
  void Terminate();

  scoped_ptr<net::NetworkChangeNotifier> network_change_notifier_;
  scoped_ptr<base::Thread> io_thread_;
  scoped_ptr<base::Thread> file_thread_;
  scoped_refptr<base::SequencedWorkerPool> blocking_pool_;
  scoped_ptr<cloud_print::CloudPrintProxy> cloud_print_proxy_;
  scoped_ptr<ServiceProcessPrefs> service_prefs_;
  scoped_ptr<ServiceIPCServer> ipc_server_;
  scoped_ptr<ServiceProcessState> service_process_state_;

  // An event that will be signalled when we shutdown.
  base::WaitableEvent shutdown_event_;

  // Pointer to the main message loop that host this object.
  base::MessageLoop* main_message_loop_;

  // Count of currently enabled services in this process.
  int enabled_services_;

  // Speficies whether a product update is available.
  bool update_available_;

  scoped_refptr<ServiceURLRequestContextGetter> request_context_getter_;

  DISALLOW_COPY_AND_ASSIGN(ServiceProcess);
};

extern ServiceProcess* g_service_process;

#endif  // CHROME_SERVICE_SERVICE_PROCESS_H_
