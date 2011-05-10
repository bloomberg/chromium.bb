// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/gpu/gpu_process_host.h"

#include "base/command_line.h"
#include "base/debug/trace_event.h"
#include "base/memory/ref_counted.h"
#include "base/metrics/histogram.h"
#include "base/process_util.h"
#include "base/string_piece.h"
#include "chrome/browser/tab_contents/render_view_host_delegate_helper.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/render_messages.h"
#include "content/browser/browser_thread.h"
#include "content/browser/gpu/gpu_data_manager.h"
#include "content/browser/gpu/gpu_process_host_ui_shim.h"
#include "content/browser/renderer_host/render_widget_host.h"
#include "content/browser/renderer_host/render_widget_host_view.h"
#include "content/common/gpu/gpu_messages.h"
#include "ipc/ipc_channel_handle.h"
#include "ipc/ipc_switches.h"
#include "media/base/media_switches.h"
#include "ui/gfx/gl/gl_context.h"
#include "ui/gfx/gl/gl_switches.h"

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

// A global map from GPU process host ID to GpuProcessHost.
static IDMap<GpuProcessHost> g_hosts_by_id;

// Number of times the gpu process has crashed in the current browser session.
static int g_gpu_crash_count = 0;

// Maximum number of times the gpu process is allowed to crash in a session.
// Once this limit is reached, any request to launch the gpu process will fail.
static const int kGpuMaxCrashCount = 3;

int g_last_host_id = 0;

#if defined(OS_LINUX)

class ReleasePermanentXIDDispatcher: public Task {
 public:
  explicit ReleasePermanentXIDDispatcher(gfx::PluginWindowHandle surface);
  void Run();
 private:
  gfx::PluginWindowHandle surface_;
};

ReleasePermanentXIDDispatcher::ReleasePermanentXIDDispatcher(
  gfx::PluginWindowHandle surface)
      : surface_(surface) {
}

void ReleasePermanentXIDDispatcher::Run() {
  GtkNativeViewManager* manager = GtkNativeViewManager::GetInstance();
  manager->ReleasePermanentXID(surface_);
}

#endif

void SendGpuProcessMessage(int renderer_id,
                           content::CauseForGpuLaunch cause,
                           IPC::Message* message) {
  GpuProcessHost* host = GpuProcessHost::GetForRenderer(
      renderer_id, cause);
  if (host) {
    host->Send(message);
  } else {
    delete message;
  }
}

}  // anonymous namespace

#if defined(OS_LINUX)
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
                          new ReleasePermanentXIDDispatcher(surface_));
}
#endif  // defined(OS_LINUX)

// static
GpuProcessHost* GpuProcessHost::GetForRenderer(
    int renderer_id, content::CauseForGpuLaunch cause) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  // Don't grant further access to GPU if it is not allowed.
  GpuDataManager* gpu_data_manager = GpuDataManager::GetInstance();
  if (gpu_data_manager != NULL && !gpu_data_manager->GpuAccessAllowed())
    return NULL;

  // The current policy is to ignore the renderer ID and use a single GPU
  // process for all renderers. Later this will be extended to allow the
  // use of multiple GPU processes.
  if (!g_hosts_by_id.IsEmpty()) {
    IDMap<GpuProcessHost>::iterator it(&g_hosts_by_id);
    return it.GetCurrentValue();
  }

  if (cause == content::CAUSE_FOR_GPU_LAUNCH_NO_LAUNCH)
    return NULL;

  int host_id;
  /* TODO(apatrick): this is currently broken because this function is called on
     the IO thread from GpuMessageFilter, and we don't have an IO thread object
     when running the GPU code in the browser at the moment.
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kSingleProcess) ||
      CommandLine::ForCurrentProcess()->HasSwitch(switches::kInProcessGPU)) {
    if (!g_browser_process->gpu_thread())
      return NULL;

    // Initialize GL on the GPU thread.
    // TODO(apatrick): Handle failure to initialize (asynchronously).
    if (!BrowserThread::PostTask(
        BrowserThread::GPU,
        FROM_HERE,
        NewRunnableFunction(&gfx::GLContext::InitializeOneOff))) {
      return NULL;
    }

    host_id = 0;
  } else {
    host_id = ++g_last_host_id;
  }
  */
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
void GpuProcessHost::SendOnIO(int renderer_id,
                              content::CauseForGpuLaunch cause,
                              IPC::Message* message) {
  BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        NewRunnableFunction(
            &SendGpuProcessMessage, renderer_id, cause, message));
}

