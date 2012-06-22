// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/gpu/gpu_process_host_ui_shim.h"

#include <algorithm>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/id_map.h"
#include "base/lazy_instance.h"
#include "base/process_util.h"
#include "base/string_number_conversions.h"
#include "base/debug/trace_event.h"
#include "content/browser/gpu/gpu_data_manager_impl.h"
#include "content/browser/gpu/gpu_process_host.h"
#include "content/browser/gpu/gpu_surface_tracker.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/common/gpu/gpu_messages.h"
#include "content/port/browser/render_widget_host_view_port.h"
#include "content/public/browser/browser_thread.h"
#include "ui/gl/gl_switches.h"

#if defined(TOOLKIT_GTK)
// These two #includes need to come after gpu_messages.h.
#include "ui/base/x/x11_util.h"
#include "ui/gfx/size.h"
#include <gdk/gdk.h>   // NOLINT
#include <gdk/gdkx.h>  // NOLINT
#endif

using content::BrowserThread;
using content::RenderWidgetHostImpl;
using content::RenderWidgetHostViewPort;

namespace {

// One of the linux specific headers defines this as a macro.
#ifdef DestroyAll
#undef DestroyAll
#endif

base::LazyInstance<IDMap<GpuProcessHostUIShim> > g_hosts_by_id =
    LAZY_INSTANCE_INITIALIZER;

void SendOnIOThreadTask(int host_id, IPC::Message* msg) {
  GpuProcessHost* host = GpuProcessHost::FromID(host_id);
  if (host)
    host->Send(msg);
  else
    delete msg;
}

class ScopedSendOnIOThread {
 public:
  ScopedSendOnIOThread(int host_id, IPC::Message* msg)
      : host_id_(host_id),
        msg_(msg),
        cancelled_(false) {
  }

  ~ScopedSendOnIOThread() {
    if (!cancelled_) {
      BrowserThread::PostTask(BrowserThread::IO,
                              FROM_HERE,
                              base::Bind(&SendOnIOThreadTask,
                                         host_id_,
                                         msg_.release()));
    }
  }

  void Cancel() { cancelled_ = true; }

 private:
  int host_id_;
  scoped_ptr<IPC::Message> msg_;
  bool cancelled_;
};

RenderWidgetHostViewPort* GetRenderWidgetHostViewFromSurfaceID(
    int surface_id) {
  int render_process_id = 0;
  int render_widget_id = 0;
  if (!GpuSurfaceTracker::Get()->GetRenderWidgetIDForSurface(
        surface_id, &render_process_id, &render_widget_id))
    return NULL;

  content::RenderProcessHost* process =
      content::RenderProcessHost::FromID(render_process_id);
  if (!process)
    return NULL;

  content::RenderWidgetHost* host = process->GetRenderWidgetHostByID(
      render_widget_id);
  return host ? RenderWidgetHostViewPort::FromRWHV(host->GetView()) : NULL;
}

}  // namespace

void RouteToGpuProcessHostUIShimTask(int host_id, const IPC::Message& msg) {
  GpuProcessHostUIShim* ui_shim = GpuProcessHostUIShim::FromID(host_id);
  if (ui_shim)
    ui_shim->OnMessageReceived(msg);
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

// static
GpuProcessHostUIShim* GpuProcessHostUIShim::GetOneInstance() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (g_hosts_by_id.Pointer()->IsEmpty())
    return NULL;
  IDMap<GpuProcessHostUIShim>::iterator it(g_hosts_by_id.Pointer());
  return it.GetCurrentValue();
}

bool GpuProcessHostUIShim::Send(IPC::Message* msg) {
  DCHECK(CalledOnValidThread());
  return BrowserThread::PostTask(BrowserThread::IO,
                                 FROM_HERE,
                                 base::Bind(&SendOnIOThreadTask,
                                            host_id_,
                                            msg));
}

bool GpuProcessHostUIShim::OnMessageReceived(const IPC::Message& message) {
  DCHECK(CalledOnValidThread());

  if (message.routing_id() != MSG_ROUTING_CONTROL)
    return false;

  return OnControlMessageReceived(message);
}

void GpuProcessHostUIShim::SimulateRemoveAllContext() {
  Send(new GpuMsg_Clean());
}

