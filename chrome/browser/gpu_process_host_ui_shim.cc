// TODO(jam): move this file to src/content once we have an interface that the
// embedder provides.  We can then use it to get the resource and resize the
// window.
// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gpu_process_host_ui_shim.h"

#include "base/id_map.h"
#include "chrome/common/gpu_messages.h"
#include "chrome/browser/gpu_data_manager.h"
#include "content/browser/browser_thread.h"
#include "content/browser/gpu_process_host.h"
#include "content/browser/renderer_host/render_process_host.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/renderer_host/render_widget_host_view.h"

#if defined(OS_LINUX)
// These two #includes need to come after gpu_messages.h.
#include <gdk/gdkwindow.h>  // NOLINT
#include <gdk/gdkx.h>  // NOLINT
#include "ui/base/x/x11_util.h"
#include "ui/gfx/gtk_native_view_id_manager.h"
#include "ui/gfx/size.h"
#endif  // defined(OS_LINUX)

// Tasks used by this file
namespace {

int g_last_host_id = 0;
IDMap<GpuProcessHostUIShim> g_hosts_by_id;

class SendOnIOThreadTask : public Task {
 public:
  SendOnIOThreadTask(int host_id, IPC::Message* msg)
      : host_id_(host_id),
        msg_(msg) {
  }

 private:
  void Run() {
    GpuProcessHost* host = GpuProcessHost::FromID(host_id_);
    if (host)
      host->Send(msg_);
    else
      delete msg_;
  }

  int host_id_;
  IPC::Message* msg_;
};

}  // namespace

class GpuProcessHostUIShim::ViewSurface {
 public:
  explicit ViewSurface(ViewID view_id);
  ~ViewSurface();
  gfx::PluginWindowHandle surface() { return surface_; }
 private:
  RenderWidgetHostView* GetRenderWidgetHostView();
  ViewID view_id_;
  gfx::PluginWindowHandle surface_;
};

GpuProcessHostUIShim::ViewSurface::ViewSurface(ViewID view_id)
    : view_id_(view_id), surface_(gfx::kNullPluginWindow) {
  RenderWidgetHostView* view = GetRenderWidgetHostView();
  if (view)
    surface_ = view->AcquireCompositingSurface();
}

GpuProcessHostUIShim::ViewSurface::~ViewSurface() {
  if (!surface_)
    return;

  RenderWidgetHostView* view = GetRenderWidgetHostView();
  if (view)
    view->ReleaseCompositingSurface(surface_);
}

// We do separate lookups for the RenderWidgetHostView when acquiring
// and releasing surfaces (rather than caching) because the
// RenderWidgetHostView could die without warning. In such a case,
// it's the RenderWidgetHostView's responsibility to cleanup.
RenderWidgetHostView* GpuProcessHostUIShim::ViewSurface::
    GetRenderWidgetHostView() {
  RenderProcessHost* process = RenderProcessHost::FromID(view_id_.first);
  RenderWidgetHost* host = NULL;
  if (process) {
    host = static_cast<RenderWidgetHost*>(
        process->GetListenerByID(view_id_.second));
  }

  RenderWidgetHostView* view = NULL;
  if (host)
    view = host->view();

  return view;
}

GpuProcessHostUIShim::GpuProcessHostUIShim()
    : host_id_(++g_last_host_id),
      gpu_process_(NULL) {
  g_hosts_by_id.AddWithID(this, host_id_);
  gpu_data_manager_ = GpuDataManager::GetInstance();
  DCHECK(gpu_data_manager_);
}

