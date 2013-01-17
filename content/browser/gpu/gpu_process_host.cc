// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/gpu/gpu_process_host.h"

#include "base/base_switches.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/debug/trace_event.h"
#include "base/memory/ref_counted.h"
#include "base/metrics/histogram.h"
#include "base/process_util.h"
#include "base/threading/thread.h"
#include "content/browser/browser_child_process_host_impl.h"
#include "content/browser/gpu/gpu_data_manager_impl.h"
#include "content/browser/gpu/gpu_process_host_ui_shim.h"
#include "content/browser/gpu/gpu_surface_tracker.h"
#include "content/browser/renderer_host/render_widget_helper.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/common/child_process_host_impl.h"
#include "content/common/gpu/gpu_messages.h"
#include "content/common/view_messages.h"
#include "content/gpu/gpu_child_thread.h"
#include "content/gpu/gpu_process.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/result_codes.h"
#include "gpu/command_buffer/service/gpu_switches.h"
#include "ipc/ipc_channel_handle.h"
#include "ipc/ipc_switches.h"
#include "ui/gl/gl_switches.h"

#if defined(TOOLKIT_GTK)
#include "ui/gfx/gtk_native_view_id_manager.h"
#endif

#if defined(OS_WIN)
#include "ui/surface/accelerated_surface_win.h"
#endif

namespace content {

bool GpuProcessHost::gpu_enabled_ = true;
bool GpuProcessHost::hardware_gpu_enabled_ = true;

namespace {

enum GPUProcessLifetimeEvent {
  LAUNCHED,
  DIED_FIRST_TIME,
  DIED_SECOND_TIME,
  DIED_THIRD_TIME,
  DIED_FOURTH_TIME,
  GPU_PROCESS_LIFETIME_EVENT_MAX = 100
};

// Indexed by GpuProcessKind. There is one of each kind maximum. This array may
// only be accessed from the IO thread.
GpuProcessHost* g_gpu_process_hosts[GpuProcessHost::GPU_PROCESS_KIND_COUNT];

#if defined(TOOLKIT_GTK)

void ReleasePermanentXIDDispatcher(gfx::PluginWindowHandle surface) {
  GtkNativeViewManager* manager = GtkNativeViewManager::GetInstance();
  manager->ReleasePermanentXID(surface);
}

#endif

void SendGpuProcessMessage(GpuProcessHost::GpuProcessKind kind,
                           CauseForGpuLaunch cause,
                           IPC::Message* message) {
  GpuProcessHost* host = GpuProcessHost::Get(kind, cause);
  if (host) {
    host->Send(message);
  } else {
    delete message;
  }
}

void AcceleratedSurfaceBuffersSwappedCompletedForGPU(int host_id,
                                                     int route_id,
                                                     bool alive,
                                                     uint64 surface_handle) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::IO)) {
    BrowserThread::PostTask(
        BrowserThread::IO,
        FROM_HERE,
        base::Bind(&AcceleratedSurfaceBuffersSwappedCompletedForGPU,
                   host_id,
                   route_id,
                   alive,
                   surface_handle));
    return;
  }

  GpuProcessHost* host = GpuProcessHost::FromID(host_id);
  if (host) {
    if (alive) {
      AcceleratedSurfaceMsg_BufferPresented_Params ack_params;
      ack_params.sync_point = 0;
      host->Send(
          new AcceleratedSurfaceMsg_BufferPresented(route_id, ack_params));
    } else {
      host->ForceShutdown();
    }
  }
}

#if defined(OS_WIN)
// This sends a ViewMsg_SwapBuffers_ACK directly to the renderer process
// (RenderWidget).
void AcceleratedSurfaceBuffersSwappedCompletedForRenderer(
    int surface_id,
    base::TimeTicks timebase,
    base::TimeDelta interval) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    BrowserThread::PostTask(
        BrowserThread::UI,
        FROM_HERE,
        base::Bind(&AcceleratedSurfaceBuffersSwappedCompletedForRenderer,
                   surface_id, timebase, interval));
    return;
  }

  int render_process_id = 0;
  int render_widget_id = 0;
  if (!GpuSurfaceTracker::Get()->GetRenderWidgetIDForSurface(
      surface_id, &render_process_id, &render_widget_id)) {
    return;
  }
  RenderProcessHost* host = RenderProcessHost::FromID(render_process_id);
  if (!host)
    return;
  RenderWidgetHost* rwh = host->GetRenderWidgetHostByID(render_widget_id);
  if (!rwh)
    return;
  RenderWidgetHostImpl::From(rwh)->AcknowledgeSwapBuffersToRenderer();
  if (interval != base::TimeDelta())
    RenderWidgetHostImpl::From(rwh)->UpdateVSyncParameters(timebase, interval);
}

