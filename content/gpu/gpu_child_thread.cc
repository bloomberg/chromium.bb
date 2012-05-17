// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/gpu/gpu_child_thread.h"

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/threading/worker_pool.h"
#include "build/build_config.h"
#include "content/common/child_process.h"
#include "content/common/gpu/gpu_messages.h"
#include "content/gpu/gpu_info_collector.h"
#include "content/gpu/gpu_watchdog_thread.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "ipc/ipc_channel_handle.h"
#include "ipc/ipc_sync_message_filter.h"
#include "ui/gl/gl_implementation.h"

const int kGpuTimeout = 10000;

namespace {

bool GpuProcessLogMessageHandler(int severity,
                                 const char* file, int line,
                                 size_t message_start,
                                 const std::string& str) {
  std::string header = str.substr(0, message_start);
  std::string message = str.substr(message_start);

  // If we are not on main thread in child process, send through
  // the sync_message_filter; otherwise send directly.
  if (MessageLoop::current() !=
      ChildProcess::current()->main_thread()->message_loop()) {
    ChildProcess::current()->main_thread()->sync_message_filter()->Send(
        new GpuHostMsg_OnLogMessage(severity, header, message));
  } else {
    ChildThread::current()->Send(new GpuHostMsg_OnLogMessage(severity, header,
        message));
  }

  return false;
}

}  // namespace

GpuChildThread::GpuChildThread(bool dead_on_arrival,
                               const content::GPUInfo& gpu_info)
    : dead_on_arrival_(dead_on_arrival),
      gpu_info_(gpu_info) {
#if defined(OS_WIN)
  target_services_ = NULL;
  collecting_dx_diagnostics_ = false;
#endif
}

GpuChildThread::GpuChildThread(const std::string& channel_id)
    : ChildThread(channel_id),
      dead_on_arrival_(false) {
#if defined(OS_WIN)
  target_services_ = NULL;
  collecting_dx_diagnostics_ = false;
#endif
}


GpuChildThread::~GpuChildThread() {
  logging::SetLogMessageHandler(NULL);
}

void GpuChildThread::Init(const base::Time& process_start_time) {
  process_start_time_ = process_start_time;
}

bool GpuChildThread::Send(IPC::Message* msg) {
  // The GPU process must never send a synchronous IPC message to the browser
  // process. This could result in deadlock.
  DCHECK(!msg->is_sync());

  return ChildThread::Send(msg);
}

bool GpuChildThread::OnControlMessageReceived(const IPC::Message& msg) {
  bool msg_is_ok = true;
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP_EX(GpuChildThread, msg, msg_is_ok)
    IPC_MESSAGE_HANDLER(GpuMsg_Initialize, OnInitialize)
    IPC_MESSAGE_HANDLER(GpuMsg_CollectGraphicsInfo, OnCollectGraphicsInfo)
    IPC_MESSAGE_HANDLER(GpuMsg_Clean, OnClean)
    IPC_MESSAGE_HANDLER(GpuMsg_Crash, OnCrash)
    IPC_MESSAGE_HANDLER(GpuMsg_Hang, OnHang)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP_EX()

  if (handled)
    return true;

  return gpu_channel_manager_.get() &&
      gpu_channel_manager_->OnMessageReceived(msg);
}

void GpuChildThread::OnInitialize() {
  if (dead_on_arrival_) {
    VLOG(1) << "Exiting GPU process due to errors during initialization";
    MessageLoop::current()->Quit();
    return;
  }

  // We don't need to pipe log messages if we are running the GPU thread in
  // the browser process.
  if (!CommandLine::ForCurrentProcess()->HasSwitch(switches::kSingleProcess) &&
      !CommandLine::ForCurrentProcess()->HasSwitch(switches::kInProcessGPU))
    logging::SetLogMessageHandler(GpuProcessLogMessageHandler);

  // Record initialization only after collecting the GPU info because that can
  // take a significant amount of time.
  gpu_info_.initialization_time = base::Time::Now() - process_start_time_;

  // In addition to disabling the watchdog if the command line switch is
  // present, disable it in two other cases. OSMesa is expected to run very
  // slowly.  Also disable the watchdog on valgrind because the code is expected
  // to run slowly in that case.
  bool enable_watchdog =
      !CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableGpuWatchdog) &&
      gfx::GetGLImplementation() != gfx::kGLImplementationOSMesaGL &&
      !RunningOnValgrind();

  // Disable the watchdog in debug builds because they tend to only be run by
  // developers who will not appreciate the watchdog killing the GPU process.