// static
GpuProcessHostUIShim* GpuProcessHostUIShim::GetForRenderer(int renderer_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // The current policy is to ignore the renderer ID and use a single GPU
  // process for all renderers. Later this will be extended to allow the
  // use of multiple GPU processes.
  if (!g_hosts_by_id.IsEmpty()) {
    IDMap<GpuProcessHostUIShim>::iterator it(&g_hosts_by_id);
    return it.GetCurrentValue();
  }

  GpuProcessHostUIShim* ui_shim(new GpuProcessHostUIShim);

  // If Init succeeds, post a task to create the corresponding GpuProcessHost.
  // The GpuProcessHost will take ownership of the GpuProcessHostUIShim.
  BrowserThread::PostTask(BrowserThread::IO,
                          FROM_HERE,
                          NewRunnableFunction(&GpuProcessHost::Create,
                                              ui_shim->host_id_));

  return ui_shim;
}

// static
void GpuProcessHostUIShim::Destroy(int host_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  delete FromID(host_id);
}

// static
void GpuProcessHostUIShim::NotifyGpuProcessLaunched(
    int host_id,
    base::ProcessHandle gpu_process) {
  DCHECK(gpu_process);

  GpuProcessHostUIShim* ui_shim = FromID(host_id);
  DCHECK(ui_shim);

  ui_shim->gpu_process_ = gpu_process;
}

// static
GpuProcessHostUIShim* GpuProcessHostUIShim::FromID(int host_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (host_id == 0)
    return NULL;

  return g_hosts_by_id.Lookup(host_id);
}

bool GpuProcessHostUIShim::Send(IPC::Message* msg) {
  DCHECK(CalledOnValidThread());

  BrowserThread::PostTask(BrowserThread::IO,
                          FROM_HERE,
                          new SendOnIOThreadTask(host_id_, msg));
  return true;
}

// Post a Task to execute callbacks on a error conditions in order to
// clear the call stacks (and aid debugging).
namespace {

void EstablishChannelCallbackDispatcher(
    GpuProcessHostUIShim::EstablishChannelCallback* callback,
    const IPC::ChannelHandle& channel_handle,
    base::ProcessHandle renderer_process_for_gpu,
    const GPUInfo& gpu_info) {
  scoped_ptr<GpuProcessHostUIShim::EstablishChannelCallback>
    wrapped_callback(callback);
  wrapped_callback->Run(channel_handle, renderer_process_for_gpu, gpu_info);
}

void EstablishChannelError(
    GpuProcessHostUIShim::EstablishChannelCallback* callback,
    const IPC::ChannelHandle& channel_handle,
    base::ProcessHandle renderer_process_for_gpu,
    const GPUInfo& gpu_info) {
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      NewRunnableFunction(&EstablishChannelCallbackDispatcher,
                          callback,
                          channel_handle,
                          renderer_process_for_gpu,
                          gpu_info));
}

void SynchronizeCallbackDispatcher(
    GpuProcessHostUIShim::SynchronizeCallback* callback) {
  scoped_ptr<GpuProcessHostUIShim::SynchronizeCallback>
    wrapped_callback(callback);
  wrapped_callback->Run();
}

void SynchronizeError(
    GpuProcessHostUIShim::SynchronizeCallback* callback) {
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      NewRunnableFunction(&SynchronizeCallbackDispatcher, callback));
}

void CreateCommandBufferCallbackDispatcher(
    GpuProcessHostUIShim::CreateCommandBufferCallback* callback,
    int32 route_id) {
  scoped_ptr<GpuProcessHostUIShim::CreateCommandBufferCallback>
    wrapped_callback(callback);
  callback->Run(route_id);
}

void CreateCommandBufferError(
    GpuProcessHostUIShim::CreateCommandBufferCallback* callback,
    int32 route_id) {
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      NewRunnableFunction(&CreateCommandBufferCallbackDispatcher,
                          callback, route_id));
}

}  // namespace

void GpuProcessHostUIShim::SendOutstandingReplies() {
  // First send empty channel handles for all EstablishChannel requests.
  while (!channel_requests_.empty()) {
    linked_ptr<EstablishChannelCallback> callback = channel_requests_.front();
    channel_requests_.pop();
    EstablishChannelError(callback.release(),
                          IPC::ChannelHandle(),
                          NULL,
                          GPUInfo());
  }

  // Now unblock all renderers waiting for synchronization replies.
  while (!synchronize_requests_.empty()) {
    linked_ptr<SynchronizeCallback> callback = synchronize_requests_.front();
    synchronize_requests_.pop();
    SynchronizeError(callback.release());
  }
}

