// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gpu_process_host_ui_shim.h"

#include "base/command_line.h"
#include "base/metrics/histogram.h"
#include "chrome/browser/browser_thread.h"
#include "chrome/browser/gpu_blacklist.h"
#include "chrome/browser/gpu_process_host.h"
#include "chrome/browser/renderer_host/render_process_host.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/renderer_host/render_widget_host_view.h"
#include "chrome/common/child_process_logging.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/gpu_messages.h"
#include "grit/browser_resources.h"
#include "ui/base/resource/resource_bundle.h"

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

class SendOnIOThreadTask : public Task {
 public:
  explicit SendOnIOThreadTask(IPC::Message* msg) : msg_(msg) {
  }

 private:
  void Run() {
    GpuProcessHost::Get()->Send(msg_);
  }
  IPC::Message* msg_;
};

}  // namespace

GpuProcessHostUIShim::GpuProcessHostUIShim()
    : last_routing_id_(1),
      initialized_(false),
      initialized_successfully_(false),
      gpu_feature_flags_set_(false) {
}

GpuProcessHostUIShim::~GpuProcessHostUIShim() {
}

bool GpuProcessHostUIShim::EnsureInitialized() {
  if (!initialized_) {
    initialized_ = true;
    initialized_successfully_ = Init();
  }
  return initialized_successfully_;
}

bool GpuProcessHostUIShim::Init() {
  return LoadGpuBlacklist();
}

// static
GpuProcessHostUIShim* GpuProcessHostUIShim::GetInstance() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return Singleton<GpuProcessHostUIShim>::get();
}

bool GpuProcessHostUIShim::Send(IPC::Message* msg) {
  DCHECK(CalledOnValidThread());
  if (!EnsureInitialized())
    return false;

  BrowserThread::PostTask(BrowserThread::IO,
                          FROM_HERE,
                          new SendOnIOThreadTask(msg));
  return true;
}

// Post a Task to execute callbacks on a error conditions in order to
// clear the call stacks (and aid debugging).
namespace {

void EstablishChannelCallbackDispatcher(
    GpuProcessHostUIShim::EstablishChannelCallback* callback,
    const IPC::ChannelHandle& channel_handle,
    const GPUInfo& gpu_info) {
  scoped_ptr<GpuProcessHostUIShim::EstablishChannelCallback>
    wrapped_callback(callback);
  wrapped_callback->Run(channel_handle, gpu_info);
}

void EstablishChannelError(
    GpuProcessHostUIShim::EstablishChannelCallback* callback,
    const IPC::ChannelHandle& channel_handle,
    const GPUInfo& gpu_info) {
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      NewRunnableFunction(&EstablishChannelCallbackDispatcher,
                          callback, channel_handle, gpu_info));
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
    EstablishChannelError(callback.release(), IPC::ChannelHandle(), GPUInfo());
  }

  // Now unblock all renderers waiting for synchronization replies.
  while (!synchronize_requests_.empty()) {
    linked_ptr<SynchronizeCallback> callback = synchronize_requests_.front();
    synchronize_requests_.pop();
    SynchronizeError(callback.release());
  }
}

int32 GpuProcessHostUIShim::GetNextRoutingId() {
  DCHECK(CalledOnValidThread());
  return ++last_routing_id_;
}

void GpuProcessHostUIShim::AddRoute(int32 routing_id,
                                    IPC::Channel::Listener* listener) {
  DCHECK(CalledOnValidThread());
  router_.AddRoute(routing_id, listener);
}

void GpuProcessHostUIShim::RemoveRoute(int32 routing_id) {
  DCHECK(CalledOnValidThread());
  router_.RemoveRoute(routing_id);
}

bool GpuProcessHostUIShim::OnMessageReceived(const IPC::Message& message) {
  DCHECK(CalledOnValidThread());

  if (message.routing_id() == MSG_ROUTING_CONTROL)
    return OnControlMessageReceived(message);

  return router_.RouteMessage(message);
}

