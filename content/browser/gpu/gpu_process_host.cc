// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/gpu/gpu_process_host.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/debug/trace_event.h"
#include "base/lazy_instance.h"
#include "base/memory/ref_counted.h"
#include "base/metrics/histogram.h"
#include "base/process_util.h"
#include "base/string_piece.h"
#include "base/threading/thread.h"
#include "content/browser/gpu/gpu_data_manager.h"
#include "content/browser/gpu/gpu_process_host_ui_shim.h"
#include "content/browser/renderer_host/render_widget_host.h"
#include "content/browser/renderer_host/render_widget_host_view.h"
#include "content/common/child_process_host_impl.h"
#include "content/common/gpu/gpu_messages.h"
#include "content/gpu/gpu_child_thread.h"
#include "content/gpu/gpu_process.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/result_codes.h"
#include "ipc/ipc_channel_handle.h"
#include "ipc/ipc_switches.h"
#include "ui/gfx/gl/gl_context.h"
#include "ui/gfx/gl/gl_implementation.h"
#include "ui/gfx/gl/gl_switches.h"

#if defined(TOOLKIT_USES_GTK)
#include "ui/gfx/gtk_native_view_id_manager.h"
#endif

using content::BrowserThread;
using content::ChildProcessHost;

bool GpuProcessHost::gpu_enabled_ = true;

namespace {

enum GPUProcessLifetimeEvent {
  LAUNCHED,
  DIED_FIRST_TIME,
  DIED_SECOND_TIME,
  DIED_THIRD_TIME,
  DIED_FOURTH_TIME,
  GPU_PROCESS_LIFETIME_EVENT_MAX
};

// A global map from GPU process host ID to GpuProcessHost.
static base::LazyInstance<IDMap<GpuProcessHost> > g_hosts_by_id =
    LAZY_INSTANCE_INITIALIZER;

// Number of times the gpu process has crashed in the current browser session.
static int g_gpu_crash_count = 0;

// Maximum number of times the gpu process is allowed to crash in a session.
// Once this limit is reached, any request to launch the gpu process will fail.
static const int kGpuMaxCrashCount = 3;

int g_last_host_id = 0;

#if defined(TOOLKIT_USES_GTK)

void ReleasePermanentXIDDispatcher(gfx::PluginWindowHandle surface) {
  GtkNativeViewManager* manager = GtkNativeViewManager::GetInstance();
  manager->ReleasePermanentXID(surface);
}

#endif

void SendGpuProcessMessage(int client_id,
                           content::CauseForGpuLaunch cause,
                           IPC::Message* message) {
  GpuProcessHost* host = GpuProcessHost::GetForRenderer(
      client_id, cause);
  if (host) {
    host->Send(message);
  } else {
    delete message;
  }
}

}  // anonymous namespace

#if defined(TOOLKIT_USES_GTK)
// Used to put a lock on surfaces so that the window to which the GPU
// process is drawing to doesn't disappear while it is drawing when
// a tab is closed.
class GpuProcessHost::SurfaceRef {
 public:
  explicit SurfaceRef(gfx::PluginWindowHandle surface);
  ~SurfaceRef();
 private:
  gfx::PluginWindowHandle surface_;
};

GpuProcessHost::SurfaceRef::SurfaceRef(gfx::PluginWindowHandle surface)
    : surface_(surface) {
  GtkNativeViewManager* manager = GtkNativeViewManager::GetInstance();
  if (!manager->AddRefPermanentXID(surface_)) {
    LOG(ERROR) << "Surface " << surface << " cannot be referenced.";
  }
}

GpuProcessHost::SurfaceRef::~SurfaceRef() {
  BrowserThread::PostTask(BrowserThread::UI,
                          FROM_HERE,
                          base::Bind(&ReleasePermanentXIDDispatcher, surface_));
}
#endif  // defined(TOOLKIT_USES_GTK)

// This class creates a GPU thread (instead of a GPU process), when running
// with --in-process-gpu or --single-process.
class GpuMainThread : public base::Thread {
 public:
  explicit GpuMainThread(const std::string& channel_id)
      : base::Thread("Chrome_InProcGpuThread"),
        channel_id_(channel_id),
        gpu_process_(NULL),
        child_thread_(NULL) {
  }

  ~GpuMainThread() {
    Stop();
  }