#ifndef NDEBUG
  enable_watchdog = false;
#endif

  // Disable the watchdog for Windows. It tends to abort when the GPU process
  // is not hung but still taking a long time to do something. Instead, the
  // browser process displays a dialog when it notices that the child window
  // is hung giving the user an opportunity to terminate it. This is the
  // same mechanism used to abort hung plugins.
#if defined(OS_WIN)
  enable_watchdog = false;
#endif

  // Start the GPU watchdog only after anything that is expected to be time
  // consuming has completed, otherwise the process is liable to be aborted.
  if (enable_watchdog) {
    watchdog_thread_ = new GpuWatchdogThread(kGpuTimeout);
    watchdog_thread_->Start();
  }

  // Defer creation of the render thread. This is to prevent it from handling
  // IPC messages before the sandbox has been enabled and all other necessary
  // initialization has succeeded.
  gpu_channel_manager_.reset(new GpuChannelManager(
      this,
      watchdog_thread_,
      ChildProcess::current()->io_message_loop_proxy(),
      ChildProcess::current()->GetShutDownEvent()));

#if defined(OS_LINUX)
  // Ensure the browser process receives the GPU info before a reply to any
  // subsequent IPC it might send.
  Send(new GpuHostMsg_GraphicsInfoCollected(gpu_info_));
#endif
}

void GpuChildThread::StopWatchdog() {
  if (watchdog_thread_.get()) {
    watchdog_thread_->Stop();
  }
}

void GpuChildThread::OnCollectGraphicsInfo() {
  if (!gpu_info_.finalized &&
      CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableGpuSandbox)) {
    // GPU full info collection should only happen on un-sandboxed GPU process.

    if (!gpu_info_collector::CollectGraphicsInfo(&gpu_info_))
      VLOG(1) << "gpu_info_collector::CollectGraphicsInfo failed";
    content::GetContentClient()->SetGpuInfo(gpu_info_);

#if defined(OS_WIN)
    if (!collecting_dx_diagnostics_) {
      // Prevent concurrent collection of DirectX diagnostics.
      collecting_dx_diagnostics_ = true;

      // Asynchronously collect the DirectX diagnostics because this can take a
      // couple of seconds.
      if (!base::WorkerPool::PostTask(
          FROM_HERE, base::Bind(&GpuChildThread::CollectDxDiagnostics, this),
          true)) {
        // Flag GPU info as complete if the DirectX diagnostics cannot be
        // collected.
        collecting_dx_diagnostics_ = false;
        gpu_info_.finalized = true;
      }
    }
#endif
  }
  Send(new GpuHostMsg_GraphicsInfoCollected(gpu_info_));
}

void GpuChildThread::OnClean() {
  VLOG(1) << "GPU: Removing all contexts";
  if (gpu_channel_manager_.get())
    gpu_channel_manager_->LoseAllContexts();
}

void GpuChildThread::OnCrash() {
  VLOG(1) << "GPU: Simulating GPU crash";
  // Good bye, cruel world.
  volatile int* it_s_the_end_of_the_world_as_we_know_it = NULL;
  *it_s_the_end_of_the_world_as_we_know_it = 0xdead;
}

void GpuChildThread::OnHang() {
  VLOG(1) << "GPU: Simulating GPU hang";
  for (;;) {
    // Do not sleep here. The GPU watchdog timer tracks the amount of user
    // time this thread is using and it doesn't use much while calling Sleep.
  }
}

#if defined(OS_WIN)

// Runs on a worker thread. The GPU process never terminates voluntarily so
// it is safe to assume that its message loop is valid.
void GpuChildThread::CollectDxDiagnostics(GpuChildThread* thread) {
  content::DxDiagNode node;
  gpu_info_collector::GetDxDiagnostics(&node);

  thread->message_loop()->PostTask(
      FROM_HERE, base::Bind(&GpuChildThread::SetDxDiagnostics, thread, node));
}

// Runs on the main thread.
void GpuChildThread::SetDxDiagnostics(GpuChildThread* thread,
                                      const content::DxDiagNode& node) {
  thread->gpu_info_.dx_diagnostics = node;
  thread->gpu_info_.finalized = true;
  thread->collecting_dx_diagnostics_ = false;
  thread->Send(new GpuHostMsg_GraphicsInfoCollected(thread->gpu_info_));
}

#endif