// static
GpuProcessHost* GpuProcessHost::FromID(int host_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  if (host_id == 0)
    return NULL;

  return g_hosts_by_id.Lookup(host_id);
}

GpuProcessHost::GpuProcessHost(int host_id)
    : BrowserChildProcessHost(GPU_PROCESS),
      host_id_(host_id),
      gpu_process_(base::kNullProcessHandle) {
  g_hosts_by_id.AddWithID(this, host_id_);
  if (host_id == 0)
    gpu_process_ = base::GetCurrentProcessHandle();

  // Post a task to create the corresponding GpuProcessHostUIShim.  The
  // GpuProcessHostUIShim will be destroyed if either the browser exits,
  // in which case it calls GpuProcessHostUIShim::DestroyAll, or the
  // GpuProcessHost is destroyed, which happens when the corresponding GPU
  // process terminates or fails to launch.
  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      NewRunnableFunction(&GpuProcessHostUIShim::Create, host_id));
}

GpuProcessHost::~GpuProcessHost() {
  DCHECK(CalledOnValidThread());
#if defined(OS_WIN)
  if (gpu_process_)
    CloseHandle(gpu_process_);
#endif

  // In case we never started, clean up.
  while (!queued_messages_.empty()) {
    delete queued_messages_.front();
    queued_messages_.pop();
  }

  g_hosts_by_id.Remove(host_id_);

  BrowserThread::PostTask(BrowserThread::UI,
                          FROM_HERE,
                          NewRunnableFunction(GpuProcessHostUIShim::Destroy,
                                              host_id_));
}

bool GpuProcessHost::Init() {
  if (!CreateChannel())
    return false;

  if (!LaunchGpuProcess())
    return false;

  return Send(new GpuMsg_Initialize());
}

void GpuProcessHost::RouteOnUIThread(const IPC::Message& message) {
  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      new RouteToGpuProcessHostUIShimTask(host_id_, message));
}

bool GpuProcessHost::Send(IPC::Message* msg) {
  DCHECK(CalledOnValidThread());
  if (opening_channel()) {
    queued_messages_.push(msg);
    return true;
  }

  return BrowserChildProcessHost::Send(msg);
}

bool GpuProcessHost::OnMessageReceived(const IPC::Message& message) {
  DCHECK(CalledOnValidThread());
  IPC_BEGIN_MESSAGE_MAP(GpuProcessHost, message)
    IPC_MESSAGE_HANDLER(GpuHostMsg_ChannelEstablished, OnChannelEstablished)
    IPC_MESSAGE_HANDLER(GpuHostMsg_SynchronizeReply, OnSynchronizeReply)
    IPC_MESSAGE_HANDLER(GpuHostMsg_CommandBufferCreated, OnCommandBufferCreated)
    IPC_MESSAGE_HANDLER(GpuHostMsg_DestroyCommandBuffer, OnDestroyCommandBuffer)
    IPC_MESSAGE_HANDLER(GpuHostMsg_GraphicsInfoCollected,
                        OnGraphicsInfoCollected)
    IPC_MESSAGE_UNHANDLED(RouteOnUIThread(message))
  IPC_END_MESSAGE_MAP()

  return true;
}

void GpuProcessHost::OnChannelConnected(int32 peer_pid) {
  while (!queued_messages_.empty()) {
    Send(queued_messages_.front());
    queued_messages_.pop();
  }
}

void GpuProcessHost::EstablishGpuChannel(
    int renderer_id,
    EstablishChannelCallback *callback) {
  DCHECK(CalledOnValidThread());
  TRACE_EVENT0("gpu", "GpuProcessHostUIShim::EstablishGpuChannel");
  linked_ptr<EstablishChannelCallback> wrapped_callback(callback);

  // If GPU features are already blacklisted, no need to establish the channel.
  if (!GpuDataManager::GetInstance()->GpuAccessAllowed()) {
    EstablishChannelError(
        wrapped_callback.release(), IPC::ChannelHandle(),
        base::kNullProcessHandle, GPUInfo());
    return;
  }

  if (Send(new GpuMsg_EstablishChannel(renderer_id))) {
    channel_requests_.push(wrapped_callback);
  } else {
    EstablishChannelError(
        wrapped_callback.release(), IPC::ChannelHandle(),
        base::kNullProcessHandle, GPUInfo());
  }
}