 protected:
  virtual void Init() {
    // TODO: Currently, ChildProcess supports only a single static instance,
    // which is a problem in --single-process mode, where both gpu and renderer
    // should be able to create separate instances.
    if (GpuProcess::current()) {
      child_thread_ = new GpuChildThread(channel_id_);
    } else {
      gpu_process_ = new GpuProcess();
      // The process object takes ownership of the thread object, so do not
      // save and delete the pointer.
      gpu_process_->set_main_thread(new GpuChildThread(channel_id_));
    }
  }

  virtual void CleanUp() {
    delete gpu_process_;
    if (child_thread_)
      delete child_thread_;
  }

 private:
  std::string channel_id_;
  // Deleted in CleanUp() on the gpu thread, so don't use smart pointers.
  GpuProcess* gpu_process_;
  GpuChildThread* child_thread_;

  DISALLOW_COPY_AND_ASSIGN(GpuMainThread);
};

static bool HostIsValid(GpuProcessHost* host) {
  if (!host)
    return false;

  // Check if the GPU process has died and the host is about to be destroyed.
  if (host->disconnect_was_alive())
    return false;

  // The Gpu process is invalid if it's not using software, the card is
  // blacklisted, and we can kill it and start over.
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kSingleProcess) ||
      CommandLine::ForCurrentProcess()->HasSwitch(switches::kInProcessGPU) ||
      host->software_rendering() ||
      !GpuDataManager::GetInstance()->software_rendering()) {
    return true;
  }

  host->ForceShutdown();
  return false;
}

// static
GpuProcessHost* GpuProcessHost::GetForRenderer(
    int client_id, content::CauseForGpuLaunch cause) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  // Don't grant further access to GPU if it is not allowed.
  GpuDataManager* gpu_data_manager = GpuDataManager::GetInstance();
  if (gpu_data_manager != NULL && !gpu_data_manager->GpuAccessAllowed())
    return NULL;

  // The current policy is to ignore the renderer ID and use a single GPU
  // process (the first valid host in the host-id map) for all renderers. Later
  // this will be extended to allow the use of multiple GPU processes.
  for (IDMap<GpuProcessHost>::iterator it(g_hosts_by_id.Pointer());
       !it.IsAtEnd(); it.Advance()) {
    GpuProcessHost *host = it.GetCurrentValue();

    if (HostIsValid(host))
      return host;
  }

  if (cause == content::CAUSE_FOR_GPU_LAUNCH_NO_LAUNCH)
    return NULL;

  int host_id;
  host_id = ++g_last_host_id;

  UMA_HISTOGRAM_ENUMERATION("GPU.GPUProcessLaunchCause",
                            cause,
                            content::CAUSE_FOR_GPU_LAUNCH_MAX_ENUM);

  GpuProcessHost* host = new GpuProcessHost(host_id);
  if (host->Init())
    return host;

  delete host;
  return NULL;
}

// static
void GpuProcessHost::SendOnIO(int client_id,
                              content::CauseForGpuLaunch cause,
                              IPC::Message* message) {
  BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(
            &SendGpuProcessMessage, client_id, cause, message));
}

// static
GpuProcessHost* GpuProcessHost::FromID(int host_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  if (host_id == 0)
    return NULL;

  GpuProcessHost *host = g_hosts_by_id.Pointer()->Lookup(host_id);
  if (HostIsValid(host))
    return host;

  return NULL;
}

GpuProcessHost::GpuProcessHost(int host_id)
    : BrowserChildProcessHost(content::PROCESS_TYPE_GPU),
      host_id_(host_id),
      gpu_process_(base::kNullProcessHandle),
      in_process_(false),
      software_rendering_(false) {
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kSingleProcess) ||
      CommandLine::ForCurrentProcess()->HasSwitch(switches::kInProcessGPU))
    in_process_ = true;

  // If the 'single GPU process' policy ever changes, we still want to maintain
  // it for 'gpu thread' mode and only create one instance of host and thread.
  DCHECK(!in_process_ || g_hosts_by_id.Pointer()->IsEmpty());

  g_hosts_by_id.Pointer()->AddWithID(this, host_id_);

  // Post a task to create the corresponding GpuProcessHostUIShim.  The
  // GpuProcessHostUIShim will be destroyed if either the browser exits,
  // in which case it calls GpuProcessHostUIShim::DestroyAll, or the
  // GpuProcessHost is destroyed, which happens when the corresponding GPU
  // process terminates or fails to launch.
  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(base::IgnoreResult(&GpuProcessHostUIShim::Create), host_id));
}

