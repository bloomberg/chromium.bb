// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/gpu/gpu_thread.h"

#include <string>
#include <vector>

#include "app/gfx/gl/gl_context.h"
#include "app/gfx/gl/gl_implementation.h"
#include "app/win/scoped_com_initializer.h"
#include "base/command_line.h"
#include "base/threading/worker_pool.h"
#include "build/build_config.h"
#include "chrome/common/child_process_logging.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/gpu_messages.h"
#include "chrome/gpu/gpu_info_collector.h"
#include "chrome/gpu/gpu_watchdog_thread.h"
#include "content/common/child_process.h"
#include "ipc/ipc_channel_handle.h"

#if defined(OS_MACOSX)
#include "chrome/common/sandbox_init_wrapper.h"
#include "chrome/common/sandbox_mac.h"
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

}  // namespace

#if defined(OS_WIN)
GpuThread::GpuThread(sandbox::TargetServices* target_services)
    : target_services_(target_services) {
}
#else
GpuThread::GpuThread() {
}
#endif

GpuThread::GpuThread(const std::string& channel_id)
    : ChildThread(channel_id) {
#if defined(OS_WIN)
  target_services_ = NULL;
#endif
}


GpuThread::~GpuThread() {
  logging::SetLogMessageHandler(NULL);
}

void GpuThread::Init(const base::Time& process_start_time) {
  process_start_time_ = process_start_time;
}

void GpuThread::RemoveChannel(int renderer_id) {
  gpu_channels_.erase(renderer_id);
}

bool GpuThread::OnControlMessageReceived(const IPC::Message& msg) {
  bool msg_is_ok = true;
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP_EX(GpuThread, msg, msg_is_ok)
    IPC_MESSAGE_HANDLER(GpuMsg_Initialize, OnInitialize)
    IPC_MESSAGE_HANDLER(GpuMsg_EstablishChannel, OnEstablishChannel)
    IPC_MESSAGE_HANDLER(GpuMsg_CloseChannel, OnCloseChannel)
    IPC_MESSAGE_HANDLER(GpuMsg_CreateViewCommandBuffer,
                        OnCreateViewCommandBuffer);
    IPC_MESSAGE_HANDLER(GpuMsg_Synchronize, OnSynchronize)
    IPC_MESSAGE_HANDLER(GpuMsg_CollectGraphicsInfo, OnCollectGraphicsInfo)
#if defined(OS_MACOSX)
    IPC_MESSAGE_HANDLER(GpuMsg_AcceleratedSurfaceBuffersSwappedACK,
                        OnAcceleratedSurfaceBuffersSwappedACK)
    IPC_MESSAGE_HANDLER(GpuMsg_DidDestroyAcceleratedSurface,
                        OnDidDestroyAcceleratedSurface)
#endif
    IPC_MESSAGE_HANDLER(GpuMsg_Crash, OnCrash)
    IPC_MESSAGE_HANDLER(GpuMsg_Hang, OnHang)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP_EX()
  return handled;
}

namespace {

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

void GpuThread::OnInitialize() {
  // Redirect LOG messages to the GpuProcessHost
  bool single_process = CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kSingleProcess);
  if (!single_process)
    logging::SetLogMessageHandler(GpuProcessLogMessageHandler);

  // Load the GL implementation and locate the bindings before starting the GPU
  // watchdog because this can take a lot of time and the GPU watchdog might
  // terminate the GPU process.
  if (!gfx::GLContext::InitializeOneOff()) {
    LOG(INFO) << "GLContext::InitializeOneOff failed";
    MessageLoop::current()->Quit();
    return;
  }
  bool gpu_info_result = gpu_info_collector::CollectGraphicsInfo(&gpu_info_);
  if (!gpu_info_result) {
    gpu_info_.SetCollectionError(true);
    LOG(ERROR) << "gpu_info_collector::CollectGraphicsInfo() failed";
  }
  child_process_logging::SetGpuInfo(gpu_info_);
  LOG(INFO) << "gpu_info_collector::CollectGraphicsInfo complete";

  // Record initialization only after collecting the GPU info because that can
  // take a significant amount of time.
  gpu_info_.SetInitializationTime(base::Time::Now() - process_start_time_);

#if defined (OS_MACOSX)
  // Note that kNoSandbox will also disable the GPU sandbox.
  bool no_gpu_sandbox = CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kNoGpuSandbox);
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
}

void GpuThread::StopWatchdog() {
  if (watchdog_thread_.get()) {
    watchdog_thread_->Stop();
  }
}

