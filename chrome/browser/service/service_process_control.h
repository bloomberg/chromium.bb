// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SERVICE_SERVICE_PROCESS_CONTROL_H_
#define CHROME_BROWSER_SERVICE_SERVICE_PROCESS_CONTROL_H_

#include <queue>
#include <set>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/id_map.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/singleton.h"
#include "base/process.h"
#include "base/task.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "ipc/ipc_channel_proxy.h"

class CommandLine;

namespace cloud_print {
struct CloudPrintProxyInfo;
}  // namespace cloud_print

// A ServiceProcessControl works as a portal between the service process and
// the browser process.
//
// It is used to start and terminate the service process. It is also used
// to send and receive IPC messages from the service process.
//
// THREADING
//
// This class is accessed on the UI thread through some UI actions. It then
// talks to the IPC channel on the IO thread.
class ServiceProcessControl : public IPC::Channel::Sender,
                              public IPC::Channel::Listener,
                              public content::NotificationObserver {
 public:
  typedef IDMap<ServiceProcessControl>::iterator iterator;
  typedef std::queue<IPC::Message> MessageQueue;
  typedef base::Callback<void(const cloud_print::CloudPrintProxyInfo&)>
      CloudPrintProxyInfoHandler;

  // Returns the singleton instance of this class.
  static ServiceProcessControl* GetInstance();

  // Return true if this object is connected to the service.
  // Virtual for testing.
  virtual bool is_connected() const;

  // If no service process is currently running, creates a new service process
  // and connects to it. If a service process is already running this method
  // will try to connect to it.
  // |success_task| is called when we have successfully launched the process
  // and connected to it.
  // |failure_task| is called when we failed to connect to the service process.
  // It is OK to pass the same value for |success_task| and |failure_task|. In
  // this case, the task is invoked on success or failure.
  // Note that if we are already connected to service process then
  // |success_task| can be invoked in the context of the Launch call.
  // Virtual for testing.
  virtual void Launch(const base::Closure& success_task,
                      const base::Closure& failure_task);

  // Disconnect the IPC channel from the service process.
  // Virtual for testing.
  virtual void Disconnect();

  // IPC::Channel::Listener implementation.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;
  virtual void OnChannelConnected(int32 peer_pid) OVERRIDE;
  virtual void OnChannelError() OVERRIDE;

  // IPC::Channel::Sender implementation
  virtual bool Send(IPC::Message* message) OVERRIDE;

  // content::NotificationObserver implementation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // Message handlers
  void OnCloudPrintProxyInfo(
      const cloud_print::CloudPrintProxyInfo& proxy_info);

  // Send a shutdown message to the service process. IPC channel will be
  // destroyed after calling this method.
  // Return true if the message was sent.
  // Virtual for testing.
  virtual bool Shutdown();

  // Send request for cloud print proxy info (enabled state, email, proxy id).
  // The callback gets the information when received.
  bool GetCloudPrintProxyInfo(
      const CloudPrintProxyInfoHandler& cloud_print_status_callback);

 private:
  // This class is responsible for launching the service process on the
  // PROCESS_LAUNCHER thread.
  class Launcher
      : public base::RefCountedThreadSafe<ServiceProcessControl::Launcher> {
   public:
    Launcher(ServiceProcessControl* process, CommandLine* cmd_line);
    // Execute the command line to start the process asynchronously. After the
    // command is executed |task| is called with the process handle on the UI
    // thread.
    void Run(const base::Closure& task);

    bool launched() const { return launched_; }

   private:
    friend class base::RefCountedThreadSafe<ServiceProcessControl::Launcher>;
    virtual ~Launcher();

#if !defined(OS_MACOSX)
    void DoDetectLaunched();
#endif  // !OS_MACOSX

    void DoRun();
    void Notify();
    ServiceProcessControl* process_;
    scoped_ptr<CommandLine> cmd_line_;
    base::Closure notify_task_;
    bool launched_;
    uint32 retry_count_;
  };

  friend class MockServiceProcessControl;
  ServiceProcessControl();
  virtual ~ServiceProcessControl();

  friend struct DefaultSingletonTraits<ServiceProcessControl>;

  typedef std::vector<base::Closure> TaskList;

  // Helper method to invoke all the callbacks based on success or failure.
  void RunConnectDoneTasks();

  // Method called by Launcher when the service process is launched.
  void OnProcessLaunched();

  // Used internally to connect to the service process.
  void ConnectInternal();

  static void RunAllTasksHelper(TaskList* task_list);

  // IPC channel to the service process.
  scoped_ptr<IPC::ChannelProxy> channel_;

  // Service process launcher.
  scoped_refptr<Launcher> launcher_;

  // Callbacks that get invoked when the channel is successfully connected.
  TaskList connect_success_tasks_;
  // Callbacks that get invoked when there was a connection failure.
  TaskList connect_failure_tasks_;

  // Callback that gets invoked when a status message is received from
  // the cloud print proxy.
  CloudPrintProxyInfoHandler cloud_print_info_callback_;

  content::NotificationRegistrar registrar_;
};

#endif  // CHROME_BROWSER_SERVICE_SERVICE_PROCESS_CONTROL_H_
