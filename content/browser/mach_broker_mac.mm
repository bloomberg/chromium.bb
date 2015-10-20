// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/mach_broker_mac.h"

#include <bsm/libbsm.h>
#include <servers/bootstrap.h>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/mac/foundation_util.h"
#include "base/mac/mach_logging.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_data.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/common/content_switches.h"

namespace content {

namespace {

// Mach message structure used in the child as a sending message.
struct MachBroker_ChildSendMsg {
  mach_msg_header_t header;
  mach_msg_body_t body;
  mach_msg_port_descriptor_t child_task_port;
};

// Complement to the ChildSendMsg, this is used in the parent for receiving
// a message. Contains a message trailer with audit information.
struct MachBroker_ParentRecvMsg : public MachBroker_ChildSendMsg {
  mach_msg_audit_trailer_t trailer;
};

}  // namespace

bool MachBroker::ChildSendTaskPortToParent() {
  // Look up the named MachBroker port that's been registered with the
  // bootstrap server.
  mach_port_t parent_port;
  kern_return_t kr = bootstrap_look_up(bootstrap_port,
      const_cast<char*>(GetMachPortName().c_str()), &parent_port);
  if (kr != KERN_SUCCESS) {
    BOOTSTRAP_LOG(ERROR, kr) << "bootstrap_look_up";
    return false;
  }
  base::mac::ScopedMachSendRight scoped_right(parent_port);

  // Create the check in message. This will copy a send right on this process'
  // (the child's) task port and send it to the parent.
  MachBroker_ChildSendMsg msg;
  bzero(&msg, sizeof(msg));
  msg.header.msgh_bits = MACH_MSGH_BITS_REMOTE(MACH_MSG_TYPE_COPY_SEND) |
                         MACH_MSGH_BITS_COMPLEX;
  msg.header.msgh_remote_port = parent_port;
  msg.header.msgh_size = sizeof(msg);
  msg.body.msgh_descriptor_count = 1;
  msg.child_task_port.name = mach_task_self();
  msg.child_task_port.disposition = MACH_MSG_TYPE_PORT_SEND;
  msg.child_task_port.type = MACH_MSG_PORT_DESCRIPTOR;

  kr = mach_msg(&msg.header, MACH_SEND_MSG | MACH_SEND_TIMEOUT, sizeof(msg),
      0, MACH_PORT_NULL, 100 /*milliseconds*/, MACH_PORT_NULL);
  if (kr != KERN_SUCCESS) {
    MACH_LOG(ERROR, kr) << "mach_msg";
    return false;
  }

  return true;
}

MachBroker* MachBroker::GetInstance() {
  return base::Singleton<MachBroker,
                         base::LeakySingletonTraits<MachBroker>>::get();
}

base::Lock& MachBroker::GetLock() {
  return lock_;
}

void MachBroker::EnsureRunning() {
  lock_.AssertAcquired();

  if (initialized_)
    return;

  // Do not attempt to reinitialize in the event of failure.
  initialized_ = true;

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&MachBroker::RegisterNotifications, base::Unretained(this)));

  if (!Init()) {
    LOG(ERROR) << "Failed to initialize the MachListenerThreadDelegate";
  }
}

void MachBroker::AddPlaceholderForPid(base::ProcessHandle pid,
                                      int child_process_id) {
  lock_.AssertAcquired();

  DCHECK_EQ(0u, mach_map_.count(pid));
  mach_map_[pid] = MACH_PORT_NULL;
  child_process_id_map_[child_process_id] = pid;
}

mach_port_t MachBroker::TaskForPid(base::ProcessHandle pid) const {
  base::AutoLock lock(lock_);
  MachBroker::MachMap::const_iterator it = mach_map_.find(pid);
  if (it == mach_map_.end())
    return MACH_PORT_NULL;
  return it->second;
}

void MachBroker::BrowserChildProcessHostDisconnected(
    const ChildProcessData& data) {
  InvalidateChildProcessId(data.id);
}

void MachBroker::BrowserChildProcessCrashed(const ChildProcessData& data,
    int exit_code) {
  InvalidateChildProcessId(data.id);
}

void MachBroker::Observe(int type,
                         const NotificationSource& source,
                         const NotificationDetails& details) {
  switch (type) {
    case NOTIFICATION_RENDERER_PROCESS_TERMINATED:
    case NOTIFICATION_RENDERER_PROCESS_CLOSED: {
      RenderProcessHost* host = Source<RenderProcessHost>(source).ptr();
      InvalidateChildProcessId(host->GetID());
      break;
    }
    default:
      NOTREACHED() << "Unexpected notification";
      break;
  }
}

