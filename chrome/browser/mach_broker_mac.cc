// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/mach_broker_mac.h"

#include "base/logging.h"

// Returns the global MachBroker.
MachBroker* MachBroker::instance() {
  return Singleton<MachBroker>::get();
}

// Adds mach info for a given pid.
void MachBroker::RegisterPid(
    base::ProcessHandle pid, const MachInfo& mach_info) {
  AutoLock lock(lock_);
  DCHECK_EQ(0u, mach_map_.count(pid));
  mach_map_[pid] = mach_info;
}

// Removes all mappings belonging to |pid| from the broker.
void MachBroker::Invalidate(base::ProcessHandle pid) {
  AutoLock lock(lock_);
  mach_map_.erase(pid);
}

// Returns the mach task belonging to |pid|.
mach_port_t MachBroker::TaskForPid(base::ProcessHandle pid) const {
  AutoLock lock(lock_);
  MachBroker::MachMap::const_iterator it = mach_map_.find(pid);
  if (it == mach_map_.end())
    return MACH_PORT_NULL;
  return it->second.mach_task_;
}