void AcceleratedSurfaceBuffersSwappedCompleted(int host_id,
                                               int route_id,
                                               int surface_id,
                                               uint64 surface_handle,
                                               bool alive,
                                               base::TimeTicks timebase,
                                               base::TimeDelta interval) {
  AcceleratedSurfaceBuffersSwappedCompletedForGPU(host_id, route_id,
                                                  alive, surface_handle);
  AcceleratedSurfaceBuffersSwappedCompletedForRenderer(surface_id, timebase,
                                                       interval);
}
#endif  // defined(OS_WIN)

}  // anonymous namespace

#if defined(TOOLKIT_GTK)
// Used to put a lock on surfaces so that the window to which the GPU
// process is drawing to doesn't disappear while it is drawing when
// a WebContents is closed.
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
#endif  // defined(TOOLKIT_GTK)

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

  virtual ~GpuMainThread() {
    Stop();
  }

 protected:
  virtual void Init() OVERRIDE {
    if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kSingleProcess)) {
      child_thread_ = new GpuChildThread(channel_id_);
    } else {
      gpu_process_ = new GpuProcess();
      // The process object takes ownership of the thread object, so do not
      // save and delete the pointer.
      gpu_process_->set_main_thread(new GpuChildThread(channel_id_));
    }
  }

  virtual void CleanUp() OVERRIDE {
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

// static
bool GpuProcessHost::ValidateHost(GpuProcessHost* host) {
  if (!host)
    return false;

  // The Gpu process is invalid if it's not using software, the card is
  // blacklisted, and we can kill it and start over.
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kSingleProcess) ||
      CommandLine::ForCurrentProcess()->HasSwitch(switches::kInProcessGPU) ||
      (host->valid_ &&
       (host->software_rendering() ||
        !GpuDataManagerImpl::GetInstance()->ShouldUseSoftwareRendering()))) {
    return true;
  }

  host->ForceShutdown();
  return false;
}

// static
GpuProcessHost* GpuProcessHost::Get(GpuProcessKind kind,
                                    CauseForGpuLaunch cause) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  // Don't grant further access to GPU if it is not allowed.
  GpuDataManagerImpl* gpu_data_manager = GpuDataManagerImpl::GetInstance();
  DCHECK(gpu_data_manager);
  if (!gpu_data_manager->GpuAccessAllowed())
    return NULL;

  if (g_gpu_process_hosts[kind] && ValidateHost(g_gpu_process_hosts[kind]))
    return g_gpu_process_hosts[kind];

  if (cause == CAUSE_FOR_GPU_LAUNCH_NO_LAUNCH)
    return NULL;

  static int last_host_id = 0;
  int host_id;
  host_id = ++last_host_id;

  UMA_HISTOGRAM_ENUMERATION("GPU.GPUProcessLaunchCause",
                            cause,
                            CAUSE_FOR_GPU_LAUNCH_MAX_ENUM);

  GpuProcessHost* host = new GpuProcessHost(host_id, kind);
  if (host->Init())
    return host;

  delete host;
  return NULL;
}

// static
void GpuProcessHost::GetProcessHandles(
    const GpuDataManager::GetGpuProcessHandlesCallback& callback)  {
  if (!BrowserThread::CurrentlyOn(BrowserThread::IO)) {
    BrowserThread::PostTask(
        BrowserThread::IO,
        FROM_HERE,
        base::Bind(&GpuProcessHost::GetProcessHandles, callback));
    return;
  }
  std::list<base::ProcessHandle> handles;
  for (size_t i = 0; i < arraysize(g_gpu_process_hosts); ++i) {
    GpuProcessHost* host = g_gpu_process_hosts[i];
    if (host && ValidateHost(host))
      handles.push_back(host->process_->GetHandle());
  }
  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(callback, handles));
}

// static
void GpuProcessHost::SendOnIO(GpuProcessKind kind,
                              CauseForGpuLaunch cause,
                              IPC::Message* message) {
  if (!BrowserThread::PostTask(
          BrowserThread::IO, FROM_HERE,
          base::Bind(
              &SendGpuProcessMessage, kind, cause, message))) {
    delete message;
  }
}

// static
GpuProcessHost* GpuProcessHost::FromID(int host_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  for (int i = 0; i < GPU_PROCESS_KIND_COUNT; ++i) {
    GpuProcessHost* host = g_gpu_process_hosts[i];
    if (host && host->host_id_ == host_id && ValidateHost(host))
      return host;
  }

  return NULL;
}

