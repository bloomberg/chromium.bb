// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MACH_BROKER_MAC_H_
#define CONTENT_BROWSER_MACH_BROKER_MAC_H_

#include <mach/mach.h>

#include <map>
#include <string>

#include "base/mac/dispatch_source_mach.h"
#include "base/mac/scoped_mach_port.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/singleton.h"
#include "base/process/port_provider_mac.h"
#include "base/process/process_handle.h"
#include "base/synchronization/lock.h"
#include "content/public/browser/browser_child_process_observer.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

namespace content {

// On OS X, the task port of a process is required to collect metrics about the
// process, and to insert Mach ports into the process. Running |task_for_pid()|
// is only allowed for privileged code. However, a process has port rights to
// all its subprocesses, so let the browser's child processes send their Mach
// port to the browser over IPC.
//
// Mach ports can only be sent over Mach IPC, not over the |socketpair()| that
// the regular IPC system uses. Hence, the child processes open a Mach
// connection shortly after launching and ipc their mach data to the browser
// process. This data is kept in a global |MachBroker| object.
//
// Since this data arrives over a separate channel, it is not available
// immediately after a child process has been started.
class CONTENT_EXPORT MachBroker : public base::PortProvider,
                                  public BrowserChildProcessObserver,
                                  public NotificationObserver {
 public:
  // For use in child processes. This will send the task port of the current
  // process over Mach IPC to the port registered by name (via this class) in
  // the parent process. Returns true if the message was sent successfully
  // and false if otherwise.
  static bool ChildSendTaskPortToParent();

  // Returns the global MachBroker.
  static MachBroker* GetInstance();

  // The lock that protects this MachBroker object.  Clients MUST acquire and
  // release this lock around calls to EnsureRunning(), PlaceholderForPid(),
  // and FinalizePid().
  base::Lock& GetLock();

  // Performs any necessary setup that cannot happen in the constructor.
  // Callers MUST acquire the lock given by GetLock() before calling this
  // method (and release the lock afterwards).
  void EnsureRunning();

  // Adds a placeholder to the map for the given pid with MACH_PORT_NULL.
  // Callers are expected to later update the port with FinalizePid().  Callers
  // MUST acquire the lock given by GetLock() before calling this method (and
  // release the lock afterwards).
  void AddPlaceholderForPid(base::ProcessHandle pid, int child_process_id);

  // Implement |base::PortProvider|.
  mach_port_t TaskForPid(base::ProcessHandle process) const override;

  // Implement |BrowserChildProcessObserver|.
  void BrowserChildProcessHostDisconnected(
      const ChildProcessData& data) override;
  void BrowserChildProcessCrashed(const ChildProcessData& data,
      int exit_code) override;

  // Implement |NotificationObserver|.
  void Observe(int type,
               const NotificationSource& source,
               const NotificationDetails& details) override;

  // Returns the Mach port name to use when sending or receiving messages.
  // Does the Right Thing in the browser and in child processes.
  static std::string GetMachPortName();

 private:
  friend class MachBrokerTest;
  friend struct base::DefaultSingletonTraits<MachBroker>;

  MachBroker();
  ~MachBroker() override;

  // Performs any initialization work.
  bool Init();

  // Message handler that is invoked on |dispatch_source_| when an
  // incoming message needs to be received.
  void HandleRequest();

  // Updates the mapping for |pid| to include the given |mach_info|.  Does
  // nothing if PlaceholderForPid() has not already been called for the given
  // |pid|.  Callers MUST acquire the lock given by GetLock() before calling
  // this method (and release the lock afterwards).
  void FinalizePid(base::ProcessHandle pid, mach_port_t task_port);

  // Removes all mappings belonging to |child_process_id| from the broker.
  void InvalidateChildProcessId(int child_process_id);

  // Callback used to register notifications on the UI thread.
  void RegisterNotifications();

  // Whether or not the class has been initialized.
  bool initialized_;

  // Used to register for notifications received by NotificationObserver.
  // Accessed only on the UI thread.
  NotificationRegistrar registrar_;

  // The Mach port on which the server listens.
  base::mac::ScopedMachReceiveRight server_port_;

  // The dispatch source and queue on which Mach messages will be received.
  scoped_ptr<base::DispatchSourceMach> dispatch_source_;

  // Stores mach info for every process in the broker.
  typedef std::map<base::ProcessHandle, mach_port_t> MachMap;
  MachMap mach_map_;

  // Stores the Child process unique id (RenderProcessHost ID) for every
  // process.
  typedef std::map<int, base::ProcessHandle> ChildProcessIdMap;
  ChildProcessIdMap child_process_id_map_;

  // Mutex that guards |mach_map_| and |child_process_id_map_|.
  mutable base::Lock lock_;

  DISALLOW_COPY_AND_ASSIGN(MachBroker);
};

}  // namespace content

#endif  // CONTENT_BROWSER_MACH_BROKER_MAC_H_
