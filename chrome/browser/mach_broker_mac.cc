// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/mach_broker_mac.h"

#include "base/logging.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/extensions/extension_host.h"
#include "chrome/browser/renderer_host/render_process_host.h"
#include "chrome/common/child_process_info.h"
#include "chrome/common/notification_service.h"

// Required because notifications happen on the UI thread.
class RegisterNotificationTask : public Task {
 public:
  RegisterNotificationTask(
      MachBroker* broker)
      : broker_(broker) { }

  virtual void Run() {
    broker_->registrar_.Add(broker_,
        NotificationType::RENDERER_PROCESS_CLOSED,
        NotificationService::AllSources());
    broker_->registrar_.Add(broker_,
        NotificationType::RENDERER_PROCESS_TERMINATED,
        NotificationService::AllSources());
    broker_->registrar_.Add(broker_,
        NotificationType::CHILD_PROCESS_CRASHED,
        NotificationService::AllSources());
    broker_->registrar_.Add(broker_,
        NotificationType::CHILD_PROCESS_HOST_DISCONNECTED,
        NotificationService::AllSources());
    broker_->registrar_.Add(broker_,
        NotificationType::EXTENSION_PROCESS_TERMINATED,
        NotificationService::AllSources());
  }

 private:
  MachBroker* broker_;
};

MachBroker::MachBroker() {
  ChromeThread::PostTask(
      ChromeThread::UI, FROM_HERE, new RegisterNotificationTask(this));
}

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
  MachBroker::MachMap::iterator it = mach_map_.find(pid);
  if (it == mach_map_.end())
    return;

  kern_return_t kr = mach_port_deallocate(mach_task_self(),
                                          it->second.mach_task_);
  LOG_IF(WARNING, kr != KERN_SUCCESS)
     << "Failed to mach_port_deallocate mach task " << it->second.mach_task_
     << ", error " << kr;
  mach_map_.erase(it);
}

// Returns the mach task belonging to |pid|.
mach_port_t MachBroker::TaskForPid(base::ProcessHandle pid) const {
  AutoLock lock(lock_);
  MachBroker::MachMap::const_iterator it = mach_map_.find(pid);
  if (it == mach_map_.end())
    return MACH_PORT_NULL;
  return it->second.mach_task_;
}

void MachBroker::Observe(NotificationType type,
                         const NotificationSource& source,
                         const NotificationDetails& details) {
  base::ProcessHandle handle = 0;
  switch (type.value) {
    case NotificationType::RENDERER_PROCESS_CLOSED:
    case NotificationType::RENDERER_PROCESS_TERMINATED:
      handle = Source<RenderProcessHost>(source)->GetHandle();
      break;
    case NotificationType::EXTENSION_PROCESS_TERMINATED:
      handle =
          Details<ExtensionHost>(details)->render_process_host()->GetHandle();
      break;
    case NotificationType::CHILD_PROCESS_CRASHED:
    case NotificationType::CHILD_PROCESS_HOST_DISCONNECTED:
      handle = Details<ChildProcessInfo>(details)->handle();
      break;
    default:
      NOTREACHED() << "Unexpected notification";
      break;
  }
  Invalidate(handle);
}