void GpuProcessHost::Synchronize(SynchronizeCallback* callback) {
  DCHECK(CalledOnValidThread());
  linked_ptr<SynchronizeCallback> wrapped_callback(callback);

  if (Send(new GpuMsg_Synchronize())) {
    synchronize_requests_.push(wrapped_callback);
  } else {
    SynchronizeError(wrapped_callback.release());
  }
}

void GpuProcessHost::CreateViewCommandBuffer(
    gfx::PluginWindowHandle compositing_surface,
    int32 render_view_id,
    int32 renderer_id,
    const GPUCreateCommandBufferConfig& init_params,
    CreateCommandBufferCallback* callback) {
  DCHECK(CalledOnValidThread());
  linked_ptr<CreateCommandBufferCallback> wrapped_callback(callback);

#if defined(OS_LINUX)
  ViewID view_id(renderer_id, render_view_id);

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
#endif  // defined(OS_LINUX)

  if (compositing_surface != gfx::kNullPluginWindow &&
      Send(new GpuMsg_CreateViewCommandBuffer(
          compositing_surface, render_view_id, renderer_id, init_params))) {
    create_command_buffer_requests_.push(wrapped_callback);
#if defined(OS_LINUX)
    surface_refs_.insert(std::pair<ViewID, linked_ptr<SurfaceRef> >(
        view_id, surface_ref));
#endif  // defined(OS_LINUX)
  } else {
    CreateCommandBufferError(wrapped_callback.release(), MSG_ROUTING_NONE);
  }
}

void GpuProcessHost::OnChannelEstablished(
    const IPC::ChannelHandle& channel_handle) {
  // The GPU process should have launched at this point and this object should
  // have been notified of its process handle.
  DCHECK(gpu_process_);

  linked_ptr<EstablishChannelCallback> callback = channel_requests_.front();
  channel_requests_.pop();

  // Currently if any of the GPU features are blacklisted, we don't establish a
  // GPU channel.
  if (!channel_handle.name.empty() &&
      !GpuDataManager::GetInstance()->GpuAccessAllowed()) {
    Send(new GpuMsg_CloseChannel(channel_handle));
    EstablishChannelError(callback.release(),
                          IPC::ChannelHandle(),
                          base::kNullProcessHandle,
                          GPUInfo());
    RouteOnUIThread(GpuHostMsg_OnLogMessage(
        logging::LOG_WARNING,
        "WARNING", 
        "Hardware acceleration is unavailable."));
    return;
  }

  callback->Run(
      channel_handle, gpu_process_, GpuDataManager::GetInstance()->gpu_info());
}

void GpuProcessHost::OnSynchronizeReply() {
  // Guard against race conditions in abrupt GPU process termination.
  if (!synchronize_requests_.empty()) {
    linked_ptr<SynchronizeCallback> callback(synchronize_requests_.front());
    synchronize_requests_.pop();
    callback->Run();
  }
}

void GpuProcessHost::OnCommandBufferCreated(const int32 route_id) {
  if (!create_command_buffer_requests_.empty()) {
    linked_ptr<CreateCommandBufferCallback> callback =
        create_command_buffer_requests_.front();
    create_command_buffer_requests_.pop();
    if (route_id == MSG_ROUTING_NONE)
      CreateCommandBufferError(callback.release(), route_id);
    else
      callback->Run(route_id);
  }
}

void GpuProcessHost::OnDestroyCommandBuffer(
    gfx::PluginWindowHandle window, int32 renderer_id,
    int32 render_view_id) {
#if defined(OS_LINUX)
  ViewID view_id(renderer_id, render_view_id);
  SurfaceRefMap::iterator it = surface_refs_.find(view_id);
  if (it != surface_refs_.end())
    surface_refs_.erase(it);
#endif  // defined(OS_LINUX)
}

void GpuProcessHost::OnGraphicsInfoCollected(const GPUInfo& gpu_info) {
  GpuDataManager::GetInstance()->UpdateGpuInfo(gpu_info);
}