GpuProcessHost::GpuProcessHost(int host_id, GpuProcessKind kind)
    : host_id_(host_id),
      valid_(true),
      in_process_(false),
      software_rendering_(false),
      kind_(kind),
      process_launched_(false) {
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kSingleProcess) ||
      CommandLine::ForCurrentProcess()->HasSwitch(switches::kInProcessGPU)) {
    in_process_ = true;
  }

  // If the 'single GPU process' policy ever changes, we still want to maintain
  // it for 'gpu thread' mode and only create one instance of host and thread.
  DCHECK(!in_process_ || g_gpu_process_hosts[kind] == NULL);

  g_gpu_process_hosts[kind] = this;

  // Post a task to create the corresponding GpuProcessHostUIShim.  The
  // GpuProcessHostUIShim will be destroyed if either the browser exits,
  // in which case it calls GpuProcessHostUIShim::DestroyAll, or the
  // GpuProcessHost is destroyed, which happens when the corresponding GPU
  // process terminates or fails to launch.
  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(base::IgnoreResult(&GpuProcessHostUIShim::Create), host_id));

  process_.reset(new BrowserChildProcessHostImpl(PROCESS_TYPE_GPU, this));
}

GpuProcessHost::~GpuProcessHost() {
  DCHECK(CalledOnValidThread());

  SendOutstandingReplies();

  // Maximum number of times the gpu process is allowed to crash in a session.
  // Once this limit is reached, any request to launch the gpu process will
  // fail.
  const int kGpuMaxCrashCount = 3;

  // Number of times the gpu process has crashed in the current browser session.
  static int gpu_crash_count = 0;
  static int gpu_recent_crash_count = 0;
  static base::Time last_gpu_crash_time;
  static bool crashed_before = false;
  static int gpu_software_crash_count = 0;

  // Ending only acts as a failure if the GPU process was actually started and
  // was intended for actual rendering (and not just checking caps or other
  // options).
  if (process_launched_ && kind_ == GPU_PROCESS_KIND_SANDBOXED) {
    if (software_rendering_) {
      UMA_HISTOGRAM_ENUMERATION("GPU.SoftwareRendererLifetimeEvents",
                                DIED_FIRST_TIME + gpu_software_crash_count,
                                GPU_PROCESS_LIFETIME_EVENT_MAX);

      if (++gpu_software_crash_count >= kGpuMaxCrashCount) {
        // The software renderer is too unstable to use. Disable it for current
        // session.
        gpu_enabled_ = false;
      }
    } else {
      ++gpu_crash_count;
      UMA_HISTOGRAM_ENUMERATION("GPU.GPUProcessLifetimeEvents",
                                std::min(DIED_FIRST_TIME + gpu_crash_count,
                                         GPU_PROCESS_LIFETIME_EVENT_MAX - 1),
                                GPU_PROCESS_LIFETIME_EVENT_MAX);

      // Allow about 1 GPU crash per hour to be removed from the crash count,
      // so very occasional crashes won't eventually add up and prevent the
      // GPU process from launching.
      ++gpu_recent_crash_count;
      base::Time current_time = base::Time::Now();
      if (crashed_before) {
        int hours_different = (current_time - last_gpu_crash_time).InHours();
        gpu_recent_crash_count =
            std::max(0, gpu_recent_crash_count - hours_different);
      }

      crashed_before = true;
      last_gpu_crash_time = current_time;

      if (gpu_recent_crash_count >= kGpuMaxCrashCount) {
#if !defined(OS_CHROMEOS)
        // The gpu process is too unstable to use. Disable it for current
        // session.
        hardware_gpu_enabled_ = false;
        GpuDataManagerImpl::GetInstance()->BlacklistCard();
#endif
      }
    }
  }

  int exit_code;
  base::TerminationStatus status = process_->GetTerminationStatus(&exit_code);
  UMA_HISTOGRAM_ENUMERATION("GPU.GPUProcessTerminationStatus",
                            status,
                            base::TERMINATION_STATUS_MAX_ENUM);

  UMA_HISTOGRAM_COUNTS_100("GPU.AtExitSurfaceCount",
                           GpuSurfaceTracker::Get()->GetSurfaceCount());
  UMA_HISTOGRAM_COUNTS_100("GPU.AtExitWindowCount",
                           uma_memory_stats_.window_count);
  UMA_HISTOGRAM_CUSTOM_COUNTS(
      "GPU.AtExitMBytesAllocated",
      uma_memory_stats_.bytes_allocated_current / 1024 / 1024, 1, 2000, 50);
  UMA_HISTOGRAM_CUSTOM_COUNTS(
      "GPU.AtExitMBytesAllocatedMax",
      uma_memory_stats_.bytes_allocated_max / 1024 / 1024, 1, 2000, 50);
  UMA_HISTOGRAM_CUSTOM_COUNTS(
      "GPU.AtExitMBytesLimit",
      uma_memory_stats_.bytes_limit / 1024 / 1024, 1, 2000, 50);

  if (status == base::TERMINATION_STATUS_NORMAL_TERMINATION ||
      status == base::TERMINATION_STATUS_ABNORMAL_TERMINATION) {
    UMA_HISTOGRAM_ENUMERATION("GPU.GPUProcessExitCode",
                              exit_code,
                              RESULT_CODE_LAST_CODE);
  }

  // In case we never started, clean up.
  while (!queued_messages_.empty()) {
    delete queued_messages_.front();
    queued_messages_.pop();
  }

  // This is only called on the IO thread so no race against the constructor
  // for another GpuProcessHost.
  if (g_gpu_process_hosts[kind_] == this)
    g_gpu_process_hosts[kind_] = NULL;

  // If there are any remaining offscreen contexts at the point the
  // GPU process exits, assume something went wrong, and block their
  // URLs from accessing client 3D APIs without prompting.
  BlockLiveOffscreenContexts();

  BrowserThread::PostTask(BrowserThread::UI,
                          FROM_HERE,
                          base::Bind(&GpuProcessHostUIShim::Destroy, host_id_));
}

