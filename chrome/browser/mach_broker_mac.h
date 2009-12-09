// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MACH_BROKER_H_
#define CHROME_BROWSER_MACH_BROKER_H_

#include <map>

#include <mach/mach.h>

#include "base/lock.h"
#include "base/process.h"
#include "base/singleton.h"

// On OS X, the mach_port_t of a process is required to collect metrics about
// the process. Running |task_for_pid()| is only allowed for priviledged code.
// However, a process has port rights to all its subprocesses, so let the
// browser's child processes send their mach_port_t to the browser over IPC.
// This way, the brower can at least collect metrics of its child processes,
// which is what it's most interested in anyway.
//
// mach_port_ts can only be sent over mach ipc, not over the socketpair that
// the regular ipc system uses. Hence, the child processes open a mach
// connection shortly after launching and ipc their mach data to the browser
// process. This data is kept in a global |MachBroker| object.
//
// Due to this data arriving over a separate channel, it is not available
// immediately after a child process has been started.
class MachBroker {
 public:
  // Returns the global MachBroker.
  static MachBroker* instance();

  // Returns the mach task belonging to |pid|, or 0 if no mach task is
  // registered for |pid|.
  mach_port_t MachTaskForPid(base::ProcessHandle pid) const;

  // Returns the mach host belonging to |pid|, or 0 if no mach task is
  // registered for |pid|.
  mach_port_t MachHostForPid(base::ProcessHandle pid) const;

  struct MachInfo {
    MachInfo() : mach_task_(0), mach_host_(0) {}

    MachInfo& SetTask(mach_port_t task) {
      mach_task_ = task;
      return *this;
    }

    MachInfo& SetHost(mach_port_t host) {
      mach_host_ = host;
      return *this;
    }

    mach_port_t mach_task_;
    mach_port_t mach_host_;
  };

  // Adds mach info for a given pid.
  void RegisterPid(base::ProcessHandle pid, const MachInfo& mach_info);

  // Removes all mappings belonging to |pid| from the broker.
  void Invalidate(base::ProcessHandle pid);

 private:
  // Private constructor.
  MachBroker();
  friend struct DefaultSingletonTraits<MachBroker>;
  friend class MachBrokerTest;

  // Stores
  typedef std::map<base::ProcessHandle, MachInfo> MachMap;
  MachMap mach_map_;

  // Mutex that guards |mach_map_|.
  mutable Lock lock_;
};

#endif  // CHROME_BROWSER_MACH_BROKER_H_

