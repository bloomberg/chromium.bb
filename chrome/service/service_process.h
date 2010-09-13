// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICE_SERVICE_PROCESS_H_
#define CHROME_SERVICE_SERVICE_PROCESS_H_
#pragma once

#include "base/gtest_prod_util.h"
#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "base/thread.h"
#include "base/waitable_event.h"
#include "chrome/service/cloud_print/cloud_print_proxy.h"
#include "chrome/service/remoting/remoting_directory_service.h"

class JsonPrefStore;
class ServiceIPCServer;

namespace net {
class NetworkChangeNotifier;
}

namespace remoting {
class ChromotingHost;
class ChromotingHostContext;
class HostKeyPair;
class JsonHostConfig;
}

// The ServiceProcess does not inherit from ChildProcess because this
// process can live independently of the browser process.
class ServiceProcess : public RemotingDirectoryService::Client,
                       public CloudPrintProxy::Client {
 public:
  ServiceProcess();
  ~ServiceProcess();

  // Initialize the ServiceProcess with the message loop that it should run on.
  bool Initialize(MessageLoop* message_loop);
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

  CloudPrintProxy* GetCloudPrintProxy();

  // CloudPrintProxy::Client implementation.
  virtual void OnCloudPrintProxyEnabled();
  virtual void OnCloudPrintProxyDisabled();

#if defined(ENABLE_REMOTING)
  // Return the reference to the chromoting host only if it has started.
  remoting::ChromotingHost* GetChromotingHost() { return chromoting_host_; }

  // Enable chromoting host with the tokens.
  // Return true if successful.
  bool EnableChromotingHostWithTokens(const std::string& login,
                                      const std::string& remoting_token,
                                      const std::string& talk_token);

  // Start running the chromoting host asynchronously.
  // Return true if chromoting host has started.
  bool StartChromotingHost();

  // Shutdown chromoting host. Return true if chromoting host was shutdown.
  // The shutdown process will happen asynchronously.
  bool ShutdownChromotingHost();

  // RemotingDirectoryService::Client implementation.
  virtual void OnRemotingHostAdded();
  virtual void OnRemotingDirectoryError();
#endif

 private:
  // Shuts down the process if no services are enabled.
  void ShutdownIfNoServices();
  // Called exactly ONCE per process instance for each service that gets
  // enabled in this process.
  void OnServiceEnabled();
  // Called exactly ONCE per process instance for each service that gets
  // disabled in this process (note that shutdown != disabled).
  void OnServiceDisabled();

  bool AddServiceProcessToAutoStart();
  bool RemoveServiceProcessFromAutoStart();

#if defined(ENABLE_REMOTING)
  FRIEND_TEST_ALL_PREFIXES(ServiceProcessTest, RunChromoting);
  FRIEND_TEST_ALL_PREFIXES(ServiceProcessTest, RunChromotingUntilShutdown);

  // Save authenication token to the json config file.
  void SaveChromotingConfig(
      const std::string& login,
      const std::string& token,
      const std::string& host_id,
      const std::string& host_name,
      remoting::HostKeyPair* host_key_pair);

  // Load settings for chromoting from json file.
  void LoadChromotingConfig();

  // This method is called when chromoting is shutting down. This is virtual
  // for used in the test.
  virtual void OnChromotingHostShutdown();
#endif

  scoped_ptr<net::NetworkChangeNotifier> network_change_notifier_;
  scoped_ptr<base::Thread> io_thread_;
  scoped_ptr<base::Thread> file_thread_;
  scoped_ptr<CloudPrintProxy> cloud_print_proxy_;
  scoped_ptr<JsonPrefStore> service_prefs_;
  scoped_ptr<ServiceIPCServer> ipc_server_;

#if defined(ENABLE_REMOTING)
  scoped_refptr<remoting::JsonHostConfig> chromoting_config_;
  scoped_ptr<remoting::ChromotingHostContext> chromoting_context_;
  scoped_refptr<remoting::ChromotingHost> chromoting_host_;
  scoped_ptr<RemotingDirectoryService> remoting_directory_;

  // Temporary storage for remoting credentials. The content is cleared
  // after it is saved.
  std::string remoting_login_;
  std::string remoting_token_;
  std::string talk_token_;
#endif

  // An event that will be signalled when we shutdown.
  base::WaitableEvent shutdown_event_;

  // Pointer to the main message loop that host this object.
  MessageLoop* main_message_loop_;

  // Count of currently enabled services in this process.
  int enabled_services_;

  DISALLOW_COPY_AND_ASSIGN(ServiceProcess);
};

extern ServiceProcess* g_service_process;

#endif  // CHROME_SERVICE_SERVICE_PROCESS_H_