bool GpuProcessHost::Init() {
  init_start_time_ = base::TimeTicks::Now();

  TRACE_EVENT_INSTANT0("gpu", "LaunchGpuProcess");

  std::string channel_id = process_->GetHost()->CreateChannel();
  if (channel_id.empty())
    return false;

  if (in_process_) {
    CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kDisableGpuWatchdog);

    in_process_gpu_thread_.reset(new GpuMainThread(channel_id));
    in_process_gpu_thread_->Start();

    OnProcessLaunched();  // Fake a callback that the process is ready.
  } else if (!LaunchGpuProcess(channel_id)) {
    return false;
  }

  if (!Send(new GpuMsg_Initialize()))
    return false;

  return Send(new GpuMsg_SetVideoMemoryWindowCount(
      GpuDataManagerImpl::GetInstance()->GetWindowCount()));
}

void GpuProcessHost::RouteOnUIThread(const IPC::Message& message) {
  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(&RouteToGpuProcessHostUIShimTask, host_id_, message));
}

bool GpuProcessHost::Send(IPC::Message* msg) {
  DCHECK(CalledOnValidThread());
  if (process_->GetHost()->IsChannelOpening()) {
    queued_messages_.push(msg);
    return true;
  }

  bool result = process_->Send(msg);
  if (!result) {
    // Channel is hosed, but we may not get destroyed for a while. Send
    // outstanding channel creation failures now so that the caller can restart
    // with a new process/channel without waiting.
    SendOutstandingReplies();
  }
  return result;
}

void GpuProcessHost::AddFilter(IPC::ChannelProxy::MessageFilter* filter) {
  DCHECK(CalledOnValidThread());
  process_->GetHost()->AddFilter(filter);
}

bool GpuProcessHost::OnMessageReceived(const IPC::Message& message) {
  DCHECK(CalledOnValidThread());
  IPC_BEGIN_MESSAGE_MAP(GpuProcessHost, message)
    IPC_MESSAGE_HANDLER(GpuHostMsg_Initialized, OnInitialized)
    IPC_MESSAGE_HANDLER(GpuHostMsg_ChannelEstablished, OnChannelEstablished)
    IPC_MESSAGE_HANDLER(GpuHostMsg_CommandBufferCreated, OnCommandBufferCreated)
    IPC_MESSAGE_HANDLER(GpuHostMsg_DestroyCommandBuffer, OnDestroyCommandBuffer)
    IPC_MESSAGE_HANDLER(GpuHostMsg_ImageCreated, OnImageCreated)
    IPC_MESSAGE_HANDLER(GpuHostMsg_DidCreateOffscreenContext,
                        OnDidCreateOffscreenContext)
    IPC_MESSAGE_HANDLER(GpuHostMsg_DidLoseContext, OnDidLoseContext)
    IPC_MESSAGE_HANDLER(GpuHostMsg_DidDestroyOffscreenContext,
                        OnDidDestroyOffscreenContext)
    IPC_MESSAGE_HANDLER(GpuHostMsg_GpuMemoryUmaStats,
                        OnGpuMemoryUmaStatsReceived)
#if defined(OS_MACOSX)
    IPC_MESSAGE_HANDLER(GpuHostMsg_AcceleratedSurfaceBuffersSwapped,
                        OnAcceleratedSurfaceBuffersSwapped)
#endif
#if defined(OS_WIN)
    IPC_MESSAGE_HANDLER(GpuHostMsg_AcceleratedSurfaceBuffersSwapped,
                        OnAcceleratedSurfaceBuffersSwapped)
    IPC_MESSAGE_HANDLER(GpuHostMsg_AcceleratedSurfacePostSubBuffer,
                        OnAcceleratedSurfacePostSubBuffer)
    IPC_MESSAGE_HANDLER(GpuHostMsg_AcceleratedSurfaceSuspend,
                        OnAcceleratedSurfaceSuspend)
    IPC_MESSAGE_HANDLER(GpuHostMsg_AcceleratedSurfaceRelease,
                        OnAcceleratedSurfaceRelease)
#endif
    IPC_MESSAGE_UNHANDLED(RouteOnUIThread(message))
  IPC_END_MESSAGE_MAP()

  return true;
}