bool GpuProcessHostUIShim::OnMessageReceived(const IPC::Message& message) {
  DCHECK(CalledOnValidThread());

  if (message.routing_id() != MSG_ROUTING_CONTROL)
    return false;

  return OnControlMessageReceived(message);
}

void GpuProcessHostUIShim::EstablishGpuChannel(
    int renderer_id,
    EstablishChannelCallback *callback) {
  DCHECK(CalledOnValidThread());
  linked_ptr<EstablishChannelCallback> wrapped_callback(callback);

  // If GPU features are already blacklisted, no need to establish the channel.
  if (gpu_data_manager_->GetGpuFeatureFlags().flags() != 0) {
    EstablishChannelError(
        wrapped_callback.release(), IPC::ChannelHandle(), NULL, GPUInfo());
    return;
  }

  if (Send(new GpuMsg_EstablishChannel(renderer_id))) {
    channel_requests_.push(wrapped_callback);
  } else {
    EstablishChannelError(
        wrapped_callback.release(), IPC::ChannelHandle(), NULL, GPUInfo());
  }
}

void GpuProcessHostUIShim::Synchronize(SynchronizeCallback* callback) {
  DCHECK(CalledOnValidThread());
  linked_ptr<SynchronizeCallback> wrapped_callback(callback);

  if (Send(new GpuMsg_Synchronize())) {
    synchronize_requests_.push(wrapped_callback);
  } else {
    SynchronizeError(wrapped_callback.release());
  }
}

void GpuProcessHostUIShim::CreateViewCommandBuffer(
    int32 render_view_id,
    int32 renderer_id,
    const GPUCreateCommandBufferConfig& init_params,
    CreateCommandBufferCallback* callback) {
  DCHECK(CalledOnValidThread());
  linked_ptr<CreateCommandBufferCallback> wrapped_callback(callback);
  ViewID view_id(renderer_id, render_view_id);

  // We assume that there can only be one such command buffer (for the
  // compositor).
  if (acquired_surfaces_.count(view_id) != 0) {
    CreateCommandBufferError(wrapped_callback.release(), MSG_ROUTING_NONE);
    return;
  }

  linked_ptr<ViewSurface> view_surface(new ViewSurface(view_id));

  if (view_surface->surface() != gfx::kNullPluginWindow &&
      Send(new GpuMsg_CreateViewCommandBuffer(
          view_surface->surface(), render_view_id, renderer_id, init_params))) {
    create_command_buffer_requests_.push(wrapped_callback);
    acquired_surfaces_[view_id] = view_surface;
  } else {
    CreateCommandBufferError(wrapped_callback.release(), MSG_ROUTING_NONE);
  }
}

#if defined(OS_MACOSX)

void GpuProcessHostUIShim::DidDestroyAcceleratedSurface(int renderer_id,
                                                        int renderer_route_id) {
  Send(new GpuMsg_DidDestroyAcceleratedSurface(renderer_id, renderer_route_id));
}

#endif

void GpuProcessHostUIShim::CollectGpuInfoAsynchronously(
    GPUInfo::Level level) {
  DCHECK(CalledOnValidThread());

  // If GPU is already blacklisted, no more info will be collected.
  if (gpu_data_manager_->GetGpuFeatureFlags().flags() != 0)
    return;
  Send(new GpuMsg_CollectGraphicsInfo(level));
}

void GpuProcessHostUIShim::SendAboutGpuCrash() {
  DCHECK(CalledOnValidThread());
  Send(new GpuMsg_Crash());
}

void GpuProcessHostUIShim::SendAboutGpuHang() {
  DCHECK(CalledOnValidThread());
  Send(new GpuMsg_Hang());
}

const GPUInfo& GpuProcessHostUIShim::gpu_info() const {
  DCHECK(CalledOnValidThread());
  return gpu_data_manager_->gpu_info();
}