GpuProcessHost::~GpuProcessHost() {
  DCHECK(CalledOnValidThread());

  SendOutstandingReplies();
  UMA_HISTOGRAM_ENUMERATION("GPU.GPUProcessLifetimeEvents",
                            DIED_FIRST_TIME + g_gpu_crash_count,
                            GPU_PROCESS_LIFETIME_EVENT_MAX);

  int exit_code;
  base::TerminationStatus status = GetChildTerminationStatus(&exit_code);
  UMA_HISTOGRAM_ENUMERATION("GPU.GPUProcessTerminationStatus",
                            status,
                            base::TERMINATION_STATUS_MAX_ENUM);

  if (status == base::TERMINATION_STATUS_NORMAL_TERMINATION ||
      status == base::TERMINATION_STATUS_ABNORMAL_TERMINATION) {
    UMA_HISTOGRAM_ENUMERATION("GPU.GPUProcessExitCode",
                              exit_code,
                              content::RESULT_CODE_LAST_CODE);
  }

#if defined(OS_WIN)
  if (gpu_process_)
    CloseHandle(gpu_process_);
#endif

  // In case we never started, clean up.
  while (!queued_messages_.empty()) {
    delete queued_messages_.front();
    queued_messages_.pop();
  }

  g_hosts_by_id.Pointer()->Remove(host_id_);

  BrowserThread::PostTask(BrowserThread::UI,
                          FROM_HERE,
                          base::Bind(&GpuProcessHostUIShim::Destroy, host_id_));
}

bool GpuProcessHost::Init() {
  std::string channel_id = child_process_host()->CreateChannel();
  if (channel_id.empty())
    return false;

  if (in_process_) {
    CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kDisableGpuWatchdog);

    in_process_gpu_thread_.reset(new GpuMainThread(channel_id));

    base::Thread::Options options;
#if defined(OS_WIN)
  // On Windows the GPU thread needs to pump the compositor child window's
  // message loop. TODO(apatrick): make this an IO thread if / when we get rid
  // of this child window. Unfortunately it might always be necessary for
  // Windows XP because we cannot share the backing store textures between
  // processes.
  options.message_loop_type = MessageLoop::TYPE_UI;
#else
  options.message_loop_type = MessageLoop::TYPE_IO;
#endif
    in_process_gpu_thread_->StartWithOptions(options);

    OnProcessLaunched();  // Fake a callback that the process is ready.
  } else if (!LaunchGpuProcess(channel_id))
    return false;

  return Send(new GpuMsg_Initialize());
}

void GpuProcessHost::RouteOnUIThread(const IPC::Message& message) {
  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(&RouteToGpuProcessHostUIShimTask, host_id_, message));
}

bool GpuProcessHost::Send(IPC::Message* msg) {
  DCHECK(CalledOnValidThread());
  if (child_process_host()->IsChannelOpening()) {
    queued_messages_.push(msg);
    return true;
  }

  return BrowserChildProcessHost::Send(msg);
}

bool GpuProcessHost::OnMessageReceived(const IPC::Message& message) {
  DCHECK(CalledOnValidThread());
  IPC_BEGIN_MESSAGE_MAP(GpuProcessHost, message)
    IPC_MESSAGE_HANDLER(GpuHostMsg_ChannelEstablished, OnChannelEstablished)
    IPC_MESSAGE_HANDLER(GpuHostMsg_CommandBufferCreated, OnCommandBufferCreated)
    IPC_MESSAGE_HANDLER(GpuHostMsg_DestroyCommandBuffer, OnDestroyCommandBuffer)
    IPC_MESSAGE_UNHANDLED(RouteOnUIThread(message))
  IPC_END_MESSAGE_MAP()

  return true;
}

void GpuProcessHost::OnChannelConnected(int32 peer_pid) {
  BrowserChildProcessHost::OnChannelConnected(peer_pid);
  while (!queued_messages_.empty()) {
    Send(queued_messages_.front());
    queued_messages_.pop();
  }
}