void GpuProcessHost::OnChannelConnected(int32 peer_pid) {
  TRACE_EVENT0("gpu", "GpuProcessHostUIShim::OnChannelConnected");

  while (!queued_messages_.empty()) {
    Send(queued_messages_.front());
    queued_messages_.pop();
  }
}

void GpuProcessHost::EstablishGpuChannel(
    int client_id,
    bool share_context,
    const EstablishChannelCallback& callback) {
  DCHECK(CalledOnValidThread());
  TRACE_EVENT0("gpu", "GpuProcessHostUIShim::EstablishGpuChannel");

  // If GPU features are already blacklisted, no need to establish the channel.
  if (!GpuDataManagerImpl::GetInstance()->GpuAccessAllowed()) {
    callback.Run(IPC::ChannelHandle(), GPUInfo());
    return;
  }

  if (Send(new GpuMsg_EstablishChannel(client_id, share_context))) {
    channel_requests_.push(callback);
  } else {
    callback.Run(IPC::ChannelHandle(), GPUInfo());
  }
}

void GpuProcessHost::CreateViewCommandBuffer(
    const gfx::GLSurfaceHandle& compositing_surface,
    int surface_id,
    int client_id,
    const GPUCreateCommandBufferConfig& init_params,
    const CreateCommandBufferCallback& callback) {
  TRACE_EVENT0("gpu", "GpuProcessHostUIShim::CreateViewCommandBuffer");

  DCHECK(CalledOnValidThread());

#if defined(TOOLKIT_GTK)
  // There should only be one such command buffer (for the compositor).  In
  // practice, if the GPU process lost a context, GraphicsContext3D with
  // associated command buffer and view surface will not be gone until new
  // one is in place and all layers are reattached.
  linked_ptr<SurfaceRef> surface_ref;
  SurfaceRefMap::iterator it = surface_refs_.find(surface_id);
  if (it != surface_refs_.end())
    surface_ref = (*it).second;
  else
    surface_ref.reset(new SurfaceRef(compositing_surface.handle));
#endif  // defined(TOOLKIT_GTK)

  if (!compositing_surface.is_null() &&
      Send(new GpuMsg_CreateViewCommandBuffer(
          compositing_surface, surface_id, client_id, init_params))) {
    create_command_buffer_requests_.push(callback);
#if defined(TOOLKIT_GTK)
    surface_refs_.insert(std::make_pair(surface_id, surface_ref));
#endif
  } else {
    callback.Run(MSG_ROUTING_NONE);
  }
}

void GpuProcessHost::CreateImage(gfx::PluginWindowHandle window,
                                 int client_id,
                                 int image_id,
                                 const CreateImageCallback& callback) {
  TRACE_EVENT0("gpu", "GpuProcessHostUIShim::CreateImage");

  DCHECK(CalledOnValidThread());

  if (Send(new GpuMsg_CreateImage(window, client_id, image_id))) {
    create_image_requests_.push(callback);
  } else {
    callback.Run(gfx::Size());
  }
}

void GpuProcessHost::DeleteImage(int client_id,
                                 int image_id,
                                 int sync_point) {
  TRACE_EVENT0("gpu", "GpuProcessHostUIShim::DeleteImage");

  DCHECK(CalledOnValidThread());

  Send(new GpuMsg_DeleteImage(client_id, image_id, sync_point));
}

void GpuProcessHost::OnInitialized(bool result) {
  UMA_HISTOGRAM_BOOLEAN("GPU.GPUProcessInitialized", result);
}

void GpuProcessHost::OnChannelEstablished(
    const IPC::ChannelHandle& channel_handle) {
  TRACE_EVENT0("gpu", "GpuProcessHostUIShim::OnChannelEstablished");

  EstablishChannelCallback callback = channel_requests_.front();
  channel_requests_.pop();

  // Currently if any of the GPU features are blacklisted, we don't establish a
  // GPU channel.
  if (!channel_handle.name.empty() &&
      !GpuDataManagerImpl::GetInstance()->GpuAccessAllowed()) {
    Send(new GpuMsg_CloseChannel(channel_handle));
    callback.Run(IPC::ChannelHandle(), GPUInfo());
    RouteOnUIThread(GpuHostMsg_OnLogMessage(
        logging::LOG_WARNING,
        "WARNING",
        "Hardware acceleration is unavailable."));
    return;
  }

  callback.Run(channel_handle,
               GpuDataManagerImpl::GetInstance()->GetGPUInfo());
}

