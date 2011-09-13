// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/gpu/gpu_process_host_ui_shim.h"

#include <algorithm>

#include "base/id_map.h"
#include "base/process_util.h"
#include "base/debug/trace_event.h"
#include "base/lazy_instance.h"
#include "content/browser/browser_thread.h"
#include "content/browser/gpu/gpu_data_manager.h"
#include "content/browser/gpu/gpu_process_host.h"
#include "content/browser/renderer_host/render_process_host.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/renderer_host/render_widget_host_view.h"
#include "content/common/gpu/gpu_messages.h"

#if defined(TOOLKIT_USES_GTK)
// These two #includes need to come after gpu_messages.h.
#include <gdk/gdkwindow.h>  // NOLINT
#include <gdk/gdkx.h>  // NOLINT
#include "ui/base/x/x11_util.h"
#include "ui/gfx/size.h"
#endif
namespace {

// One of the linux specific headers defines this as a macro.
#ifdef DestroyAll
#undef DestroyAll
#endif

base::LazyInstance<IDMap<GpuProcessHostUIShim> > g_hosts_by_id(
    base::LINKER_INITIALIZED);

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
    : host_id_(host_id) {
  g_hosts_by_id.Pointer()->AddWithID(this, host_id_);
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
  while (!g_hosts_by_id.Pointer()->IsEmpty()) {
    IDMap<GpuProcessHostUIShim>::iterator it(g_hosts_by_id.Pointer());
    delete it.GetCurrentValue();
  }
}

// static
GpuProcessHostUIShim* GpuProcessHostUIShim::FromID(int host_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return g_hosts_by_id.Pointer()->Lookup(host_id);
}

bool GpuProcessHostUIShim::Send(IPC::Message* msg) {
  DCHECK(CalledOnValidThread());
  return BrowserThread::PostTask(BrowserThread::IO,
                                 FROM_HERE,
                                 new SendOnIOThreadTask(host_id_, msg));
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
  if (!ui_shim) {
    delete msg;
    return;
  }

  ui_shim->Send(msg);
}

#endif

GpuProcessHostUIShim::~GpuProcessHostUIShim() {
  DCHECK(CalledOnValidThread());
  g_hosts_by_id.Pointer()->Remove(host_id_);
}

bool GpuProcessHostUIShim::OnControlMessageReceived(
    const IPC::Message& message) {
  DCHECK(CalledOnValidThread());

  IPC_BEGIN_MESSAGE_MAP(GpuProcessHostUIShim, message)
    IPC_MESSAGE_HANDLER(GpuHostMsg_OnLogMessage,
                        OnLogMessage)
#if defined(TOOLKIT_USES_GTK) && !defined(TOUCH_UI) || defined(OS_WIN)
    IPC_MESSAGE_HANDLER(GpuHostMsg_ResizeView, OnResizeView)
#endif

#if defined(OS_MACOSX) || defined(TOUCH_UI)
    IPC_MESSAGE_HANDLER(GpuHostMsg_AcceleratedSurfaceSetIOSurface,
                        OnAcceleratedSurfaceSetIOSurface)
    IPC_MESSAGE_HANDLER(GpuHostMsg_AcceleratedSurfaceBuffersSwapped,
                        OnAcceleratedSurfaceBuffersSwapped)
#endif

#if defined(TOUCH_UI)
    IPC_MESSAGE_HANDLER(GpuHostMsg_AcceleratedSurfaceRelease,
                        OnAcceleratedSurfaceRelease)
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

#if defined(TOOLKIT_USES_GTK) && !defined(TOUCH_UI) || defined(OS_WIN)

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
#if defined(TOOLKIT_USES_GTK) && !defined(TOUCH_UI)
      GdkWindow* window = reinterpret_cast<GdkWindow*>(
          gdk_xid_table_lookup(handle));
      if (window) {
        Display* display = GDK_WINDOW_XDISPLAY(window);
        gdk_window_resize(window, size.width(), size.height());
        XSync(display, False);
      }
#elif defined(OS_WIN)
      // Ensure window does not have zero area because D3D cannot create a zero
      // area swap chain.
      SetWindowPos(handle,
          NULL,
          0, 0,
          std::max(1, size.width()),
          std::max(1, size.height()),
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

#endif

#if defined(OS_MACOSX) || defined(TOUCH_UI)

void GpuProcessHostUIShim::OnAcceleratedSurfaceSetIOSurface(
    const GpuHostMsg_AcceleratedSurfaceSetIOSurface_Params& params) {
  RenderViewHost* host = RenderViewHost::FromID(params.renderer_id,
                                                params.render_view_id);
  if (!host)
    return;
  RenderWidgetHostView* view = host->view();
  if (!view)
    return;
#if defined(OS_MACOSX)
  view->AcceleratedSurfaceSetIOSurface(params.window,
                                       params.width,
                                       params.height,
                                       params.identifier);
#elif defined(TOUCH_UI)
  view->AcceleratedSurfaceSetIOSurface(
      params.width, params.height, params.identifier);
  Send(new AcceleratedSurfaceMsg_SetSurfaceACK(
      params.route_id, params.identifier));
#endif
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
#if defined (OS_MACOSX)
  view->AcceleratedSurfaceBuffersSwapped(
      // Parameters needed to swap the IOSurface.
      params.window,
      params.surface_id,
      // Parameters needed to formulate an acknowledgment.
      params.renderer_id,
      params.route_id,
      host_id_,
      params.swap_buffers_count);
#elif defined(TOUCH_UI)
  view->AcceleratedSurfaceBuffersSwapped(params.surface_id);
  Send(new AcceleratedSurfaceMsg_BuffersSwappedACK(params.route_id));
#endif
}

#endif

#if defined(TOUCH_UI)

void GpuProcessHostUIShim::OnAcceleratedSurfaceRelease(
    const GpuHostMsg_AcceleratedSurfaceRelease_Params& params) {
  RenderViewHost* host = RenderViewHost::FromID(params.renderer_id,
                                                params.render_view_id);
  if (!host)
    return;
  RenderWidgetHostView* view = host->view();
  if (!view)
    return;
  view->AcceleratedSurfaceRelease(params.identifier);
}

#endif
