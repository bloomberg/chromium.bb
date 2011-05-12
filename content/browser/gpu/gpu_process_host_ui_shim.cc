// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/gpu/gpu_process_host_ui_shim.h"

#include "base/command_line.h"
#include "base/id_map.h"
#include "base/process_util.h"
#include "base/debug/trace_event.h"
#include "chrome/browser/browser_process.h"
#include "content/browser/browser_thread.h"
#include "content/browser/gpu/gpu_data_manager.h"
#include "content/browser/gpu/gpu_process_host.h"
#include "content/browser/renderer_host/render_process_host.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/renderer_host/render_widget_host_view.h"
#include "content/common/content_switches.h"
#include "content/common/gpu/gpu_messages.h"

#if defined(OS_LINUX)
// These two #includes need to come after gpu_messages.h.
#include <gdk/gdkwindow.h>  // NOLINT
#include <gdk/gdkx.h>  // NOLINT
#include "ui/base/x/x11_util.h"
#include "ui/gfx/gtk_native_view_id_manager.h"
#include "ui/gfx/size.h"
#endif  // defined(OS_LINUX)
namespace {

// One of the linux specific headers defines this as a macro.
#ifdef DestroyAll
#undef DestroyAll
#endif

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
      host->Send(msg_.release());
  }

  int host_id_;
  scoped_ptr<IPC::Message> msg_;
};

class UIThreadSender : public IPC::Channel::Sender {
 public:
  virtual bool Send(IPC::Message* msg) {
  // The GPU process must never send a synchronous IPC message to the browser
  // process. This could result in deadlock. Unfortunately linux does this for
  // GpuHostMsg_ResizeXID. TODO(apatrick): fix this before issuing any GL calls
  // on the browser process' GPU thread.
#if !defined(OS_LINUX)
    DCHECK(!msg->is_sync());
#endif

    // When the GpuChannelManager sends an IPC, post it to the UI thread without
    // using IPC.
    bool success = BrowserThread::PostTask(
        BrowserThread::UI,
        FROM_HERE,
        new RouteToGpuProcessHostUIShimTask(0, *msg));

    delete msg;
    return success;
  }
};

void ForwardMessageToGpuThread(GpuChannelManager* gpu_channel_manager,
                               IPC::Message* msg) {
  bool success = gpu_channel_manager->OnMessageReceived(*msg);

  // If the message was not handled, it is likely it was intended for the
  // GpuChildThread, which does not exist in single process and in process GPU
  // mode.
  DCHECK(success);

  delete msg;
}

}  // namespace

RouteToGpuProcessHostUIShimTask::RouteToGpuProcessHostUIShimTask(
    int host_id,
    const IPC::Message& msg)
  : host_id_(host_id),
    msg_(msg) {
}

RouteToGpuProcessHostUIShimTask::~RouteToGpuProcessHostUIShimTask() {
}

void RouteToGpuProcessHostUIShimTask::Run() {
  GpuProcessHostUIShim* ui_shim = GpuProcessHostUIShim::FromID(host_id_);
  if (ui_shim)
    ui_shim->OnMessageReceived(msg_);
}

GpuProcessHostUIShim::GpuProcessHostUIShim(int host_id)
    : host_id_(host_id),
      gpu_channel_manager_(NULL),
      ui_thread_sender_(NULL) {
  g_hosts_by_id.AddWithID(this, host_id_);
  if (host_id == 0) {
    ui_thread_sender_ = new UIThreadSender;
    gpu_channel_manager_ = new GpuChannelManager(
        ui_thread_sender_,
        NULL,
        BrowserThread::GetMessageLoopProxyForThread(BrowserThread::IO),
        g_browser_process->shutdown_event());
  }
}

// static
GpuProcessHostUIShim* GpuProcessHostUIShim::Create(int host_id) {
  DCHECK(!FromID(host_id));
  return new GpuProcessHostUIShim(host_id);
}

// static
void GpuProcessHostUIShim::Destroy(int host_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  delete FromID(host_id);
}

// static
void GpuProcessHostUIShim::DestroyAll() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  while (!g_hosts_by_id.IsEmpty()) {
    IDMap<GpuProcessHostUIShim>::iterator it(&g_hosts_by_id);
    delete it.GetCurrentValue();
  }
}

// static
GpuProcessHostUIShim* GpuProcessHostUIShim::FromID(int host_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return g_hosts_by_id.Lookup(host_id);
}

bool GpuProcessHostUIShim::Send(IPC::Message* msg) {
  DCHECK(CalledOnValidThread());

  bool success;

  if (host_id_ == 0) {
    success = BrowserThread::PostTask(
        BrowserThread::GPU,
        FROM_HERE,
        NewRunnableFunction(ForwardMessageToGpuThread,
                            gpu_channel_manager_,
                            msg));
  } else {
    success = BrowserThread::PostTask(
        BrowserThread::IO,
        FROM_HERE,
        new SendOnIOThreadTask(host_id_, msg));
  }

  return success;
}