// static
std::string MachBroker::GetMachPortName() {
  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();
  const bool is_child = command_line->HasSwitch(switches::kProcessType);

  // In non-browser (child) processes, use the parent's pid.
  const pid_t pid = is_child ? getppid() : getpid();
  return base::StringPrintf("%s.rohitfork.%d", base::mac::BaseBundleID(), pid);
}

MachBroker::MachBroker() : initialized_(false) {
}

MachBroker::~MachBroker() {}

bool MachBroker::Init() {
  DCHECK(server_port_.get() == MACH_PORT_NULL);

  // Check in with launchd and publish the service name.
  mach_port_t port;
  kern_return_t kr =
      bootstrap_check_in(bootstrap_port, GetMachPortName().c_str(), &port);
  if (kr != KERN_SUCCESS) {
    BOOTSTRAP_LOG(ERROR, kr) << "bootstrap_check_in";
    return false;
  }
  server_port_.reset(port);

  // Start the dispatch source.
  std::string queue_name =
      base::StringPrintf("%s.MachBroker", base::mac::BaseBundleID());
  dispatch_source_.reset(new base::DispatchSourceMach(
      queue_name.c_str(), server_port_.get(), ^{ HandleRequest(); }));
  dispatch_source_->Resume();

  return true;
}

void MachBroker::HandleRequest() {
  MachBroker_ParentRecvMsg msg;
  bzero(&msg, sizeof(msg));
  msg.header.msgh_size = sizeof(msg);
  msg.header.msgh_local_port = server_port_.get();

  const mach_msg_option_t options = MACH_RCV_MSG |
      MACH_RCV_TRAILER_TYPE(MACH_RCV_TRAILER_AUDIT) |
      MACH_RCV_TRAILER_ELEMENTS(MACH_RCV_TRAILER_AUDIT);

  kern_return_t kr = mach_msg(&msg.header,
                              options,
                              0,
                              sizeof(msg),
                              server_port_.get(),
                              MACH_MSG_TIMEOUT_NONE,
                              MACH_PORT_NULL);
  if (kr != KERN_SUCCESS) {
    MACH_LOG(ERROR, kr) << "mach_msg";
    return;
  }

  // Use the kernel audit information to make sure this message is from
  // a task that this process spawned. The kernel audit token contains the
  // unspoofable pid of the task that sent the message.
  //
  // TODO(rsesek): In the 10.7 SDK, there's audit_token_to_pid().
  pid_t child_pid;
  audit_token_to_au32(msg.trailer.msgh_audit,
      NULL, NULL, NULL, NULL, NULL, &child_pid, NULL, NULL);

  mach_port_t child_task_port = msg.child_task_port.name;

  // Take the lock and update the broker information.
  base::AutoLock lock(GetLock());
  FinalizePid(child_pid, child_task_port);
}

void MachBroker::FinalizePid(base::ProcessHandle pid,
                             mach_port_t task_port) {
  lock_.AssertAcquired();

  MachMap::iterator it = mach_map_.find(pid);
  if (it == mach_map_.end()) {
    // Do nothing for unknown pids.
    LOG(ERROR) << "Unknown process " << pid << " is sending Mach IPC messages!";
    return;
  }

  DCHECK(it->second == MACH_PORT_NULL);
  if (it->second == MACH_PORT_NULL)
    it->second = task_port;
}

void MachBroker::InvalidateChildProcessId(int child_process_id) {
  base::AutoLock lock(lock_);
  MachBroker::ChildProcessIdMap::iterator it =
      child_process_id_map_.find(child_process_id);
  if (it == child_process_id_map_.end())
    return;

  MachMap::iterator mach_it = mach_map_.find(it->second);
  if (mach_it != mach_map_.end()) {
    kern_return_t kr = mach_port_deallocate(mach_task_self(),
                                            mach_it->second);
    MACH_LOG_IF(WARNING, kr != KERN_SUCCESS, kr) << "mach_port_deallocate";
    mach_map_.erase(mach_it);
  }
  child_process_id_map_.erase(it);
}

void MachBroker::RegisterNotifications() {
  registrar_.Add(this, NOTIFICATION_RENDERER_PROCESS_CLOSED,
                 NotificationService::AllBrowserContextsAndSources());
  registrar_.Add(this, NOTIFICATION_RENDERER_PROCESS_TERMINATED,
                 NotificationService::AllBrowserContextsAndSources());

  // No corresponding StopObservingBrowserChildProcesses,
  // we leak this singleton.
  BrowserChildProcessObserver::Add(this);
}

}  // namespace content