bool GpuProcessHost::CanShutdown() {
  return true;
}

void GpuProcessHost::OnProcessLaunched() {
  // Send the GPU process handle to the UI thread before it has to
  // respond to any requests to establish a GPU channel. The response
  // to such requests require that the GPU process handle be known.
#if defined(OS_WIN)
  DuplicateHandle(base::GetCurrentProcessHandle(),
                  handle(),
                  base::GetCurrentProcessHandle(),
                  &gpu_process_,
                  PROCESS_DUP_HANDLE,
                  FALSE,
                  0);
#else
  gpu_process_ = handle();
#endif
}

void GpuProcessHost::OnChildDied() {
  SendOutstandingReplies();
  // Located in OnChildDied because OnProcessCrashed suffers from a race
  // condition on Linux.
  UMA_HISTOGRAM_ENUMERATION("GPU.GPUProcessLifetimeEvents",
                            DIED_FIRST_TIME + g_gpu_crash_count,
                            GPU_PROCESS_LIFETIME_EVENT_MAX);
  base::TerminationStatus status = GetChildTerminationStatus(NULL);
  UMA_HISTOGRAM_ENUMERATION("GPU.GPUProcessTerminationStatus",
                            status,
                            base::TERMINATION_STATUS_MAX_ENUM);
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

bool GpuProcessHost::LaunchGpuProcess() {
  if (!RenderViewHostDelegateHelper::gpu_enabled() ||
      g_gpu_crash_count >= kGpuMaxCrashCount)
  {
    SendOutstandingReplies();
    RenderViewHostDelegateHelper::set_gpu_enabled(false);
    return false;
  }

  const CommandLine& browser_command_line = *CommandLine::ForCurrentProcess();

  CommandLine::StringType gpu_launcher =
      browser_command_line.GetSwitchValueNative(switches::kGpuLauncher);

  FilePath exe_path = ChildProcessHost::GetChildPath(gpu_launcher.empty());
  if (exe_path.empty())
    return false;

  CommandLine* cmd_line = new CommandLine(exe_path);
  cmd_line->AppendSwitchASCII(switches::kProcessType, switches::kGpuProcess);
  cmd_line->AppendSwitchASCII(switches::kProcessChannelID, channel_id());

  // Propagate relevant command line switches.
  static const char* const kSwitchNames[] = {
    switches::kUseGL,
    switches::kDisableGpuSandbox,
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
    switches::kNoSandbox,
    switches::kDisableGLMultisampling,
  };
  cmd_line->CopySwitchesFrom(browser_command_line, kSwitchNames,
                             arraysize(kSwitchNames));

  GpuFeatureFlags flags = GpuDataManager::GetInstance()->GetGpuFeatureFlags();
  if (flags.flags() & GpuFeatureFlags::kGpuFeatureMultisampling)
    cmd_line->AppendSwitch(switches::kDisableGLMultisampling);

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
    linked_ptr<EstablishChannelCallback> callback = channel_requests_.front();
    channel_requests_.pop();
    EstablishChannelError(callback.release(),
                          IPC::ChannelHandle(),
                          base::kNullProcessHandle,
                          GPUInfo());
  }

  // Now unblock all renderers waiting for synchronization replies.
  while (!synchronize_requests_.empty()) {
    linked_ptr<SynchronizeCallback> callback = synchronize_requests_.front();
    synchronize_requests_.pop();
    SynchronizeError(callback.release());
  }
}

void GpuProcessHost::EstablishChannelError(
    EstablishChannelCallback* callback,
    const IPC::ChannelHandle& channel_handle,
    base::ProcessHandle renderer_process_for_gpu,
    const GPUInfo& gpu_info) {
  scoped_ptr<EstablishChannelCallback> wrapped_callback(callback);
  wrapped_callback->Run(channel_handle, renderer_process_for_gpu, gpu_info);
}

void GpuProcessHost::CreateCommandBufferError(
    CreateCommandBufferCallback* callback, int32 route_id) {
  scoped_ptr<GpuProcessHost::CreateCommandBufferCallback>
    wrapped_callback(callback);
  callback->Run(route_id);
}

void GpuProcessHost::SynchronizeError(SynchronizeCallback* callback) {
  scoped_ptr<SynchronizeCallback> wrapped_callback(callback);
  wrapped_callback->Run();
}
