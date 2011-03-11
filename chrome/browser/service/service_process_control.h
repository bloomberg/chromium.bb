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
#include "base/process.h"
#include "base/scoped_ptr.h"
#include "base/task.h"
#include "content/common/notification_observer.h"
#include "content/common/notification_registrar.h"
#include "ipc/ipc_sync_channel.h"

class Profile;
class CommandLine;

namespace remoting {
struct ChromotingHostInfo;
}  // namespace remoting

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
                              public NotificationObserver {
 public:
  typedef IDMap<ServiceProcessControl>::iterator iterator;
  typedef std::queue<IPC::Message> MessageQueue;
  typedef Callback1<const remoting::ChromotingHostInfo&>::Type
      RemotingHostStatusHandler;

  // An interface for handling messages received from the service process.
  class MessageHandler {
   public:
    virtual ~MessageHandler() {}

    // Called when we receive reply to remoting host status request.
    virtual void OnRemotingHostInfo(
        const remoting::ChromotingHostInfo& host_info) = 0;
  };

  // Construct a ServiceProcessControl with |profile|..
  explicit ServiceProcessControl(Profile* profile);
  virtual ~ServiceProcessControl();

  // Return the user profile associated with this service process.
  Profile* profile() const { return profile_; }

  // Return true if this object is connected to the service.
  bool is_connected() const { return channel_.get() != NULL; }

  // If no service process is currently running, creates a new service process
  // and connects to it.
  // If a service process is already running this method will try to connect
  // to it.
  // |success_task| is called when we have successfully launched the process
  // and connected to it.
  // |failure_task| is called when we failed to connect to the service process.
  // It is OK to pass the same value for |success_task| and |failure_task|. In
  // this case, the task is invoked on success or failure.
  // Note that if we are already connected to service process then
  // |success_task| can be invoked in the context of the Launch call.
  // Takes ownership of |success_task| and |failure_task|.
  void Launch(Task* success_task, Task* failure_task);

  // IPC::Channel::Listener implementation.
  virtual bool OnMessageReceived(const IPC::Message& message);
  virtual void OnChannelConnected(int32 peer_pid);
  virtual void OnChannelError();

  // IPC::Channel::Sender implementation
  virtual bool Send(IPC::Message* message);

  // NotificationObserver implementation.
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  // Message handlers
  void OnCloudPrintProxyIsEnabled(bool enabled, std::string email);
  void OnRemotingHostInfo(const remoting::ChromotingHostInfo& host_info);

  // Send a shutdown message to the service process. IPC channel will be
  // destroyed after calling this method.
  // Return true if the message was sent.
  bool Shutdown();

  // Send request for cloud print proxy status and the registered
  // email address. The callback gets the information when received.
  bool GetCloudPrintProxyStatus(
      Callback2<bool, std::string>::Type* cloud_print_status_callback);

  // Send a message to enable the remoting service in the service process.
  // Return true if the message was sent.
  bool SetRemotingHostCredentials(const std::string& user,
                                  const std::string& auth_token);

  bool EnableRemotingHost();
  bool DisableRemotingHost();

  // Send request for current status of the remoting service.
  // MessageHandler::OnRemotingHostInfo() will be called when remoting host
  // status is available.
  bool RequestRemotingHostStatus();

  // Add a message handler for receiving messages from the service
  // process.
  void AddMessageHandler(MessageHandler* message_handler);

  // Remove a message handler from the list of message handlers. Must
  // not be called from a message handler (i.e. while a message is
  // being processed).
  void RemoveMessageHandler(MessageHandler* message_handler);

 private:
  // This class is responsible for launching the service process on the
  // PROCESS_LAUNCHER thread.
  class Launcher
      : public base::RefCountedThreadSafe<ServiceProcessControl::Launcher> {
   public:
    Launcher(ServiceProcessControl* process, CommandLine* cmd_line);
    // Execute the command line to start the process asynchronously.
    // After the comamnd is executed |task| is called with the process handle on
    // the UI thread.
    void Run(Task* task);

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
    scoped_ptr<Task> notify_task_;
    bool launched_;
    uint32 retry_count_;
  };

  typedef std::vector<Task*> TaskList;

  // Helper method to invoke all the callbacks based on success on failure.
  void RunConnectDoneTasks();

  // Method called by Launcher when the service process is launched.
  void OnProcessLaunched();

  // Used internally to connect to the service process.
  void ConnectInternal();

  static void RunAllTasksHelper(TaskList* task_list);

  Profile* profile_;

  // IPC channel to the service process.
  scoped_ptr<IPC::SyncChannel> channel_;

  // Service process launcher.
  scoped_refptr<Launcher> launcher_;

  // Callbacks that get invoked when the channel is successfully connected or
  // if there was a failure in connecting.
  TaskList connect_done_tasks_;
  // Callbacks that get invoked ONLY when the channel is successfully connected.
  TaskList connect_success_tasks_;
  // Callbacks that get invoked ONLY when there was a connection failure.
  TaskList connect_failure_tasks_;

  // Callback that gets invoked when a status message is received from
  // the cloud print proxy.
  scoped_ptr<Callback2<bool, std::string>::Type> cloud_print_status_callback_;

  // Handler for messages from service process.
  std::set<MessageHandler*> message_handlers_;

  NotificationRegistrar registrar_;
};

#endif  // CHROME_BROWSER_SERVICE_SERVICE_PROCESS_CONTROL_H_
