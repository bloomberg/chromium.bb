// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/gpu/gpu_child_thread.h"

#include <string>
#include <vector>

#include "app/win/scoped_com_initializer.h"
#include "base/command_line.h"
#include "base/threading/worker_pool.h"
#include "build/build_config.h"
#include "content/common/child_process.h"
#include "content/common/content_client.h"
#include "content/common/content_switches.h"
#include "content/common/gpu/gpu_messages.h"
#include "content/gpu/gpu_info_collector.h"
#include "content/gpu/gpu_watchdog_thread.h"
#include "ipc/ipc_channel_handle.h"
#include "ui/gfx/gl/gl_implementation.h"
#include "ui/gfx/gl/gl_surface.h"

#if defined(OS_MACOSX)
#include "content/common/sandbox_init_wrapper.h"
#include "content/common/sandbox_mac.h"
#elif defined(OS_WIN)
#include "sandbox/src/sandbox.h"
#endif

const int kGpuTimeout = 10000;

namespace {

bool InitializeGpuSandbox() {
#if defined(OS_MACOSX)
  CommandLine* parsed_command_line = CommandLine::ForCurrentProcess();
  SandboxInitWrapper sandbox_wrapper;
  return sandbox_wrapper.InitializeSandbox(*parsed_command_line,
                                           switches::kGpuProcess);
#else
  // TODO(port): Create GPU sandbox for linux.
  return true;
#endif
}

bool GpuProcessLogMessageHandler(int severity,
                                 const char* file, int line,
                                 size_t message_start,
                                 const std::string& str) {
  std::string header = str.substr(0, message_start);
  std::string message = str.substr(message_start);
  ChildThread::current()->Send(
      new GpuHostMsg_OnLogMessage(severity, header, message));
  return false;
}

}  // namespace

#if defined(OS_WIN)
GpuChildThread::GpuChildThread(sandbox::TargetServices* target_services)
    : target_services_(target_services),
      collecting_dx_diagnostics_(false) {
}
#else
GpuChildThread::GpuChildThread() {
}
#endif

GpuChildThread::GpuChildThread(const std::string& channel_id)
    : ChildThread(channel_id) {
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
  // process. This could result in deadlock. Unfortunately linux does this for
  // GpuHostMsg_ResizeXID. TODO(apatrick): fix this before issuing any GL calls
  // on the browser process' GPU thread.
#if !defined(OS_LINUX)
    DCHECK(!msg->is_sync());
#endif

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
  logging::SetLogMessageHandler(GpuProcessLogMessageHandler);

  // Load the GL implementation and locate the bindings before starting the GPU
  // watchdog because this can take a lot of time and the GPU watchdog might
  // terminate the GPU process.
  if (!gfx::GLSurface::InitializeOneOff()) {
    LOG(INFO) << "GLContext::InitializeOneOff failed";
    MessageLoop::current()->Quit();
    return;
  }
  gpu_info_collector::CollectGraphicsInfo(&gpu_info_);

  content::GetContentClient()->SetGpuInfo(gpu_info_);
  LOG(INFO) << "gpu_info_collector::CollectGraphicsInfo complete";

  // Record initialization only after collecting the GPU info because that can
  // take a significant amount of time.
  gpu_info_.initialization_time = base::Time::Now() - process_start_time_;

#if defined (OS_MACOSX)
  // Note that kNoSandbox will also disable the GPU sandbox.
  bool no_gpu_sandbox = CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kDisableGpuSandbox);
  if (!no_gpu_sandbox) {
    if (!InitializeGpuSandbox()) {
      LOG(ERROR) << "Failed to initialize the GPU sandbox";
      MessageLoop::current()->Quit();
      return;
    }
  } else {
    LOG(ERROR) << "Running without GPU sandbox";
  }
#elif defined(OS_WIN)
  // For windows, if the target_services interface is not zero, the process
  // is sandboxed and we must call LowerToken() before rendering untrusted
  // content.
  if (target_services_)
    target_services_->LowerToken();
#endif

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

  // Ensure the browser process receives the GPU info before a reply to any
  // subsequent IPC it might send.
  Send(new GpuHostMsg_GraphicsInfoCollected(gpu_info_));
}

void GpuChildThread::StopWatchdog() {
  if (watchdog_thread_.get()) {
    watchdog_thread_->Stop();
  }
}

void GpuChildThread::OnCollectGraphicsInfo() {
#if defined(OS_WIN)
  if (!gpu_info_.finalized && !collecting_dx_diagnostics_) {
    // Prevent concurrent collection of DirectX diagnostics.
    collecting_dx_diagnostics_ = true;

    // Asynchronously collect the DirectX diagnostics because this can take a
    // couple of seconds.
    if (!base::WorkerPool::PostTask(
        FROM_HERE,
        NewRunnableFunction(&GpuChildThread::CollectDxDiagnostics, this),
        true)) {
      // Flag GPU info as complete if the DirectX diagnostics cannot be
      // collected.
      collecting_dx_diagnostics_ = false;
      gpu_info_.finalized = true;
    } else {
      // Do not send response if we are still completing the GPUInfo struct
      return;
    }
  }
#endif
  Send(new GpuHostMsg_GraphicsInfoCollected(gpu_info_));
}

void GpuChildThread::OnClean() {
  LOG(INFO) << "GPU: Removing all contexts";
  if (gpu_channel_manager_.get())
    gpu_channel_manager_->LoseAllContexts();
}

void GpuChildThread::OnCrash() {
  LOG(INFO) << "GPU: Simulating GPU crash";
  // Good bye, cruel world.
  volatile int* it_s_the_end_of_the_world_as_we_know_it = NULL;
  *it_s_the_end_of_the_world_as_we_know_it = 0xdead;
}

void GpuChildThread::OnHang() {
  LOG(INFO) << "GPU: Simulating GPU hang";
  for (;;) {
    // Do not sleep here. The GPU watchdog timer tracks the amount of user
    // time this thread is using and it doesn't use much while calling Sleep.
  }
}

#if defined(OS_WIN)

// Runs on a worker thread. The GPU process never terminates voluntarily so
// it is safe to assume that its message loop is valid.
void GpuChildThread::CollectDxDiagnostics(GpuChildThread* thread) {
  app::win::ScopedCOMInitializer com_initializer;

  DxDiagNode node;
  gpu_info_collector::GetDxDiagnostics(&node);

  thread->message_loop()->PostTask(
      FROM_HERE,
      NewRunnableFunction(&GpuChildThread::SetDxDiagnostics, thread, node));
}

// Runs on the main thread.
void GpuChildThread::SetDxDiagnostics(GpuChildThread* thread,
                                      const DxDiagNode& node) {
  thread->gpu_info_.dx_diagnostics = node;
  thread->gpu_info_.finalized = true;
  thread->collecting_dx_diagnostics_ = false;
  thread->Send(new GpuHostMsg_GraphicsInfoCollected(thread->gpu_info_));
}

#endif