GpuProcessHostUIShim::~GpuProcessHostUIShim() {
  DCHECK(CalledOnValidThread());
  g_hosts_by_id.Remove(host_id_);

#if defined(OS_WIN)
  if (gpu_process_)
    CloseHandle(gpu_process_);
#endif
}

void GpuProcessHostUIShim::AddCustomLogMessage(int level,
    const std::string& header,
    const std::string& message) {
  OnLogMessage(level, header, message);
}

bool GpuProcessHostUIShim::OnControlMessageReceived(
    const IPC::Message& message) {
  DCHECK(CalledOnValidThread());

  IPC_BEGIN_MESSAGE_MAP(GpuProcessHostUIShim, message)
    IPC_MESSAGE_HANDLER(GpuHostMsg_ChannelEstablished,
                        OnChannelEstablished)
    IPC_MESSAGE_HANDLER(GpuHostMsg_CommandBufferCreated,
                        OnCommandBufferCreated)
    IPC_MESSAGE_HANDLER(GpuHostMsg_DestroyCommandBuffer,
                        OnDestroyCommandBuffer)
    IPC_MESSAGE_HANDLER(GpuHostMsg_GraphicsInfoCollected,
                        OnGraphicsInfoCollected)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(GpuHostMsg_PreliminaryGraphicsInfoCollected,
                                    OnPreliminaryGraphicsInfoCollected)
    IPC_MESSAGE_HANDLER(GpuHostMsg_OnLogMessage,
                        OnLogMessage)
    IPC_MESSAGE_HANDLER(GpuHostMsg_SynchronizeReply,
                        OnSynchronizeReply)
#if defined(OS_LINUX) && !defined(TOUCH_UI)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(GpuHostMsg_ResizeXID, OnResizeXID)
#elif defined(OS_MACOSX)
    IPC_MESSAGE_HANDLER(GpuHostMsg_AcceleratedSurfaceSetIOSurface,
                        OnAcceleratedSurfaceSetIOSurface)
    IPC_MESSAGE_HANDLER(GpuHostMsg_AcceleratedSurfaceBuffersSwapped,
                        OnAcceleratedSurfaceBuffersSwapped)
#elif defined(OS_WIN)
    IPC_MESSAGE_HANDLER(GpuHostMsg_ScheduleComposite, OnScheduleComposite);
#endif
    IPC_MESSAGE_UNHANDLED_ERROR()
  IPC_END_MESSAGE_MAP()

  return true;
}

void GpuProcessHostUIShim::OnChannelEstablished(
    const IPC::ChannelHandle& channel_handle,
    const GPUInfo& gpu_info) {
  gpu_data_manager_->UpdateGpuInfo(gpu_info);

  // The GPU process should have launched at this point and this object should
  // have been notified of its process handle.
  DCHECK(gpu_process_);

  linked_ptr<EstablishChannelCallback> callback = channel_requests_.front();
  channel_requests_.pop();

  // Currently if any of the GPU features are blacklisted, we don't establish a
  // GPU channel.
  if (channel_handle.name.size() != 0 &&
      gpu_data_manager_->GetGpuFeatureFlags().flags() != 0) {
    Send(new GpuMsg_CloseChannel(channel_handle));
    EstablishChannelError(callback.release(),
                          IPC::ChannelHandle(),
                          NULL,
                          gpu_info);
    AddCustomLogMessage(logging::LOG_WARNING, "WARNING",
        "Hardware acceleration is unavailable.");
    return;
  }

  callback->Run(channel_handle, gpu_process_, gpu_info);
}

void GpuProcessHostUIShim::OnSynchronizeReply() {
  // Guard against race conditions in abrupt GPU process termination.
  if (synchronize_requests_.size() > 0) {
    linked_ptr<SynchronizeCallback> callback(synchronize_requests_.front());
    synchronize_requests_.pop();
    callback->Run();
  }
}

