// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MACH_BROKER_H_
#define CHROME_BROWSER_MACH_BROKER_H_
#pragma once

#include <map>
#include <string>

#include <mach/mach.h>

#include "base/process.h"
#include "base/process_util.h"
#include "base/singleton.h"
#include "base/synchronization/lock.h"
#include "content/common/notification_observer.h"
#include "content/common/notification_registrar.h"

// On OS X, the mach_port_t of a process is required to collect metrics about
// the process. Running |task_for_pid()| is only allowed for privileged code.
// However, a process has port rights to all its subprocesses, so let the
// browser's child processes send their Mach port to the browser over IPC.
// This way, the brower can at least collect metrics of its child processes,
// which is what it's most interested in anyway.
//
// Mach ports can only be sent over Mach IPC, not over the |socketpair()| that
// the regular IPC system uses. Hence, the child processes open a Mach
// connection shortly after launching and ipc their mach data to the browser
// process. This data is kept in a global |MachBroker| object.
//
// Since this data arrives over a separate channel, it is not available
// immediately after a child process has been started.
class MachBroker : public base::ProcessMetrics::PortProvider,
                   public NotificationObserver {
 public:
  // Returns the global MachBroker.
  static MachBroker* GetInstance();

  // Performs any necessary setup that cannot happen in the constructor.
  // Clients MUST call this method before fork()ing any children.
  void PrepareForFork();

  struct MachInfo {
    MachInfo() : mach_task_(MACH_PORT_NULL) {}

    MachInfo& SetTask(mach_port_t task) {
      mach_task_ = task;
      return *this;
    }

    mach_port_t mach_task_;
  };

  // Adds a placeholder to the map for the given pid with MACH_PORT_NULL.
  // Callers are expected to later update the port with FinalizePid().  Callers
  // MUST acquire the lock given by GetLock() before calling this method (and
  // release the lock afterwards).
  void AddPlaceholderForPid(base::ProcessHandle pid);

  // Updates the mapping for |pid| to include the given |mach_info|.  Does
  // nothing if PlaceholderForPid() has not already been called for the given
  // |pid|.  Callers MUST acquire the lock given by GetLock() before calling
  // this method (and release the lock afterwards).
  void FinalizePid(base::ProcessHandle pid, const MachInfo& mach_info);

  // Removes all mappings belonging to |pid| from the broker.
  void InvalidatePid(base::ProcessHandle pid);

  // The lock that protects this MachBroker object.  Clients MUST acquire and
  // release this lock around calls to PlaceholderForPid() and FinalizePid().
  base::Lock& GetLock();

  // Returns the Mach port name to use when sending or receiving messages.
  // Does the Right Thing in the browser and in child processes.
  static std::string GetMachPortName();

  // Implement |ProcessMetrics::PortProvider|.
  virtual mach_port_t TaskForPid(base::ProcessHandle process) const;

  // Implement |NotificationObserver|.
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);
 private:
  // Private constructor.
  MachBroker();
  ~MachBroker();

  // True if the listener thread has been started.
  bool listener_thread_started_;

  // Used to register for notifications received by NotificationObserver.
  // Accessed only on the UI thread.
  NotificationRegistrar registrar_;

  // Stores mach info for every process in the broker.
  typedef std::map<base::ProcessHandle, MachInfo> MachMap;
  MachMap mach_map_;

  // Mutex that guards |mach_map_|.
  mutable base::Lock lock_;

  friend class MachBrokerTest;
  friend class RegisterNotificationTask;
  // Needed in order to make the constructor private.
  friend struct DefaultSingletonTraits<MachBroker>;
  DISALLOW_COPY_AND_ASSIGN(MachBroker);
};

#endif  // CHROME_BROWSER_MACH_BROKER_H_