void GpuThread::OnEstablishChannel(int renderer_id) {
  scoped_refptr<GpuChannel> channel;
  IPC::ChannelHandle channel_handle;
  GPUInfo gpu_info;

  GpuChannelMap::const_iterator iter = gpu_channels_.find(renderer_id);
  if (iter == gpu_channels_.end())
    channel = new GpuChannel(this, renderer_id);
  else
    channel = iter->second;

  DCHECK(channel != NULL);

  if (channel->Init())
    gpu_channels_[renderer_id] = channel;
  else
    channel = NULL;

  if (channel.get()) {
    channel_handle.name = channel->GetChannelName();
#if defined(OS_POSIX)
    // On POSIX, pass the renderer-side FD. Also mark it as auto-close so
    // that it gets closed after it has been sent.
    int renderer_fd = channel->GetRendererFileDescriptor();
    channel_handle.socket = base::FileDescriptor(renderer_fd, false);
#endif
  }

  Send(new GpuHostMsg_ChannelEstablished(channel_handle, gpu_info_));
}

void GpuThread::OnCloseChannel(const IPC::ChannelHandle& channel_handle) {
  for (GpuChannelMap::iterator iter = gpu_channels_.begin();
       iter != gpu_channels_.end(); ++iter) {
    if (iter->second->GetChannelName() == channel_handle.name) {
      gpu_channels_.erase(iter);
      return;
    }
  }
}

void GpuThread::OnSynchronize() {
  Send(new GpuHostMsg_SynchronizeReply());
}

void GpuThread::OnCollectGraphicsInfo(GPUInfo::Level level) {
#if defined(OS_WIN)
  if (level == GPUInfo::kComplete && gpu_info_.level() <= GPUInfo::kPartial) {
    // Prevent concurrent collection of DirectX diagnostics.
    gpu_info_.SetLevel(GPUInfo::kCompleting);

    // Asynchronously collect the DirectX diagnostics because this can take a
    // couple of seconds.
    if (!base::WorkerPool::PostTask(
        FROM_HERE,
        NewRunnableFunction(&GpuThread::CollectDxDiagnostics, this),
        true)) {

      // Flag GPU info as complete if the DirectX diagnostics cannot be
      // collected.
      gpu_info_.SetLevel(GPUInfo::kComplete);
    } else {
      // Do not send response if we are still completing the GPUInfo struct
      return;
    }
  }
#endif
  Send(new GpuHostMsg_GraphicsInfoCollected(gpu_info_));
}

void GpuThread::OnCreateViewCommandBuffer(
    gfx::PluginWindowHandle window,
    int32 render_view_id,
    int32 renderer_id,
    const GPUCreateCommandBufferConfig& init_params) {
  int32 route_id = MSG_ROUTING_NONE;

  GpuChannelMap::const_iterator iter = gpu_channels_.find(renderer_id);
  if (iter != gpu_channels_.end()) {
    iter->second->CreateViewCommandBuffer(
        window, render_view_id, init_params, &route_id);
  }

  Send(new GpuHostMsg_CommandBufferCreated(route_id));
}

#if defined(OS_MACOSX)
void GpuThread::OnAcceleratedSurfaceBuffersSwappedACK(
    int renderer_id, int32 route_id, uint64 swap_buffers_count) {
  GpuChannelMap::const_iterator iter = gpu_channels_.find(renderer_id);
  if (iter == gpu_channels_.end())
    return;
  scoped_refptr<GpuChannel> channel = iter->second;
  channel->AcceleratedSurfaceBuffersSwapped(route_id, swap_buffers_count);
}
void GpuThread::OnDidDestroyAcceleratedSurface(
    int renderer_id, int32 renderer_route_id) {
  GpuChannelMap::const_iterator iter = gpu_channels_.find(renderer_id);
  if (iter == gpu_channels_.end())
    return;
  scoped_refptr<GpuChannel> channel = iter->second;
  channel->DidDestroySurface(renderer_route_id);
}
#endif

void GpuThread::OnCrash() {
  LOG(INFO) << "GPU: Simulating GPU crash";
  // Good bye, cruel world.
  volatile int* it_s_the_end_of_the_world_as_we_know_it = NULL;
  *it_s_the_end_of_the_world_as_we_know_it = 0xdead;
}

void GpuThread::OnHang() {
  LOG(INFO) << "GPU: Simulating GPU hang";
  for (;;) {
    // Do not sleep here. The GPU watchdog timer tracks the amount of user
    // time this thread is using and it doesn't use much while calling Sleep.
  }
}

#if defined(OS_WIN)

// Runs on a worker thread. The GpuThread never terminates voluntarily so it is
// safe to assume that its message loop is valid.
void GpuThread::CollectDxDiagnostics(GpuThread* thread) {
  app::win::ScopedCOMInitializer com_initializer;

  DxDiagNode node;
  gpu_info_collector::GetDxDiagnostics(&node);

  thread->message_loop()->PostTask(
      FROM_HERE,
      NewRunnableFunction(&GpuThread::SetDxDiagnostics, thread, node));
}

// Runs on the GPU thread.
void GpuThread::SetDxDiagnostics(GpuThread* thread, const DxDiagNode& node) {
  thread->gpu_info_.SetDxDiagnostics(node);
  thread->gpu_info_.SetLevel(GPUInfo::kComplete);
  thread->Send(new GpuHostMsg_GraphicsInfoCollected(thread->gpu_info_));
}

#endif