void GpuProcessHost::EstablishGpuChannel(
    int client_id,
    const EstablishChannelCallback& callback) {
  DCHECK(CalledOnValidThread());
  TRACE_EVENT0("gpu", "GpuProcessHostUIShim::EstablishGpuChannel");

  // If GPU features are already blacklisted, no need to establish the channel.
  if (!GpuDataManager::GetInstance()->GpuAccessAllowed()) {
    EstablishChannelError(
        callback, IPC::ChannelHandle(),
        base::kNullProcessHandle, content::GPUInfo());
    return;
  }

  if (Send(new GpuMsg_EstablishChannel(client_id, 0))) {
    channel_requests_.push(callback);
  } else {
    EstablishChannelError(
        callback, IPC::ChannelHandle(),
        base::kNullProcessHandle, content::GPUInfo());
  }
}

void GpuProcessHost::CreateViewCommandBuffer(
    gfx::PluginWindowHandle compositing_surface,
    int32 render_view_id,
    int32 client_id,
    const GPUCreateCommandBufferConfig& init_params,
    const CreateCommandBufferCallback& callback) {
  DCHECK(CalledOnValidThread());

#if defined(TOOLKIT_USES_GTK)
  ViewID view_id(client_id, render_view_id);

  // There should only be one such command buffer (for the compositor).  In
  // practice, if the GPU process lost a context, GraphicsContext3D with
  // associated command buffer and view surface will not be gone until new
  // one is in place and all layers are reattached.
  linked_ptr<SurfaceRef> surface_ref;
  SurfaceRefMap::iterator it = surface_refs_.find(view_id);
  if (it != surface_refs_.end())
    surface_ref = (*it).second;
  else
    surface_ref.reset(new SurfaceRef(compositing_surface));
#endif  // defined(TOOLKIT_USES_GTK)

  if (compositing_surface != gfx::kNullPluginWindow &&
      Send(new GpuMsg_CreateViewCommandBuffer(
          compositing_surface, render_view_id, client_id, init_params))) {
    create_command_buffer_requests_.push(callback);
#if defined(TOOLKIT_USES_GTK)
    surface_refs_.insert(std::pair<ViewID, linked_ptr<SurfaceRef> >(
        view_id, surface_ref));
#endif
  } else {
    CreateCommandBufferError(callback, MSG_ROUTING_NONE);
  }
}

void GpuProcessHost::OnChannelEstablished(
    const IPC::ChannelHandle& channel_handle) {
  // The GPU process should have launched at this point and this object should
  // have been notified of its process handle.
  DCHECK(gpu_process_);

  EstablishChannelCallback callback = channel_requests_.front();
  channel_requests_.pop();

  // Currently if any of the GPU features are blacklisted, we don't establish a
  // GPU channel.
  if (!channel_handle.name.empty() &&
      !GpuDataManager::GetInstance()->GpuAccessAllowed()) {
    Send(new GpuMsg_CloseChannel(channel_handle));
    EstablishChannelError(callback,
                          IPC::ChannelHandle(),
                          base::kNullProcessHandle,
                          content::GPUInfo());
    RouteOnUIThread(GpuHostMsg_OnLogMessage(
        logging::LOG_WARNING,
        "WARNING",
        "Hardware acceleration is unavailable."));
    return;
  }

  callback.Run(
      channel_handle, gpu_process_, GpuDataManager::GetInstance()->gpu_info());
}

void GpuProcessHost::OnCommandBufferCreated(const int32 route_id) {
  if (!create_command_buffer_requests_.empty()) {
    CreateCommandBufferCallback callback =
        create_command_buffer_requests_.front();
    create_command_buffer_requests_.pop();
    if (route_id == MSG_ROUTING_NONE)
      CreateCommandBufferError(callback, route_id);
    else
      callback.Run(route_id);
  }
}

void GpuProcessHost::OnDestroyCommandBuffer(
    gfx::PluginWindowHandle window, int32 client_id,
    int32 render_view_id) {
#if defined(TOOLKIT_USES_GTK)
  ViewID view_id(client_id, render_view_id);
  SurfaceRefMap::iterator it = surface_refs_.find(view_id);
  if (it != surface_refs_.end())
    surface_refs_.erase(it);
#endif  // defined(TOOLKIT_USES_GTK)
}

void GpuProcessHost::OnProcessLaunched() {
  // Send the GPU process handle to the UI thread before it has to
  // respond to any requests to establish a GPU channel. The response
  // to such requests require that the GPU process handle be known.

  base::ProcessHandle child_handle = in_process_ ?
      base::GetCurrentProcessHandle() : data().handle;

#if defined(OS_WIN)
  DuplicateHandle(base::GetCurrentProcessHandle(),
                  child_handle,
                  base::GetCurrentProcessHandle(),
                  &gpu_process_,
                  PROCESS_DUP_HANDLE,
                  FALSE,
                  0);
#else
  gpu_process_ = child_handle;
#endif
}