void GpuProcessHost::OnCommandBufferCreated(const int32 route_id) {
  TRACE_EVENT0("gpu", "GpuProcessHostUIShim::OnCommandBufferCreated");

  if (create_command_buffer_requests_.empty())
    return;

  CreateCommandBufferCallback callback =
      create_command_buffer_requests_.front();
  create_command_buffer_requests_.pop();
  callback.Run(route_id);
}

void GpuProcessHost::OnDestroyCommandBuffer(int32 surface_id) {
  TRACE_EVENT0("gpu", "GpuProcessHostUIShim::OnDestroyCommandBuffer");

#if defined(TOOLKIT_GTK)
  SurfaceRefMap::iterator it = surface_refs_.find(surface_id);
  if (it != surface_refs_.end())
    surface_refs_.erase(it);
#endif  // defined(TOOLKIT_GTK)
}

void GpuProcessHost::OnImageCreated(const gfx::Size size) {
  TRACE_EVENT0("gpu", "GpuProcessHost::OnImageCreated");

  if (create_image_requests_.empty())
    return;

  CreateImageCallback callback = create_image_requests_.front();
  create_image_requests_.pop();
  callback.Run(size);
}

void GpuProcessHost::OnDidCreateOffscreenContext(const GURL& url) {
  urls_with_live_offscreen_contexts_.insert(url);
}

void GpuProcessHost::OnDidLoseContext(bool offscreen,
                                      gpu::error::ContextLostReason reason,
                                      const GURL& url) {
  // TODO(kbr): would be nice to see the "offscreen" flag too.
  TRACE_EVENT2("gpu", "GpuProcessHost::OnDidLoseContext",
               "reason", reason,
               "url",
               url.possibly_invalid_spec());

  if (!offscreen || url.is_empty()) {
    // Assume that the loss of the compositor's or accelerated canvas'
    // context is a serious event and blame the loss on all live
    // offscreen contexts. This more robustly handles situations where
    // the GPU process may not actually detect the context loss in the
    // offscreen context.
    BlockLiveOffscreenContexts();
    return;
  }

  GpuDataManagerImpl::DomainGuilt guilt;
  switch (reason) {
    case gpu::error::kGuilty:
      guilt = GpuDataManagerImpl::DOMAIN_GUILT_KNOWN;
      break;
    case gpu::error::kUnknown:
      guilt = GpuDataManagerImpl::DOMAIN_GUILT_UNKNOWN;
      break;
    case gpu::error::kInnocent:
      return;
    default:
      NOTREACHED();
      return;
  }

  GpuDataManagerImpl::GetInstance()->BlockDomainFrom3DAPIs(url, guilt);
}

void GpuProcessHost::OnDidDestroyOffscreenContext(const GURL& url) {
  urls_with_live_offscreen_contexts_.erase(url);
}

void GpuProcessHost::OnGpuMemoryUmaStatsReceived(
    const GPUMemoryUmaStats& stats) {
  TRACE_EVENT0("gpu", "GpuProcessHost::OnGpuMemoryUmaStatsReceived");
  uma_memory_stats_ = stats;
}

#if defined(OS_MACOSX)
void GpuProcessHost::OnAcceleratedSurfaceBuffersSwapped(
    const GpuHostMsg_AcceleratedSurfaceBuffersSwapped_Params& params) {
  TRACE_EVENT0("gpu", "GpuProcessHost::OnAcceleratedSurfaceBuffersSwapped");

  gfx::PluginWindowHandle handle =
      GpuSurfaceTracker::Get()->GetSurfaceWindowHandle(params.surface_id);
  // Compositor window is always gfx::kNullPluginWindow.
  // TODO(jbates) http://crbug.com/105344 This will be removed when there are no
  // plugin windows.
  if (handle != gfx::kNullPluginWindow) {
    RouteOnUIThread(GpuHostMsg_AcceleratedSurfaceBuffersSwapped(params));
    return;
  }

  base::ScopedClosureRunner scoped_completion_runner(
      base::Bind(&AcceleratedSurfaceBuffersSwappedCompletedForGPU,
                 host_id_, params.route_id,
                 true /* alive */, params.surface_handle));

  int render_process_id = 0;
  int render_widget_id = 0;
  if (!GpuSurfaceTracker::Get()->GetRenderWidgetIDForSurface(
      params.surface_id, &render_process_id, &render_widget_id)) {
    return;
  }
  RenderWidgetHelper* helper =
      RenderWidgetHelper::FromProcessHostID(render_process_id);
  if (!helper)
    return;

  // Pass the SwapBuffers on to the RenderWidgetHelper to wake up the UI thread
  // if the browser is waiting for a new frame. Otherwise the RenderWidgetHelper
  // will forward to the RenderWidgetHostView via RenderProcessHostImpl and
  // RenderWidgetHostImpl.
  scoped_completion_runner.Release();
  helper->DidReceiveBackingStoreMsg(ViewHostMsg_CompositorSurfaceBuffersSwapped(
      render_widget_id,
      params.surface_id,
      params.surface_handle,
      params.route_id,
      params.size,
      host_id_));
}
#endif  // OS_MACOSX

