// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICE_SERVICE_PROCESS_H_
#define CHROME_SERVICE_SERVICE_PROCESS_H_
#pragma once

#include <string>

#include "base/gtest_prod_util.h"
#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "base/threading/thread.h"
#include "base/synchronization/waitable_event.h"
#include "chrome/service/cloud_print/cloud_print_proxy.h"
#include "chrome/service/remoting/chromoting_host_manager.h"

class ServiceProcessPrefs;
class ServiceIPCServer;
class CommandLine;

namespace net {
class NetworkChangeNotifier;
}

class CommandLine;

// The ServiceProcess does not inherit from ChildProcess because this
// process can live independently of the browser process.
class ServiceProcess : public CloudPrintProxy::Client,
                       public remoting::ChromotingHostManager::Observer {
 public:
  ServiceProcess();
  ~ServiceProcess();

  // Initialize the ServiceProcess with the message loop that it should run on.
  bool Initialize(MessageLoopForUI* message_loop,
                  const CommandLine& command_line);
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

  CloudPrintProxy* GetCloudPrintProxy();

  // CloudPrintProxy::Client implementation.
  virtual void OnCloudPrintProxyEnabled(bool persist_state);
  virtual void OnCloudPrintProxyDisabled(bool persist_state);

  // ChromotingHostManager::Observer interface.
  virtual void OnChromotingHostEnabled();
  virtual void OnChromotingHostDisabled();

#if defined(ENABLE_REMOTING)
  // Return the reference to the chromoting host only if it has started.
  remoting::ChromotingHostManager* remoting_host_manager() {
    return remoting_host_manager_;
  }
#endif

 private:
  // Schedule a call to ShutdownIfNeeded.
  void ScheduleShutdownCheck();
  // Shuts down the process if no services are enabled and no clients are
  // connected.
  void ShutdownIfNeeded();
  // Called exactly ONCE per process instance for each service that gets
  // enabled in this process.
  void OnServiceEnabled();
  // Called exactly ONCE per process instance for each service that gets
  // disabled in this process (note that shutdown != disabled).
  void OnServiceDisabled();

  scoped_ptr<net::NetworkChangeNotifier> network_change_notifier_;
  scoped_ptr<base::Thread> io_thread_;
  scoped_ptr<base::Thread> file_thread_;
  scoped_ptr<CloudPrintProxy> cloud_print_proxy_;
  scoped_ptr<ServiceProcessPrefs> service_prefs_;
  scoped_ptr<ServiceIPCServer> ipc_server_;

  // An event that will be signalled when we shutdown.
  base::WaitableEvent shutdown_event_;

  // Pointer to the main message loop that host this object.
  MessageLoop* main_message_loop_;

  // Count of currently enabled services in this process.
  int enabled_services_;

  // Speficies whether a product update is available.
  bool update_available_;

#if defined(ENABLE_REMOTING)
  scoped_refptr<remoting::ChromotingHostManager> remoting_host_manager_;
#endif
  scoped_ptr<CommandLine> command_line_;

  DISALLOW_COPY_AND_ASSIGN(ServiceProcess);
};

extern ServiceProcess* g_service_process;

#endif  // CHROME_SERVICE_SERVICE_PROCESS_H_