void GpuProcessHost::OnProcessCrashed(int exit_code) {
  SendOutstandingReplies();
  if (++g_gpu_crash_count >= kGpuMaxCrashCount) {
    // The gpu process is too unstable to use. Disable it for current session.
    gpu_enabled_ = false;
  }
  BrowserChildProcessHost::OnProcessCrashed(exit_code);
}

bool GpuProcessHost::software_rendering() {
  return software_rendering_;
}

void GpuProcessHost::ForceShutdown() {
  g_hosts_by_id.Pointer()->Remove(host_id_);
  BrowserChildProcessHost::ForceShutdown();
}

bool GpuProcessHost::LaunchGpuProcess(const std::string& channel_id) {
  if (!gpu_enabled_ || g_gpu_crash_count >= kGpuMaxCrashCount) {
    SendOutstandingReplies();
    gpu_enabled_ = false;
    return false;
  }

  const CommandLine& browser_command_line = *CommandLine::ForCurrentProcess();

  CommandLine::StringType gpu_launcher =
      browser_command_line.GetSwitchValueNative(switches::kGpuLauncher);

#if defined(OS_LINUX)
  int child_flags = gpu_launcher.empty() ? ChildProcessHost::CHILD_ALLOW_SELF :
                                           ChildProcessHost::CHILD_NORMAL;
#else
  int child_flags = ChildProcessHost::CHILD_NORMAL;
#endif

  FilePath exe_path = ChildProcessHost::GetChildPath(child_flags);
  if (exe_path.empty())
    return false;

  CommandLine* cmd_line = new CommandLine(exe_path);
  cmd_line->AppendSwitchASCII(switches::kProcessType, switches::kGpuProcess);
  cmd_line->AppendSwitchASCII(switches::kProcessChannelID, channel_id);

  // Propagate relevant command line switches.
  static const char* const kSwitchNames[] = {
    switches::kDisableBreakpad,
    switches::kDisableGLMultisampling,
    switches::kDisableGpuDriverBugWorkarounds,
    switches::kDisableGpuSandbox,
    switches::kDisableGpuVsync,
    switches::kDisableGpuWatchdog,
    switches::kDisableImageTransportSurface,
    switches::kDisableLogging,
    switches::kEnableGPUServiceLogging,
    switches::kEnableLogging,
#if defined(OS_MACOSX)
    switches::kEnableSandboxLogging,
#endif
    switches::kGpuNoContextLost,
    switches::kGpuStartupDialog,
    switches::kLoggingLevel,
    switches::kNoSandbox,
    switches::kTraceStartup,
  };
  cmd_line->CopySwitchesFrom(browser_command_line, kSwitchNames,
                             arraysize(kSwitchNames));

  // If --ignore-gpu-blacklist is passed in, don't send in crash reports
  // because GPU is expected to be unreliable.
  if (browser_command_line.HasSwitch(switches::kIgnoreGpuBlacklist) &&
      !cmd_line->HasSwitch(switches::kDisableBreakpad))
    cmd_line->AppendSwitch(switches::kDisableBreakpad);

  GpuDataManager::GetInstance()->AppendGpuCommandLine(cmd_line);

  if (cmd_line->HasSwitch(switches::kUseGL))
    software_rendering_ =
        (cmd_line->GetSwitchValueASCII(switches::kUseGL) == "swiftshader");

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

void GpuProcessHost::SendOutstandingReplies() {
  // First send empty channel handles for all EstablishChannel requests.
  while (!channel_requests_.empty()) {
    EstablishChannelCallback callback = channel_requests_.front();
    channel_requests_.pop();
    EstablishChannelError(callback,
                          IPC::ChannelHandle(),
                          base::kNullProcessHandle,
                          content::GPUInfo());
  }
}

void GpuProcessHost::EstablishChannelError(
    const EstablishChannelCallback& callback,
    const IPC::ChannelHandle& channel_handle,
    base::ProcessHandle renderer_process_for_gpu,
    const content::GPUInfo& gpu_info) {
  callback.Run(channel_handle, renderer_process_for_gpu, gpu_info);
}

void GpuProcessHost::CreateCommandBufferError(
    const CreateCommandBufferCallback& callback, int32 route_id) {
  callback.Run(route_id);
}