#if defined(OS_WIN)
void GpuProcessHost::OnAcceleratedSurfaceBuffersSwapped(
    const GpuHostMsg_AcceleratedSurfaceBuffersSwapped_Params& params) {
  TRACE_EVENT0("gpu", "GpuProcessHost::OnAcceleratedSurfaceBuffersSwapped");

  base::ScopedClosureRunner scoped_completion_runner(
      base::Bind(&AcceleratedSurfaceBuffersSwappedCompleted,
          host_id_, params.route_id, params.surface_id, params.surface_handle,
          true, base::TimeTicks(), base::TimeDelta()));

  gfx::PluginWindowHandle handle =
      GpuSurfaceTracker::Get()->GetSurfaceWindowHandle(params.surface_id);

  if (!handle) {
    TRACE_EVENT1("gpu", "SurfaceIDNotFound_RoutingToUI",
                 "surface_id", params.surface_id);
#if defined(USE_AURA)
    // This is a content area swap, send it on to the UI thread.
    scoped_completion_runner.Release();
    RouteOnUIThread(GpuHostMsg_AcceleratedSurfaceBuffersSwapped(params));
#endif
    return;
  }

  // Otherwise it's the UI swap.

  scoped_refptr<AcceleratedPresenter> presenter(
      AcceleratedPresenter::GetForWindow(handle));
  if (!presenter) {
    TRACE_EVENT1("gpu", "EarlyOut_NativeWindowNotFound", "handle", handle);
    return;
  }

  scoped_completion_runner.Release();
  presenter->AsyncPresentAndAcknowledge(
      params.size,
      params.surface_handle,
      base::Bind(&AcceleratedSurfaceBuffersSwappedCompleted,
                 host_id_,
                 params.route_id,
                 params.surface_id,
                 params.surface_handle));
}

void GpuProcessHost::OnAcceleratedSurfacePostSubBuffer(
    const GpuHostMsg_AcceleratedSurfacePostSubBuffer_Params& params) {
  TRACE_EVENT0("gpu", "GpuProcessHost::OnAcceleratedSurfacePostSubBuffer");

  NOTIMPLEMENTED();
}

void GpuProcessHost::OnAcceleratedSurfaceSuspend(int32 surface_id) {
  TRACE_EVENT0("gpu", "GpuProcessHost::OnAcceleratedSurfaceSuspend");

  gfx::PluginWindowHandle handle =
      GpuSurfaceTracker::Get()->GetSurfaceWindowHandle(surface_id);

  if (!handle) {
#if defined(USE_AURA)
    RouteOnUIThread(GpuHostMsg_AcceleratedSurfaceSuspend(surface_id));
#endif
    return;
  }

  scoped_refptr<AcceleratedPresenter> presenter(
      AcceleratedPresenter::GetForWindow(handle));
  if (!presenter)
    return;

  presenter->Suspend();
}

void GpuProcessHost::OnAcceleratedSurfaceRelease(
    const GpuHostMsg_AcceleratedSurfaceRelease_Params& params) {
  TRACE_EVENT0("gpu", "GpuProcessHost::OnAcceleratedSurfaceRelease");

  gfx::PluginWindowHandle handle =
      GpuSurfaceTracker::Get()->GetSurfaceWindowHandle(params.surface_id);
  if (!handle) {
#if defined(USE_AURA)
    RouteOnUIThread(GpuHostMsg_AcceleratedSurfaceRelease(params));
    return;
#endif
  }

  scoped_refptr<AcceleratedPresenter> presenter(
      AcceleratedPresenter::GetForWindow(handle));
  if (!presenter)
    return;

  presenter->ReleaseSurface();
}

#endif  // OS_WIN

void GpuProcessHost::OnProcessLaunched() {
  UMA_HISTOGRAM_TIMES("GPU.GPUProcessLaunchTime",
                      base::TimeTicks::Now() - init_start_time_);
}

void GpuProcessHost::OnProcessCrashed(int exit_code) {
  SendOutstandingReplies();
}