void GpuProcessHostUIShim::SimulateCrash() {
  Send(new GpuMsg_Crash());
}

void GpuProcessHostUIShim::SimulateHang() {
  Send(new GpuMsg_Hang());
}

GpuProcessHostUIShim::~GpuProcessHostUIShim() {
  DCHECK(CalledOnValidThread());
  g_hosts_by_id.Pointer()->Remove(host_id_);

  DictionaryValue* dict = new DictionaryValue();
  dict->SetInteger("level", logging::LOG_ERROR);
  dict->SetString("header", "GpuProcessHostUIShim");
  dict->SetString("message", "GPU Process Crashed.");
  GpuDataManagerImpl::GetInstance()->AddLogMessage(dict);
}

bool GpuProcessHostUIShim::OnControlMessageReceived(
    const IPC::Message& message) {
  DCHECK(CalledOnValidThread());

  IPC_BEGIN_MESSAGE_MAP(GpuProcessHostUIShim, message)
    IPC_MESSAGE_HANDLER(GpuHostMsg_OnLogMessage,
                        OnLogMessage)

    IPC_MESSAGE_HANDLER(GpuHostMsg_AcceleratedSurfaceBuffersSwapped,
                        OnAcceleratedSurfaceBuffersSwapped)
    IPC_MESSAGE_HANDLER(GpuHostMsg_AcceleratedSurfacePostSubBuffer,
                        OnAcceleratedSurfacePostSubBuffer)
    IPC_MESSAGE_HANDLER(GpuHostMsg_AcceleratedSurfaceSuspend,
                        OnAcceleratedSurfaceSuspend)
    IPC_MESSAGE_HANDLER(GpuHostMsg_GraphicsInfoCollected,
                        OnGraphicsInfoCollected)

#if defined(TOOLKIT_GTK) || defined(OS_WIN)
    IPC_MESSAGE_HANDLER(GpuHostMsg_ResizeView, OnResizeView)
#endif

#if defined(USE_AURA)
    IPC_MESSAGE_HANDLER(GpuHostMsg_AcceleratedSurfaceNew,
                        OnAcceleratedSurfaceNew)
#endif

#if defined(USE_AURA)
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
  GpuDataManagerImpl::GetInstance()->AddLogMessage(dict);
}

void GpuProcessHostUIShim::OnGraphicsInfoCollected(
    const content::GPUInfo& gpu_info) {
  // OnGraphicsInfoCollected is sent back after the GPU process successfully
  // initializes GL.
  TRACE_EVENT0("test_gpu", "OnGraphicsInfoCollected");

  GpuDataManagerImpl::GetInstance()->UpdateGpuInfo(gpu_info);
}

#if defined(TOOLKIT_GTK) || defined(OS_WIN)

void GpuProcessHostUIShim::OnResizeView(int32 surface_id,
                                        int32 route_id,
                                        gfx::Size size) {
  // Always respond even if the window no longer exists. The GPU process cannot
  // make progress on the resizing command buffer until it receives the
  // response.
  ScopedSendOnIOThread delayed_send(
      host_id_,
      new AcceleratedSurfaceMsg_ResizeViewACK(route_id));

  RenderWidgetHostViewPort* view =
      GetRenderWidgetHostViewFromSurfaceID(surface_id);
  if (!view)
    return;

  gfx::GLSurfaceHandle surface = view->GetCompositingSurface();

  // Resize the window synchronously. The GPU process must not issue GL
  // calls on the command buffer until the window is the size it expects it
  // to be.
#if defined(TOOLKIT_GTK)
  GdkWindow* window = reinterpret_cast<GdkWindow*>(
      gdk_xid_table_lookup(surface.handle));
  if (window) {
    Display* display = GDK_WINDOW_XDISPLAY(window);
    gdk_window_resize(window, size.width(), size.height());
    XSync(display, False);
  }
#elif defined(OS_WIN)
  // Ensure window does not have zero area because D3D cannot create a zero
  // area swap chain.
  SetWindowPos(surface.handle,
      NULL,
      0, 0,
      std::max(1, size.width()),
      std::max(1, size.height()),
      SWP_NOSENDCHANGING | SWP_NOCOPYBITS | SWP_NOZORDER |
          SWP_NOACTIVATE | SWP_DEFERERASE);
#endif
}