void GpuProcessHostUIShim::EstablishGpuChannel(
    int renderer_id, EstablishChannelCallback *callback) {
  DCHECK(CalledOnValidThread());
  linked_ptr<EstablishChannelCallback> wrapped_callback(callback);

  if (Send(new GpuMsg_EstablishChannel(renderer_id))) {
    channel_requests_.push(wrapped_callback);
  } else {
    EstablishChannelError(
        wrapped_callback.release(), IPC::ChannelHandle(), GPUInfo());
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

  gfx::PluginWindowHandle window = gfx::kNullPluginWindow;
  RenderProcessHost* process = RenderProcessHost::FromID(renderer_id);
  RenderWidgetHost* host = NULL;
  if (process) {
    host = static_cast<RenderWidgetHost*>(
        process->GetListenerByID(render_view_id));
  }

  RenderWidgetHostView* view = NULL;
  if (host)
    view = host->view();

  if (view) {
#if defined(OS_LINUX)
    gfx::NativeViewId view_id = NULL;
    view_id = gfx::IdFromNativeView(view->GetNativeView());

    // Lock the window that we will draw into.
    GtkNativeViewManager* manager = GtkNativeViewManager::GetInstance();
    if (!manager->GetPermanentXIDForId(&window, view_id)) {
      DLOG(ERROR) << "Can't find XID for view id " << view_id;
    }
#elif defined(OS_MACOSX)
    // On Mac OS X we currently pass a (fake) PluginWindowHandle for the
    // window that we draw to.
    window = view->AllocateFakePluginWindowHandle(
        /*opaque=*/true, /*root=*/true);
#elif defined(OS_WIN)
    // Create a window that we will overlay.
    window = view->GetCompositorHostWindow();
#endif
  }

  if (window != gfx::kNullPluginWindow &&
      Send(new GpuMsg_CreateViewCommandBuffer(
          window, render_view_id, renderer_id, init_params))) {
    create_command_buffer_requests_.push(wrapped_callback);
  } else {
    CreateCommandBufferError(wrapped_callback.release(), MSG_ROUTING_NONE);
  }
}

void GpuProcessHostUIShim::CollectGraphicsInfoAsynchronously(
    GPUInfo::Level level) {
  DCHECK(CalledOnValidThread());
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
  return gpu_info_;
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
    IPC_MESSAGE_HANDLER(GpuHostMsg_OnLogMessage,
                        OnLogMessage)
    IPC_MESSAGE_HANDLER(GpuHostMsg_SynchronizeReply,
                        OnSynchronizeReply)
#if defined(OS_LINUX)
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
  uint32 max_entry_id = gpu_blacklist_->max_entry_id();
  // max_entry_id can be zero if we failed to load the GPU blacklist, don't
  // bother with histograms then.
  if (channel_handle.name.size() != 0 && !gpu_feature_flags_set_ &&
      max_entry_id != 0)
  {
    gpu_feature_flags_set_ = true;

    const CommandLine& browser_command_line = *CommandLine::ForCurrentProcess();
    if (!browser_command_line.HasSwitch(switches::kIgnoreGpuBlacklist)) {
      gpu_feature_flags_ = gpu_blacklist_->DetermineGpuFeatureFlags(
          GpuBlacklist::kOsAny, NULL, gpu_info);

      if (gpu_feature_flags_.flags() != 0) {
        std::vector<uint32> flag_entries;
        gpu_blacklist_->GetGpuFeatureFlagEntries(
            GpuFeatureFlags::kGpuFeatureAll, flag_entries);
        DCHECK_GT(flag_entries.size(), 0u);
        for (size_t i = 0; i < flag_entries.size(); ++i) {
          UMA_HISTOGRAM_ENUMERATION("GPU.BlacklistTestResultsPerEntry",
              flag_entries[i], max_entry_id + 1);
        }
      } else {
        // id 0 is never used by any entry, so we use it here to indicate that
        // gpu is allowed.
        UMA_HISTOGRAM_ENUMERATION("GPU.BlacklistTestResultsPerEntry",
            0, max_entry_id + 1);
      }
    }
  }
  linked_ptr<EstablishChannelCallback> callback = channel_requests_.front();
  channel_requests_.pop();

  // Currently if any of the GPU features are blacklisted, we don't establish a
  // GPU channel.
  if (gpu_feature_flags_.flags() != 0) {
    Send(new GpuMsg_CloseChannel(channel_handle));
    EstablishChannelError(callback.release(), IPC::ChannelHandle(), gpu_info);
    AddCustomLogMessage(logging::LOG_WARNING, "WARNING", "GPU is blacklisted.");
  } else {
    callback->Run(channel_handle, gpu_info);
  }
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
  if (!window)
    return;

#if defined(OS_LINUX)
  GtkNativeViewManager* manager = GtkNativeViewManager::GetInstance();
  manager->ReleasePermanentXID(window);
#elif defined(OS_MACOSX) || defined(OS_WIN)
  RenderProcessHost* process = RenderProcessHost::FromID(renderer_id);
  RenderWidgetHost* host = NULL;
  if (process) {
    host = static_cast<RenderWidgetHost*>(
        process->GetListenerByID(render_view_id));
  }
  RenderWidgetHostView* view = NULL;
  if (host)
    view = host->view();

  if (view) {
#if defined(OS_MACOSX)
    view->DestroyFakePluginWindowHandle(window);
#elif defined(OS_WIN)
    view->ShowCompositorHostWindow(false);
#endif
  }
#endif  // defined(OS_MACOSX) || defined(OS_WIN)
}

void GpuProcessHostUIShim::OnGraphicsInfoCollected(const GPUInfo& gpu_info) {
  gpu_info_ = gpu_info;
  child_process_logging::SetGpuInfo(gpu_info);

  // Used only in testing.
  if (gpu_info_collected_callback_.get())
    gpu_info_collected_callback_->Run();
}

void GpuProcessHostUIShim::OnLogMessage(int level,
    const std::string& header,
    const std::string& message) {
  DictionaryValue* dict = new DictionaryValue();
  dict->SetInteger("level", level);
  dict->SetString("header", header);
  dict->SetString("message", message);
  log_messages_.Append(dict);
}

#if defined(OS_LINUX)

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

bool GpuProcessHostUIShim::LoadGpuBlacklist() {
  if (gpu_blacklist_.get() != NULL)
    return true;
  static const base::StringPiece gpu_blacklist_json(
      ResourceBundle::GetSharedInstance().GetRawDataResource(
          IDR_GPU_BLACKLIST));
  gpu_blacklist_.reset(new GpuBlacklist());
  if (gpu_blacklist_->LoadGpuBlacklist(gpu_blacklist_json.as_string(), true))
    return true;
  gpu_blacklist_.reset(NULL);
  return false;
}