bool GpuProcessHost::software_rendering() {
  return software_rendering_;
}

GpuProcessHost::GpuProcessKind GpuProcessHost::kind() {
  return kind_;
}

void GpuProcessHost::ForceShutdown() {
  // This is only called on the IO thread so no race against the constructor
  // for another GpuProcessHost.
  if (g_gpu_process_hosts[kind_] == this)
    g_gpu_process_hosts[kind_] = NULL;

  process_->ForceShutdown();
}

bool GpuProcessHost::LaunchGpuProcess(const std::string& channel_id) {
  if (!(gpu_enabled_ &&
      GpuDataManagerImpl::GetInstance()->ShouldUseSoftwareRendering()) &&
      !hardware_gpu_enabled_) {
    SendOutstandingReplies();
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

  if (kind_ == GPU_PROCESS_KIND_UNSANDBOXED)
    cmd_line->AppendSwitch(switches::kDisableGpuSandbox);

  // Propagate relevant command line switches.
  static const char* const kSwitchNames[] = {
    switches::kCrashOnGpuHang,
    switches::kDisableAcceleratedVideoDecode,
    switches::kDisableBreakpad,
    switches::kDisableGLMultisampling,
    switches::kDisableGpuSandbox,
    switches::kDisableGpuVsync,
    switches::kDisableGpuWatchdog,
    switches::kDisableImageTransportSurface,
    switches::kDisableLogging,
    switches::kDisableSeccompFilterSandbox,
    switches::kEnableGpuSandbox,
    switches::kEnableGPUServiceLogging,
    switches::kEnableLogging,
    switches::kEnableVirtualGLContexts,
    switches::kGpuNoContextLost,
    switches::kGpuStartupDialog,
    switches::kGpuSwitching,
    switches::kLoggingLevel,
    switches::kNoSandbox,
    switches::kReduceGpuSandbox,
    switches::kTestGLLib,
    switches::kTraceStartup,
    switches::kV,
    switches::kVModule,
#if defined(OS_MACOSX)
    switches::kEnableSandboxLogging,
#endif
#if defined(USE_AURA)
    switches::kUIPrioritizeInGpuProcess,
#endif
    switches::kUseExynosVda,
  };
  cmd_line->CopySwitchesFrom(browser_command_line, kSwitchNames,
                             arraysize(kSwitchNames));
  cmd_line->CopySwitchesFrom(
      browser_command_line, switches::kGpuSwitches, switches::kNumGpuSwitches);

  GetContentClient()->browser()->AppendExtraCommandLineSwitches(
      cmd_line, process_->GetData().id);

  GpuDataManagerImpl::GetInstance()->AppendGpuCommandLine(cmd_line);

  if (cmd_line->HasSwitch(switches::kUseGL)) {
    software_rendering_ =
        (cmd_line->GetSwitchValueASCII(switches::kUseGL) == "swiftshader");
  }

  UMA_HISTOGRAM_BOOLEAN("GPU.GPUProcessSoftwareRendering", software_rendering_);

#if defined(OS_WIN)
  // Make GoogleDesktopNetwork3.dll think that the GPU process is a renderer
  // process so the DLL unloads itself. http://crbug/129884
  cmd_line->AppendSwitchASCII("ignored", " --type=renderer ");
#endif

  // If specified, prepend a launcher program to the command line.
  if (!gpu_launcher.empty())
    cmd_line->PrependWrapper(gpu_launcher);

  process_->Launch(
#if defined(OS_WIN)
      FilePath(),
#elif defined(OS_POSIX)
      false,
      base::EnvironmentVector(),
#endif
      cmd_line);
  process_launched_ = true;

  UMA_HISTOGRAM_ENUMERATION("GPU.GPUProcessLifetimeEvents",
                            LAUNCHED, GPU_PROCESS_LIFETIME_EVENT_MAX);
  return true;
}

void GpuProcessHost::SendOutstandingReplies() {
  valid_ = false;
  // First send empty channel handles for all EstablishChannel requests.
  while (!channel_requests_.empty()) {
    EstablishChannelCallback callback = channel_requests_.front();
    channel_requests_.pop();
    callback.Run(IPC::ChannelHandle(), GPUInfo());
  }
}

void GpuProcessHost::BlockLiveOffscreenContexts() {
  for (std::multiset<GURL>::iterator iter =
           urls_with_live_offscreen_contexts_.begin();
       iter != urls_with_live_offscreen_contexts_.end(); ++iter) {
    GpuDataManagerImpl::GetInstance()->BlockDomainFrom3DAPIs(
        *iter, GpuDataManagerImpl::DOMAIN_GUILT_UNKNOWN);
  }
}

}  // namespace content