#endif

#if defined(USE_AURA)

void GpuProcessHostUIShim::OnAcceleratedSurfaceNew(
    const GpuHostMsg_AcceleratedSurfaceNew_Params& params) {
  ScopedSendOnIOThread delayed_send(
      host_id_,
      new AcceleratedSurfaceMsg_NewACK(
          params.route_id,
          params.surface_handle,
          TransportDIB::DefaultHandleValue()));

  RenderWidgetHostViewPort* view = GetRenderWidgetHostViewFromSurfaceID(
      params.surface_id);
  if (!view)
    return;

  uint64 surface_handle = params.surface_handle;
  TransportDIB::Handle shm_handle = TransportDIB::DefaultHandleValue();

  view->AcceleratedSurfaceNew(
      params.width, params.height, &surface_handle, &shm_handle);
  delayed_send.Cancel();
  Send(new AcceleratedSurfaceMsg_NewACK(
      params.route_id, surface_handle, shm_handle));
}

#endif

static base::TimeDelta GetSwapDelay() {
  CommandLine* cmd_line = CommandLine::ForCurrentProcess();
  int delay = 0;
  if (cmd_line->HasSwitch(switches::kGpuSwapDelay)) {
    base::StringToInt(cmd_line->GetSwitchValueNative(
        switches::kGpuSwapDelay).c_str(), &delay);
  }
  return base::TimeDelta::FromMilliseconds(delay);
}

void GpuProcessHostUIShim::OnAcceleratedSurfaceBuffersSwapped(
    const GpuHostMsg_AcceleratedSurfaceBuffersSwapped_Params& params) {
  TRACE_EVENT0("renderer",
      "GpuProcessHostUIShim::OnAcceleratedSurfaceBuffersSwapped");

  ScopedSendOnIOThread delayed_send(
      host_id_,
      new AcceleratedSurfaceMsg_BuffersSwappedACK(params.route_id));

  RenderWidgetHostViewPort* view = GetRenderWidgetHostViewFromSurfaceID(
      params.surface_id);
  if (!view)
    return;

  delayed_send.Cancel();

  static const base::TimeDelta swap_delay = GetSwapDelay();
  if (swap_delay.ToInternalValue())
    base::PlatformThread::Sleep(swap_delay);

  // View must send ACK message after next composite.
  view->AcceleratedSurfaceBuffersSwapped(params, host_id_);
}

void GpuProcessHostUIShim::OnAcceleratedSurfacePostSubBuffer(
    const GpuHostMsg_AcceleratedSurfacePostSubBuffer_Params& params) {
  TRACE_EVENT0("renderer",
      "GpuProcessHostUIShim::OnAcceleratedSurfacePostSubBuffer");

  ScopedSendOnIOThread delayed_send(
      host_id_,
      new AcceleratedSurfaceMsg_PostSubBufferACK(params.route_id));

  RenderWidgetHostViewPort* view =
      GetRenderWidgetHostViewFromSurfaceID(params.surface_id);
  if (!view)
    return;

  delayed_send.Cancel();

  // View must send ACK message after next composite.
  view->AcceleratedSurfacePostSubBuffer(params, host_id_);
}

void GpuProcessHostUIShim::OnAcceleratedSurfaceSuspend(int32 surface_id) {
  TRACE_EVENT0("renderer",
      "GpuProcessHostUIShim::OnAcceleratedSurfaceSuspend");

  RenderWidgetHostViewPort* view =
      GetRenderWidgetHostViewFromSurfaceID(surface_id);
  if (!view)
    return;

  view->AcceleratedSurfaceSuspend();
}

#if defined(USE_AURA)

void GpuProcessHostUIShim::OnAcceleratedSurfaceRelease(
    const GpuHostMsg_AcceleratedSurfaceRelease_Params& params) {
  RenderWidgetHostViewPort* view = GetRenderWidgetHostViewFromSurfaceID(
      params.surface_id);
  if (!view)
    return;
  view->AcceleratedSurfaceRelease(params.identifier);
}

#endif
