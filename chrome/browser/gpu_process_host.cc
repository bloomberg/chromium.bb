// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gpu_process_host.h"

#include "app/app_switches.h"
#include "base/metrics/histogram.h"
#include "base/ref_counted.h"
#include "base/string_piece.h"
#include "base/threading/thread.h"
#include "chrome/browser/browser_thread.h"
#include "chrome/browser/gpu_process_host_ui_shim.h"
#include "chrome/browser/renderer_host/render_widget_host.h"
#include "chrome/browser/renderer_host/render_widget_host_view.h"
#include "chrome/browser/tab_contents/render_view_host_delegate_helper.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/gpu_feature_flags.h"
#include "chrome/common/gpu_info.h"
#include "chrome/common/gpu_messages.h"
#include "chrome/common/render_messages.h"
#include "chrome/gpu/gpu_thread.h"
#include "ipc/ipc_channel_handle.h"
#include "ipc/ipc_switches.h"
#include "media/base/media_switches.h"

#if defined(OS_LINUX)
#include "ui/gfx/gtk_native_view_id_manager.h"
#endif  // defined(OS_LINUX)

namespace {

enum GPUProcessLifetimeEvent {
  LAUNCHED,
  DIED_FIRST_TIME,
  DIED_SECOND_TIME,
  DIED_THIRD_TIME,
  DIED_FOURTH_TIME,
  GPU_PROCESS_LIFETIME_EVENT_MAX
  };

// Tasks used by this file
class RouteOnUIThreadTask : public Task {
 public:
  explicit RouteOnUIThreadTask(const IPC::Message& msg) : msg_(msg) {
  }

 private:
  void Run() {
    GpuProcessHostUIShim::GetInstance()->OnMessageReceived(msg_);
  }
  IPC::Message msg_;
};

// Global GpuProcessHost instance.
// We can not use Singleton<GpuProcessHost> because that gets
// terminated on the wrong thread (main thread). We need the
// GpuProcessHost to be terminated on the same thread on which it is
// initialized, the IO thread.
static GpuProcessHost* sole_instance_ = NULL;

// Number of times the gpu process has crashed in the current browser session.
static int g_gpu_crash_count = 0;
// Maximum number of times the gpu process is allowed to crash in a session.
// Once this limit is reached, any request to launch the gpu process will fail.
static const int kGpuMaxCrashCount = 3;

void RouteOnUIThread(const IPC::Message& message) {
  BrowserThread::PostTask(BrowserThread::UI,
                          FROM_HERE,
                          new RouteOnUIThreadTask(message));
}

bool SendDelayedMsg(IPC::Message* reply_msg) {
  return GpuProcessHost::Get()->Send(reply_msg);
}

}  // anonymous namespace

class GpuMainThread : public base::Thread {
 public:
  explicit GpuMainThread(const std::string& channel_id)
      : base::Thread("CrGpuMain"),
        channel_id_(channel_id) {
  }

  ~GpuMainThread() {
    Stop();
  }

 protected:
  virtual void Init() {
    // Must be created on GPU thread.
    gpu_thread_.reset(new GpuThread(channel_id_));
    gpu_thread_->Init(base::Time::Now());
  }

  virtual void CleanUp() {
    // Must be destroyed on GPU thread.
    gpu_thread_.reset();
  }

 private:
  scoped_ptr<GpuThread> gpu_thread_;
  std::string channel_id_;
  DISALLOW_COPY_AND_ASSIGN(GpuMainThread);
};

GpuProcessHost::GpuProcessHost()
    : BrowserChildProcessHost(GPU_PROCESS, NULL),
      initialized_(false),
      initialized_successfully_(false) {
  DCHECK_EQ(sole_instance_, static_cast<GpuProcessHost*>(NULL));
}

GpuProcessHost::~GpuProcessHost() {
  DCHECK_EQ(sole_instance_, this);
  sole_instance_ = NULL;
}

bool GpuProcessHost::EnsureInitialized() {
  if (!initialized_) {
    initialized_ = true;
    initialized_successfully_ = Init();
    if (initialized_successfully_) {
      Send(new GpuMsg_Initialize());
    }
  }
  return initialized_successfully_;
}

bool GpuProcessHost::Init() {
  if (!CreateChannel())
    return false;

  if (!CanLaunchGpuProcess())
    return false;

  return LaunchGpuProcess();
}

// static
GpuProcessHost* GpuProcessHost::Get() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  if (sole_instance_ == NULL)
    sole_instance_ = new GpuProcessHost();
  return sole_instance_;
}

