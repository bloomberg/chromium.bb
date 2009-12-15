// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/notifier/base/mac/network_status_detector_task_mac.h"

#include <SystemConfiguration/SCNetworkReachability.h>

#include "base/logging.h"
#include "base/scoped_cftyperef.h"
#include "base/scoped_ptr.h"
#include "base/string_util.h"
#include "base/sys_string_conversions.h"
#include "talk/base/physicalsocketserver.h"
#include "talk/base/socket.h"
#include "talk/base/thread.h"

namespace notifier {

NetworkStatusDetectorTaskMac::WorkerInfo::WorkerInfo(
    PlatformThreadId thread_id)
    : thread_state(WORKER_THREAD_STOPPED),
      thread_id(thread_id),
      thread_run_loop(NULL) {}

NetworkStatusDetectorTaskMac::WorkerInfo::WorkerInfo(
    WorkerThreadState thread_state,
    PlatformThreadId thread_id,
    CFRunLoopRef thread_run_loop)
    : thread_state(thread_state),
      thread_id(thread_id),
      thread_run_loop(thread_run_loop) {
  DCHECK_EQ(thread_state == WORKER_THREAD_RUNNING, thread_run_loop != NULL);
}

NetworkStatusDetectorTaskMac::NetworkStatusDetectorTaskMac(
    talk_base::Task* parent)
    : NetworkStatusDetectorTask(parent),
      parent_thread_id_(PlatformThread::CurrentId()),
      parent_thread_(talk_base::Thread::Current()),
      worker_thread_(kNullThreadHandle),
      worker_thread_not_stopped_(&worker_lock_),
      worker_shared_info_(parent_thread_id_) {
  DCHECK(parent_thread_);
  DCHECK(IsOnParentThread());
}

NetworkStatusDetectorTaskMac::~NetworkStatusDetectorTaskMac() {
  ClearWorker();
}

void NetworkStatusDetectorTaskMac::ClearWorker() {
  DCHECK(IsOnParentThread());
  // Sadly, there's no Lock::AssertNotAcquired().
  WorkerThreadState worker_thread_state;
  CFRunLoopRef worker_thread_run_loop;
  {
    AutoLock auto_lock(worker_lock_);
    worker_thread_state = worker_shared_info_.thread_state;
    worker_thread_run_loop = worker_shared_info_.thread_run_loop;
  }
  if (worker_thread_state == WORKER_THREAD_RUNNING) {
    CFRunLoopStop(worker_thread_run_loop);
  }
  if (worker_thread_ != kNullThreadHandle) {
    DCHECK_NE(worker_thread_state, WORKER_THREAD_STOPPED);
    PlatformThread::Join(worker_thread_);
  }

  worker_thread_ = kNullThreadHandle;
  worker_shared_info_ = WorkerInfo(parent_thread_id_);
}

bool NetworkStatusDetectorTaskMac::IsOnParentThread() const {
  return PlatformThread::CurrentId() == parent_thread_id_;
}

bool NetworkStatusDetectorTaskMac::IsOnWorkerThread() {
  PlatformThreadId current_thread_id = PlatformThread::CurrentId();
  AutoLock auto_lock(worker_lock_);
  return
      (worker_shared_info_.thread_id != parent_thread_id_) &&
      (current_thread_id == worker_shared_info_.thread_id);
}

int NetworkStatusDetectorTaskMac::ProcessStart() {
  DCHECK(IsOnParentThread());
  if (logging::DEBUG_MODE) {
    AutoLock auto_lock(worker_lock_);
    DCHECK_EQ(worker_shared_info_.thread_state, WORKER_THREAD_STOPPED);
    DCHECK(!worker_shared_info_.thread_run_loop);
    DCHECK_EQ(worker_shared_info_.thread_id, parent_thread_id_);
  }

  if (!PlatformThread::Create(0, this, &worker_thread_)) {
    LOG(WARNING) << "Could not create network reachability thread";
    ClearWorker();
    return STATE_ERROR;
  }

  // Wait for the just-created worker thread to start up and
  // initialize itself.
  WorkerThreadState worker_thread_state;
  {
    AutoLock auto_lock(worker_lock_);
    while (worker_shared_info_.thread_state == WORKER_THREAD_STOPPED) {
      worker_thread_not_stopped_.Wait();
    }
    worker_thread_state = worker_shared_info_.thread_state;
  }

  if (worker_thread_state == WORKER_THREAD_ERROR) {
    ClearWorker();
    return STATE_ERROR;
  }

  if (logging::DEBUG_MODE) {
    AutoLock auto_lock(worker_lock_);
    DCHECK_EQ(worker_shared_info_.thread_state, WORKER_THREAD_RUNNING);
    DCHECK(worker_shared_info_.thread_run_loop);
    DCHECK_NE(worker_shared_info_.thread_id, parent_thread_id_);
  }

  return STATE_RESPONSE;
}

void NetworkStatusDetectorTaskMac::Stop() {
  ClearWorker();
  NetworkStatusDetectorTask::Stop();
}

void NetworkStatusDetectorTaskMac::OnMessage(talk_base::Message* message) {
  DCHECK(IsOnParentThread());
  bool alive = message->message_id;
  SetNetworkAlive(alive);
}

NetworkStatusDetectorTask* NetworkStatusDetectorTask::Create(
    talk_base::Task* parent) {
  return new NetworkStatusDetectorTaskMac(parent);
}

// Everything below is run in the worker thread.

namespace {

// TODO(akalin): Use these constants across all platform
// implementations.
const char kTalkHost[] = "talk.google.com";
const int kTalkPort = 5222;

CFStringRef NetworkReachabilityCopyDescription(const void *info) {
  return base::SysUTF8ToCFStringRef(
      StringPrintf("NetworkStatusDetectorTaskMac(0x%p)", info));
}

void NetworkReachabilityChangedCallback(SCNetworkReachabilityRef target,
                                        SCNetworkConnectionFlags flags,
                                        void* info) {
  bool network_active = ((flags & (kSCNetworkFlagsReachable |
                                   kSCNetworkFlagsConnectionRequired |
                                   kSCNetworkFlagsConnectionAutomatic |
                                   kSCNetworkFlagsInterventionRequired)) ==
                         kSCNetworkFlagsReachable);
  NetworkStatusDetectorTaskMac* network_status_detector_task_mac =
      static_cast<NetworkStatusDetectorTaskMac*>(info);
  network_status_detector_task_mac->NetworkReachabilityChanged(
      network_active);
}


SCNetworkReachabilityRef CreateAndScheduleNetworkReachability(
    SCNetworkReachabilityContext* network_reachability_context) {
  scoped_cftyperef<SCNetworkReachabilityRef> network_reachability(
      SCNetworkReachabilityCreateWithName(kCFAllocatorDefault, kTalkHost));
  if (!network_reachability.get()) {
    LOG(WARNING) << "Could not create network reachability object";
    return NULL;
  }

  if (!SCNetworkReachabilitySetCallback(network_reachability.get(),
                                        &NetworkReachabilityChangedCallback,
                                        network_reachability_context)) {
    LOG(WARNING) << "Could not set network reachability callback";
    return NULL;
  }

  if (!SCNetworkReachabilityScheduleWithRunLoop(network_reachability.get(),
                                                CFRunLoopGetCurrent(),
                                                kCFRunLoopDefaultMode)) {
    LOG(WARNING) << "Could not schedule network reachability with run loop";
    return NULL;
  }

  return network_reachability.release();
}

}  // namespace

void NetworkStatusDetectorTaskMac::ThreadMain() {
  DCHECK(!IsOnParentThread());
  PlatformThread::SetName("NetworkStatusDetectorTaskMac worker thread");

  SCNetworkReachabilityContext network_reachability_context;
  network_reachability_context.version = 0;
  network_reachability_context.info = static_cast<void *>(this);
  network_reachability_context.retain = NULL;
  network_reachability_context.release = NULL;
  network_reachability_context.copyDescription =
      &NetworkReachabilityCopyDescription;

  PlatformThreadId worker_thread_id = PlatformThread::CurrentId();

  scoped_cftyperef<SCNetworkReachabilityRef> network_reachability(
      CreateAndScheduleNetworkReachability(&network_reachability_context));
  if (!network_reachability.get()) {
    {
      AutoLock auto_lock(worker_lock_);
      worker_shared_info_ =
          WorkerInfo(WORKER_THREAD_ERROR, worker_thread_id, NULL);
    }
    worker_thread_not_stopped_.Signal();
    return;
  }

  CFRunLoopRef run_loop = CFRunLoopGetCurrent();
  {
    AutoLock auto_lock(worker_lock_);
    worker_shared_info_ =
        WorkerInfo(WORKER_THREAD_RUNNING, worker_thread_id, run_loop);
  }
  worker_thread_not_stopped_.Signal();

  DCHECK(IsOnWorkerThread());
  CFRunLoopRun();

  // We reach here only when our run loop is stopped (usually by the
  // parent thread).  The parent thread is responsible for resetting
  // worker_thread_shared_info_, et al. to appropriate values.
}

void NetworkStatusDetectorTaskMac::NetworkReachabilityChanged(
    bool network_active) {
  DCHECK(IsOnWorkerThread());

  bool alive = network_active;
  if (alive) {
    talk_base::PhysicalSocketServer physical;
    scoped_ptr<talk_base::Socket> socket(physical.CreateSocket(SOCK_STREAM));
    alive =
      (socket->Connect(talk_base::SocketAddress(kTalkHost, kTalkPort)) == 0);
    LOG(INFO) << "network is " << (alive ? "alive" : "not alive")
              << " based on connecting to " << kTalkHost << ":" << kTalkPort;
  } else {
    LOG(INFO) << "network is not alive";
  }

  parent_thread_->Send(this, alive);
}

}  // namespace notifier