bool GpuProcessHostUIShim::OnMessageReceived(const IPC::Message& message) {
  DCHECK(CalledOnValidThread());

  if (message.routing_id() != MSG_ROUTING_CONTROL)
    return false;

  return OnControlMessageReceived(message);
}

#if defined(OS_MACOSX)

void GpuProcessHostUIShim::DidDestroyAcceleratedSurface(int renderer_id,
                                                        int render_view_id) {
  // Destroy the command buffer that owns the accelerated surface.
  Send(new GpuMsg_DestroyCommandBuffer(renderer_id, render_view_id));
}

void GpuProcessHostUIShim::SendToGpuHost(int host_id, IPC::Message* msg) {
  GpuProcessHostUIShim* ui_shim = FromID(host_id);
  if (!ui_shim)
    return;

  ui_shim->Send(msg);
}

#endif

GpuProcessHostUIShim::~GpuProcessHostUIShim() {
  DCHECK(CalledOnValidThread());
  g_hosts_by_id.Remove(host_id_);

  // Ensure these are destroyed on the GPU thread.
  if (gpu_channel_manager_) {
    BrowserThread::DeleteSoon(BrowserThread::GPU,
                              FROM_HERE,
                              gpu_channel_manager_);
    gpu_channel_manager_ = NULL;
  }
  if (ui_thread_sender_) {
    BrowserThread::DeleteSoon(BrowserThread::GPU,
                              FROM_HERE,
                              ui_thread_sender_);
    ui_thread_sender_ = NULL;
  }
}

bool GpuProcessHostUIShim::OnControlMessageReceived(
    const IPC::Message& message) {
  DCHECK(CalledOnValidThread());

  IPC_BEGIN_MESSAGE_MAP(GpuProcessHostUIShim, message)
    IPC_MESSAGE_HANDLER(GpuHostMsg_OnLogMessage,
                        OnLogMessage)
#if defined(OS_LINUX) && !defined(TOUCH_UI) || defined(OS_WIN)
    IPC_MESSAGE_HANDLER(GpuHostMsg_ResizeView, OnResizeView)
#elif defined(OS_MACOSX)
    IPC_MESSAGE_HANDLER(GpuHostMsg_AcceleratedSurfaceSetIOSurface,
                        OnAcceleratedSurfaceSetIOSurface)
    IPC_MESSAGE_HANDLER(GpuHostMsg_AcceleratedSurfaceBuffersSwapped,
                        OnAcceleratedSurfaceBuffersSwapped)
#endif
    IPC_MESSAGE_UNHANDLED_ERROR()
  IPC_END_MESSAGE_MAP()

  return true;
}

void GpuProcessHostUIShim::OnLogMessage(
    int level,
    const std::string& header,
    const std::string& message) {
  DictionaryValue* dict = new DictionaryValue();
  dict->SetInteger("level", level);
  dict->SetString("header", header);
  dict->SetString("message", message);
  GpuDataManager::GetInstance()->AddLogMessage(dict);
}

#if defined(OS_LINUX) && !defined(TOUCH_UI) || defined(OS_WIN)

void GpuProcessHostUIShim::OnResizeView(int32 renderer_id,
                                        int32 render_view_id,
                                        int32 command_buffer_route_id,
                                        gfx::Size size) {
  RenderViewHost* host = RenderViewHost::FromID(renderer_id, render_view_id);
  if (host) {
    RenderWidgetHostView* view = host->view();
    if (view) {
      gfx::PluginWindowHandle handle = view->GetCompositingSurface();

      // Resize the window synchronously. The GPU process must not issue GL
      // calls on the command buffer until the window is the size it expects it
      // to be.
#if defined(OS_LINUX) && !defined(TOUCH_UI)
      GdkWindow* window = reinterpret_cast<GdkWindow*>(
          gdk_xid_table_lookup(handle));
      if (window) {
        Display* display = GDK_WINDOW_XDISPLAY(window);
        gdk_window_resize(window, size.width(), size.height());
        XSync(display, False);
      }
#elif defined(OS_WIN)
      SetWindowPos(handle,
          NULL,
          0, 0,
          size.width(),
          size.height(),
          SWP_NOSENDCHANGING | SWP_NOCOPYBITS | SWP_NOZORDER |
              SWP_NOACTIVATE | SWP_DEFERERASE);
#endif
    }
  }

  // Always respond even if the window no longer exists. The GPU process cannot
  // make progress on the resizing command buffer until it receives the
  // response.
  Send(new GpuMsg_ResizeViewACK(renderer_id, command_buffer_route_id));
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
  TRACE_EVENT0("renderer",
      "GpuProcessHostUIShim::OnAcceleratedSurfaceBuffersSwapped");
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

#endif