bool GpuProcessHost::Send(IPC::Message* msg) {
  DCHECK(CalledOnValidThread());

  if (!EnsureInitialized())
    return false;

  return BrowserChildProcessHost::Send(msg);
}

bool GpuProcessHost::OnMessageReceived(const IPC::Message& message) {
  DCHECK(CalledOnValidThread());
  RouteOnUIThread(message);
  return true;
}

bool GpuProcessHost::CanShutdown() {
  return true;
}

namespace {

void SendOutstandingRepliesDispatcher() {
  GpuProcessHostUIShim::GetInstance()->SendOutstandingReplies();
}

void SendOutstandingReplies() {
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      NewRunnableFunction(&SendOutstandingRepliesDispatcher));
}

}  // namespace

void GpuProcessHost::OnChildDied() {
  SendOutstandingReplies();
  // Located in OnChildDied because OnProcessCrashed suffers from a race
  // condition on Linux.
  UMA_HISTOGRAM_ENUMERATION("GPU.GPUProcessLifetimeEvents",
                            DIED_FIRST_TIME + g_gpu_crash_count,
                            GPU_PROCESS_LIFETIME_EVENT_MAX);
  BrowserChildProcessHost::OnChildDied();
}

void GpuProcessHost::OnProcessCrashed(int exit_code) {
  SendOutstandingReplies();
  if (++g_gpu_crash_count >= kGpuMaxCrashCount) {
    // The gpu process is too unstable to use. Disable it for current session.
    RenderViewHostDelegateHelper::set_gpu_enabled(false);
  }
  BrowserChildProcessHost::OnProcessCrashed(exit_code);
}

bool GpuProcessHost::CanLaunchGpuProcess() const {
  return RenderViewHostDelegateHelper::gpu_enabled();
}

bool GpuProcessHost::LaunchGpuProcess() {
  if (g_gpu_crash_count >= kGpuMaxCrashCount)
    return false;

  const CommandLine& browser_command_line = *CommandLine::ForCurrentProcess();

  // If the single-process switch is present, just launch the GPU service in a
  // new thread in the browser process.
  if (browser_command_line.HasSwitch(switches::kSingleProcess)) {
    GpuMainThread* thread = new GpuMainThread(channel_id());

    base::Thread::Options options;
#if defined(OS_LINUX)
    options.message_loop_type = MessageLoop::TYPE_IO;
#else
    options.message_loop_type = MessageLoop::TYPE_UI;
#endif

    if (!thread->StartWithOptions(options))
      return false;

    return true;
  }

  CommandLine::StringType gpu_launcher =
      browser_command_line.GetSwitchValueNative(switches::kGpuLauncher);

  FilePath exe_path = ChildProcessHost::GetChildPath(gpu_launcher.empty());
  if (exe_path.empty())
    return false;

  CommandLine* cmd_line = new CommandLine(exe_path);
  cmd_line->AppendSwitchASCII(switches::kProcessType, switches::kGpuProcess);
  cmd_line->AppendSwitchASCII(switches::kProcessChannelID, channel_id());

  SetCrashReporterCommandLine(cmd_line);

  // Propagate relevant command line switches.
  static const char* const kSwitchNames[] = {
    switches::kUseGL,
    switches::kDisableGpuVsync,
    switches::kDisableGpuWatchdog,
    switches::kDisableLogging,
    switches::kEnableAcceleratedDecoding,
    switches::kEnableLogging,
#if defined(OS_MACOSX)
    switches::kEnableSandboxLogging,
#endif
    switches::kGpuStartupDialog,
    switches::kLoggingLevel,
    switches::kNoGpuSandbox,
    switches::kNoSandbox,
  };
  cmd_line->CopySwitchesFrom(browser_command_line, kSwitchNames,
                             arraysize(kSwitchNames));

  // If specified, prepend a launcher program to the command line.
  if (!gpu_launcher.empty())
    cmd_line->PrependWrapper(gpu_launcher);

  Launch(
#if defined(OS_WIN)
      FilePath(),
#elif defined(OS_POSIX)
      false,  // Never use the zygote (GPU plugin can't be sandboxed).
      base::environment_vector(),
#endif
      cmd_line);

  UMA_HISTOGRAM_ENUMERATION("GPU.GPUProcessLifetimeEvents",
                            LAUNCHED, GPU_PROCESS_LIFETIME_EVENT_MAX);
  return true;
}