void GpuProcessHostUIShim::OnCommandBufferCreated(const int32 route_id) {
  if (create_command_buffer_requests_.size() > 0) {
    linked_ptr<CreateCommandBufferCallback> callback =
        create_command_buffer_requests_.front();
    create_command_buffer_requests_.pop();
    if (route_id == MSG_ROUTING_NONE)
      CreateCommandBufferError(callback.release(), route_id);
    else
      callback->Run(route_id);
  }
}

void GpuProcessHostUIShim::OnDestroyCommandBuffer(
    gfx::PluginWindowHandle window, int32 renderer_id,
    int32 render_view_id) {
  ViewID view_id(renderer_id, render_view_id);
  acquired_surfaces_.erase(view_id);
}

void GpuProcessHostUIShim::OnGraphicsInfoCollected(const GPUInfo& gpu_info) {
  gpu_data_manager_->UpdateGpuInfo(gpu_info);
}

void GpuProcessHostUIShim::OnPreliminaryGraphicsInfoCollected(
    const GPUInfo& gpu_info, IPC::Message* reply_msg) {
  gpu_data_manager_->UpdateGpuInfo(gpu_info);
  GpuFeatureFlags flags = gpu_data_manager_->GetGpuFeatureFlags();

  GpuHostMsg_PreliminaryGraphicsInfoCollected::WriteReplyParams(
      reply_msg, flags.flags() != 0);
  Send(reply_msg);
}

void GpuProcessHostUIShim::OnLogMessage(int level,
    const std::string& header,
    const std::string& message) {
  DictionaryValue* dict = new DictionaryValue();
  dict->SetInteger("level", level);
  dict->SetString("header", header);
  dict->SetString("message", message);
  gpu_data_manager_->AddLogMessage(dict);
}

#if defined(OS_LINUX) && !defined(TOUCH_UI)

void GpuProcessHostUIShim::OnResizeXID(unsigned long xid, gfx::Size size,
                                       IPC::Message *reply_msg) {
  GdkWindow* window = reinterpret_cast<GdkWindow*>(gdk_xid_table_lookup(xid));
  if (window) {
    Display* display = GDK_WINDOW_XDISPLAY(window);
    gdk_window_resize(window, size.width(), size.height());
    XSync(display, False);
  }

  GpuHostMsg_ResizeXID::WriteReplyParams(reply_msg, (window != NULL));
  Send(reply_msg);
}

#elif defined(OS_MACOSX)

void GpuProcessHostUIShim::OnAcceleratedSurfaceSetIOSurface(
    const GpuHostMsg_AcceleratedSurfaceSetIOSurface_Params& params) {
  RenderViewHost* host = RenderViewHost::FromID(params.renderer_id,
                                                params.render_view_id);
  if (!host)
    return;
  RenderWidgetHostView* view = host->view();
  if (!view)
    return;
  view->AcceleratedSurfaceSetIOSurface(params.window,
                                       params.width,
                                       params.height,
                                       params.identifier);
}

void GpuProcessHostUIShim::OnAcceleratedSurfaceBuffersSwapped(
    const GpuHostMsg_AcceleratedSurfaceBuffersSwapped_Params& params) {
  RenderViewHost* host = RenderViewHost::FromID(params.renderer_id,
                                                params.render_view_id);
  if (!host)
    return;
  RenderWidgetHostView* view = host->view();
  if (!view)
    return;
  view->AcceleratedSurfaceBuffersSwapped(
      // Parameters needed to swap the IOSurface.
      params.window,
      params.surface_id,
      // Parameters needed to formulate an acknowledgment.
      params.renderer_id,
      params.route_id,
      host_id_,
      params.swap_buffers_count);
}

#elif defined(OS_WIN)

void GpuProcessHostUIShim::OnScheduleComposite(int renderer_id,
    int render_view_id) {
  RenderViewHost* host = RenderViewHost::FromID(renderer_id,
                                                render_view_id);
  if (!host) {
    return;
  }
  host->ScheduleComposite();
}

#endif

