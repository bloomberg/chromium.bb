// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MACH_BROKER_H_
#define CHROME_BROWSER_MACH_BROKER_H_

#include <map>

#include <mach/mach.h>

#include "base/lock.h"
#include "base/process.h"
#include "base/process_util.h"
#include "base/singleton.h"
#include "chrome/common/notification_registrar.h"

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
  static MachBroker* instance();

  struct MachInfo {
    MachInfo() : mach_task_(MACH_PORT_NULL) {}

    MachInfo& SetTask(mach_port_t task) {
      mach_task_ = task;
      return *this;
    }

    mach_port_t mach_task_;
  };

  // Adds mach info for a given pid.
  void RegisterPid(base::ProcessHandle pid, const MachInfo& mach_info);

  // Removes all mappings belonging to |pid| from the broker.
  void Invalidate(base::ProcessHandle pid);

  // Implement |ProcessMetrics::PortProvider|.
  virtual mach_port_t TaskForPid(base::ProcessHandle process) const;

  // Implement |NotificationObserver|.
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);
 private:
  // Private constructor.
  MachBroker();

  // Used to register for notifications received by NotificationObserver.
  // Accessed only on the UI thread.
  NotificationRegistrar registrar_;

  friend struct DefaultSingletonTraits<MachBroker>;
  friend class MachBrokerTest;

  // Stores mach info for every process in the broker.
  typedef std::map<base::ProcessHandle, MachInfo> MachMap;
  MachMap mach_map_;

  // Mutex that guards |mach_map_|.
  mutable Lock lock_;

  friend class RegisterNotificationTask;
  DISALLOW_COPY_AND_ASSIGN(MachBroker);
};

#endif  // CHROME_BROWSER_MACH_BROKER_H_

