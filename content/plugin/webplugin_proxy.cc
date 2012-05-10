// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/plugin/webplugin_proxy.h"

#include "build/build_config.h"

#include "base/bind.h"
#include "base/lazy_instance.h"
#include "base/memory/scoped_handle.h"
#include "base/shared_memory.h"
#include "build/build_config.h"
#include "content/common/npobject_proxy.h"
#include "content/common/npobject_util.h"
#include "content/common/plugin_messages.h"
#include "content/plugin/plugin_channel.h"
#include "content/plugin/plugin_thread.h"
#include "content/public/common/content_client.h"
#include "content/public/common/url_constants.h"
#include "skia/ext/platform_canvas.h"
#include "skia/ext/platform_device.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebBindings.h"
#include "ui/gfx/blit.h"
#include "ui/gfx/canvas.h"
#include "webkit/plugins/npapi/webplugin_delegate_impl.h"

#if defined(OS_MACOSX)
#include "base/mac/mac_util.h"
#include "base/mac/scoped_cftyperef.h"
#include "content/plugin/webplugin_accelerated_surface_proxy_mac.h"
#endif

#if defined(USE_X11)
#include "ui/base/x/x11_util_internal.h"
#endif

#if defined(OS_WIN)
#include "content/public/common/sandbox_init.h"
#endif

using WebKit::WebBindings;

using webkit::npapi::WebPluginResourceClient;
#if defined(OS_MACOSX)
using webkit::npapi::WebPluginAcceleratedSurface;
#endif

WebPluginProxy::WebPluginProxy(
    PluginChannel* channel,
    int route_id,
    const GURL& page_url,
    gfx::NativeViewId containing_window,
    int host_render_view_routing_id)
    : channel_(channel),
      route_id_(route_id),
      window_npobject_(NULL),
      plugin_element_(NULL),
      delegate_(NULL),
      waiting_for_paint_(false),
      containing_window_(containing_window),
      page_url_(page_url),
      transparent_(false),
      windowless_buffer_index_(0),
      host_render_view_routing_id_(host_render_view_routing_id),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_factory_(this)) {
#if defined(USE_X11)
  windowless_shm_pixmaps_[0] = None;
  windowless_shm_pixmaps_[1] = None;
  use_shm_pixmap_ = false;

  // If the X server supports SHM pixmaps
  // and the color depth and masks match,
  // then consider using SHM pixmaps for windowless plugin painting.
  Display* display = ui::GetXDisplay();
  if (ui::QuerySharedMemorySupport(display) == ui::SHARED_MEMORY_PIXMAP &&
      ui::BitsPerPixelForPixmapDepth(
          display, DefaultDepth(display, DefaultScreen(display))) == 32) {
    Visual* vis = DefaultVisual(display, DefaultScreen(display));

    if (vis->red_mask == 0xff0000 &&
        vis->green_mask == 0xff00 &&
        vis->blue_mask == 0xff)
      use_shm_pixmap_ = true;
  }
#endif
}

WebPluginProxy::~WebPluginProxy() {
#if defined(USE_X11)
  if (windowless_shm_pixmaps_[0] != None)
    XFreePixmap(ui::GetXDisplay(), windowless_shm_pixmaps_[0]);
  if (windowless_shm_pixmaps_[1] != None)
    XFreePixmap(ui::GetXDisplay(), windowless_shm_pixmaps_[1]);
#endif

#if defined(OS_MACOSX)
  // Destroy the surface early, since it may send messages during cleanup.
  if (accelerated_surface_.get())
    accelerated_surface_.reset();
#endif

  if (plugin_element_)
    WebBindings::releaseObject(plugin_element_);
  if (window_npobject_)
    WebBindings::releaseObject(window_npobject_);
}

bool WebPluginProxy::Send(IPC::Message* msg) {
  return channel_->Send(msg);
}

void WebPluginProxy::SetWindow(gfx::PluginWindowHandle window) {
  Send(new PluginHostMsg_SetWindow(route_id_, window));
}

void WebPluginProxy::SetAcceptsInputEvents(bool accepts) {
  NOTREACHED();
}

void WebPluginProxy::WillDestroyWindow(gfx::PluginWindowHandle window) {
#if defined(OS_WIN)
  PluginThread::current()->Send(
      new PluginProcessHostMsg_PluginWindowDestroyed(
          window, ::GetParent(window)));
#elif defined(USE_X11)
  // Nothing to do.
#else
  NOTIMPLEMENTED();
#endif
}

#if defined(OS_WIN)
void WebPluginProxy::SetWindowlessPumpEvent(HANDLE pump_messages_event) {
  HANDLE pump_messages_event_for_renderer = NULL;
  content::BrokerDuplicateHandle(pump_messages_event, channel_->peer_pid(),
                                 &pump_messages_event_for_renderer,
                                 SYNCHRONIZE | EVENT_MODIFY_STATE, 0);
  DCHECK(pump_messages_event_for_renderer != NULL);
  Send(new PluginHostMsg_SetWindowlessPumpEvent(
      route_id_, pump_messages_event_for_renderer));
}

void WebPluginProxy::ReparentPluginWindow(HWND window, HWND parent) {
  PluginThread::current()->Send(
      new PluginProcessHostMsg_ReparentPluginWindow(window, parent));
}

void WebPluginProxy::ReportExecutableMemory(size_t size) {
  PluginThread::current()->Send(
      new PluginProcessHostMsg_ReportExecutableMemory(size));
}
#endif

void WebPluginProxy::CancelResource(unsigned long id) {
  Send(new PluginHostMsg_CancelResource(route_id_, id));
  resource_clients_.erase(id);
}

void WebPluginProxy::Invalidate() {
  gfx::Rect rect(0, 0,
                 delegate_->GetRect().width(),
                 delegate_->GetRect().height());
  InvalidateRect(rect);
}

void WebPluginProxy::InvalidateRect(const gfx::Rect& rect) {
#if defined(OS_MACOSX)
  // If this is a Core Animation plugin, all we need to do is inform the
  // delegate.
  if (!windowless_context()) {
    delegate_->PluginDidInvalidate();
    return;
  }

  // Some plugins will send invalidates larger than their own rect when
  // offscreen, so constrain invalidates to the plugin rect.
  gfx::Rect plugin_rect = delegate_->GetRect();
  plugin_rect.set_origin(gfx::Point(0, 0));
  const gfx::Rect invalidate_rect(rect.Intersect(plugin_rect));
#else
  const gfx::Rect invalidate_rect(rect);
#endif
  damaged_rect_ = damaged_rect_.Union(invalidate_rect);
  // Ignore NPN_InvalidateRect calls with empty rects.  Also don't send an
  // invalidate if it's outside the clipping region, since if we did it won't
  // lead to a paint and we'll be stuck waiting forever for a DidPaint response.
  //
  // TODO(piman): There is a race condition here, because this test assumes
  // that when the paint actually occurs, the clip rect will not have changed.
  // This is not true because scrolling (or window resize) could occur and be
  // handled by the renderer before it receives the InvalidateRect message,
  // changing the clip rect and then not painting.
  if (damaged_rect_.IsEmpty() ||
      !delegate_->GetClipRect().Intersects(damaged_rect_))
    return;

  // Only send a single InvalidateRect message at a time.  From DidPaint we
  // will dispatch an additional InvalidateRect message if necessary.
  if (!waiting_for_paint_) {
    waiting_for_paint_ = true;
    // Invalidates caused by calls to NPN_InvalidateRect/NPN_InvalidateRgn
    // need to be painted asynchronously as per the NPAPI spec.
    MessageLoop::current()->PostTask(FROM_HERE,
        base::Bind(&WebPluginProxy::OnPaint, weak_factory_.GetWeakPtr(),
            damaged_rect_));
    damaged_rect_ = gfx::Rect();
  }
}

NPObject* WebPluginProxy::GetWindowScriptNPObject() {
  if (window_npobject_)
    return window_npobject_;

  int npobject_route_id = channel_->GenerateRouteID();
  bool success = false;
  Send(new PluginHostMsg_GetWindowScriptNPObject(
      route_id_, npobject_route_id, &success));
  if (!success)
    return NULL;

  window_npobject_ = NPObjectProxy::Create(
      channel_, npobject_route_id, containing_window_, page_url_);

  return window_npobject_;
}

NPObject* WebPluginProxy::GetPluginElement() {
  if (plugin_element_)
    return plugin_element_;

  int npobject_route_id = channel_->GenerateRouteID();
  bool success = false;
  Send(new PluginHostMsg_GetPluginElement(route_id_, npobject_route_id,
                                          &success));
  if (!success)
    return NULL;

  plugin_element_ = NPObjectProxy::Create(
      channel_, npobject_route_id, containing_window_, page_url_);

  return plugin_element_;
}

bool WebPluginProxy::FindProxyForUrl(const GURL& url, std::string* proxy_list) {
  bool result = false;
  Send(new PluginHostMsg_ResolveProxy(route_id_, url, &result, proxy_list));
  return result;
}

void WebPluginProxy::SetCookie(const GURL& url,
                               const GURL& first_party_for_cookies,
                               const std::string& cookie) {
  Send(new PluginHostMsg_SetCookie(route_id_, url,
                                   first_party_for_cookies, cookie));
}

std::string WebPluginProxy::GetCookies(const GURL& url,
                                       const GURL& first_party_for_cookies) {
  std::string cookies;
  Send(new PluginHostMsg_GetCookies(route_id_, url,
                                    first_party_for_cookies, &cookies));

  return cookies;
}

WebPluginResourceClient* WebPluginProxy::GetResourceClient(int id) {
  ResourceClientMap::iterator iterator = resource_clients_.find(id);
  // The IPC messages which deal with streams are now asynchronous. It is
  // now possible to receive stream messages from the renderer for streams
  // which may have been cancelled by the plugin.
  if (iterator == resource_clients_.end()) {
    return NULL;
  }

  return iterator->second;
}

int WebPluginProxy::GetRendererId() {
  if (channel_.get())
    return channel_->renderer_id();
  return -1;
}

void WebPluginProxy::DidPaint() {
  // If we have an accumulated damaged rect, then check to see if we need to
  // send out another InvalidateRect message.
  waiting_for_paint_ = false;
  if (!damaged_rect_.IsEmpty())
    InvalidateRect(damaged_rect_);
}

void WebPluginProxy::OnResourceCreated(int resource_id,
                                       WebPluginResourceClient* client) {
  DCHECK(resource_clients_.find(resource_id) == resource_clients_.end());
  resource_clients_[resource_id] = client;
}

void WebPluginProxy::HandleURLRequest(const char* url,
                                      const char* method,
                                      const char* target,
                                      const char* buf,
                                      unsigned int len,
                                      int notify_id,
                                      bool popups_allowed,
                                      bool notify_redirects) {
 if (!target && (0 == base::strcasecmp(method, "GET"))) {
    // Please refer to https://bugzilla.mozilla.org/show_bug.cgi?id=366082
    // for more details on this.
    if (delegate_->GetQuirks() &
        webkit::npapi::WebPluginDelegateImpl::
            PLUGIN_QUIRK_BLOCK_NONSTANDARD_GETURL_REQUESTS) {
      GURL request_url(url);
      if (!request_url.SchemeIs(chrome::kHttpScheme) &&
          !request_url.SchemeIs(chrome::kHttpsScheme) &&
          !request_url.SchemeIs(chrome::kFtpScheme)) {
        return;
      }
    }
  }

  PluginHostMsg_URLRequest_Params params;
  params.url = url;
  params.method = method;
  if (target)
    params.target = std::string(target);

  if (len) {
    params.buffer.resize(len);
    memcpy(&params.buffer.front(), buf, len);
  }

  params.notify_id = notify_id;
  params.popups_allowed = popups_allowed;
  params.notify_redirects = notify_redirects;

  Send(new PluginHostMsg_URLRequest(route_id_, params));
}

void WebPluginProxy::Paint(const gfx::Rect& rect) {
#if defined(OS_MACOSX)
  if (!windowless_context())
    return;
#else
  if (!windowless_canvas() || !windowless_canvas()->getDevice())
    return;
#endif

  // Clear the damaged area so that if the plugin doesn't paint there we won't
  // end up with the old values.
  gfx::Rect offset_rect = rect;
  offset_rect.Offset(delegate_->GetRect().origin());
#if defined(OS_MACOSX)
  CGContextSaveGState(windowless_context());
  // It is possible for windowless_contexts_ to change during plugin painting
  // (since the plugin can make a synchronous call during paint event handling),
  // in which case we don't want to try to restore later. Not an owning ref
  // since owning the ref without owning the shared backing memory doesn't make
  // sense, so this should only be used for pointer comparisons.
  CGContextRef saved_context_weak = windowless_context();
  // We also save the buffer index for the comparison because if we flip buffers
  // but haven't reallocated them then we do need to restore the context because
  // it is going to continue to be used.
  int saved_index = windowless_buffer_index_;

  if (background_context_.get()) {
    base::mac::ScopedCFTypeRef<CGImageRef> image(
        CGBitmapContextCreateImage(background_context_));
    CGRect source_rect = rect.ToCGRect();
    // Flip the rect we use to pull from the canvas, since it's upside-down.
    source_rect.origin.y = CGImageGetHeight(image) - rect.y() - rect.height();
    base::mac::ScopedCFTypeRef<CGImageRef> sub_image(
        CGImageCreateWithImageInRect(image, source_rect));
    CGContextDrawImage(windowless_context(), rect.ToCGRect(), sub_image);
  } else if (transparent_) {
    CGContextClearRect(windowless_context(), rect.ToCGRect());
  }
  CGContextClipToRect(windowless_context(), rect.ToCGRect());
  // TODO(caryclark): This is a temporary workaround to allow the Darwin / Skia
  // port to share code with the Darwin / CG port. All ports will eventually use
  // the common code below.
  delegate_->CGPaint(windowless_context(), rect);
  if (windowless_contexts_[saved_index].get() == saved_context_weak)
    CGContextRestoreGState(windowless_contexts_[saved_index]);
#else
  windowless_canvas()->save();

  // The given clip rect is relative to the plugin coordinate system.
  SkRect sk_rect = { SkIntToScalar(rect.x()),
                     SkIntToScalar(rect.y()),
                     SkIntToScalar(rect.right()),
                     SkIntToScalar(rect.bottom()) };
  windowless_canvas()->clipRect(sk_rect);

  // Setup the background.
  if (background_canvas_.get() && background_canvas_.get()->getDevice()) {
    // When a background canvas is given, we're in transparent mode. This means
    // the plugin wants to have the image of the page in the canvas it's drawing
    // into (which is windowless_canvases_) so it can do blending. So we copy
    // the background bitmap into the windowless canvas.
    const SkBitmap& background_bitmap =
        skia::GetTopDevice(*background_canvas_)->accessBitmap(false);
    windowless_canvas()->drawBitmap(background_bitmap, 0, 0);
  } else {
    // In non-transparent mode, the plugin doesn't care what's underneath, so we
    // can just give it black.
    SkPaint black_fill_paint;
    black_fill_paint.setARGB(0xFF, 0x00, 0x00, 0x00);
    windowless_canvas()->drawPaint(black_fill_paint);
  }

  // Bring the windowless canvas into the window coordinate system, which is
  // how the plugin expects to draw (since the windowless API was originally
  // designed just for scribbling over the web page).
  windowless_canvas()->translate(SkIntToScalar(-delegate_->GetRect().x()),
                                 SkIntToScalar(-delegate_->GetRect().y()));

  // Before we send the invalidate, paint so that renderer uses the updated
  // bitmap.
  delegate_->Paint(windowless_canvas(), offset_rect);

  windowless_canvas()->restore();
#endif
}

void WebPluginProxy::UpdateGeometry(
    const gfx::Rect& window_rect,
    const gfx::Rect& clip_rect,
    const TransportDIB::Handle& windowless_buffer0,
    const TransportDIB::Handle& windowless_buffer1,
    int windowless_buffer_index,
    const TransportDIB::Handle& background_buffer,
    bool transparent) {
  gfx::Rect old = delegate_->GetRect();
  gfx::Rect old_clip_rect = delegate_->GetClipRect();
  transparent_ = transparent;

  // Update the buffers before doing anything that could call into plugin code,
  // so that we don't process buffer changes out of order if plugins make
  // synchronous calls that lead to nested UpdateGeometry calls.
  if (TransportDIB::is_valid_handle(windowless_buffer0)) {
    // The plugin's rect changed, so now we have new buffers to draw into.
    SetWindowlessBuffers(windowless_buffer0,
                         windowless_buffer1,
                         background_buffer,
                         window_rect);
  }

  DCHECK(0 <= windowless_buffer_index && windowless_buffer_index <= 1);
  windowless_buffer_index_ = windowless_buffer_index;
#if defined(USE_X11)
  delegate_->SetWindowlessShmPixmap(windowless_shm_pixmap());
#endif

#if defined(OS_MACOSX)
  delegate_->UpdateGeometryAndContext(
      window_rect, clip_rect, windowless_context());
#else
  delegate_->UpdateGeometry(window_rect, clip_rect);
#endif

  // Send over any pending invalidates which occured when the plugin was
  // off screen.
  if (delegate_->IsWindowless() && !clip_rect.IsEmpty() &&
      !damaged_rect_.IsEmpty()) {
    InvalidateRect(damaged_rect_);
  }
}

#if defined(OS_WIN)

void WebPluginProxy::CreateCanvasFromHandle(
    const TransportDIB::Handle& dib_handle,
    const gfx::Rect& window_rect,
    scoped_ptr<skia::PlatformCanvas>* canvas_out) {
  scoped_ptr<skia::PlatformCanvas> canvas(new skia::PlatformCanvas);
  if (!canvas->initialize(
          window_rect.width(),
          window_rect.height(),
          true,
          dib_handle)) {
    canvas_out->reset();
  }
  canvas_out->reset(canvas.release());
  // The canvas does not own the section so we need to close it now.
  CloseHandle(dib_handle);
}

void WebPluginProxy::SetWindowlessBuffers(
    const TransportDIB::Handle& windowless_buffer0,
    const TransportDIB::Handle& windowless_buffer1,
    const TransportDIB::Handle& background_buffer,
    const gfx::Rect& window_rect) {
  CreateCanvasFromHandle(windowless_buffer0,
                         window_rect,
                         &windowless_canvases_[0]);
  if (!windowless_canvases_[0].get()) {
    windowless_canvases_[1].reset();
    background_canvas_.reset();
    return;
  }
  CreateCanvasFromHandle(windowless_buffer1,
                         window_rect,
                         &windowless_canvases_[1]);
  if (!windowless_canvases_[1].get()) {
    windowless_canvases_[0].reset();
    background_canvas_.reset();
    return;
  }

  if (background_buffer) {
    CreateCanvasFromHandle(background_buffer,
                           window_rect,
                           &background_canvas_);
    if (!background_canvas_.get()) {
      windowless_canvases_[0].reset();
      windowless_canvases_[1].reset();
      return;
    }
  }
}

#elif defined(OS_MACOSX)

void WebPluginProxy::CreateDIBAndCGContextFromHandle(
    const TransportDIB::Handle& dib_handle,
    const gfx::Rect& window_rect,
    scoped_ptr<TransportDIB>* dib_out,
    base::mac::ScopedCFTypeRef<CGContextRef>* cg_context_out) {
  // Convert the shared memory handle to a handle that works in our process,
  // and then use that to create a CGContextRef.
  TransportDIB* dib = TransportDIB::Map(dib_handle);
  CGContextRef cg_context = NULL;
  if (dib) {
    cg_context = CGBitmapContextCreate(
        dib->memory(),
        window_rect.width(),
        window_rect.height(),
        8,
        4 * window_rect.width(),
        base::mac::GetSystemColorSpace(),
        kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Host);
    CGContextTranslateCTM(cg_context, 0, window_rect.height());
    CGContextScaleCTM(cg_context, 1, -1);
  }
  dib_out->reset(dib);
  cg_context_out->reset(cg_context);
}

void WebPluginProxy::SetWindowlessBuffers(
    const TransportDIB::Handle& windowless_buffer0,
    const TransportDIB::Handle& windowless_buffer1,
    const TransportDIB::Handle& background_buffer,
    const gfx::Rect& window_rect) {
  CreateDIBAndCGContextFromHandle(windowless_buffer0,
                                  window_rect,
                                  &windowless_dibs_[0],
                                  &windowless_contexts_[0]);
  CreateDIBAndCGContextFromHandle(windowless_buffer1,
                                  window_rect,
                                  &windowless_dibs_[1],
                                  &windowless_contexts_[1]);
  CreateDIBAndCGContextFromHandle(background_buffer,
                                  window_rect,
                                  &background_dib_,
                                  &background_context_);
}

#elif defined(USE_X11)

void WebPluginProxy::CreateDIBAndCanvasFromHandle(
    const TransportDIB::Handle& dib_handle,
    const gfx::Rect& window_rect,
    scoped_ptr<TransportDIB>* dib_out,
    scoped_ptr<skia::PlatformCanvas>* canvas_out) {
  TransportDIB* dib = TransportDIB::Map(dib_handle);
  skia::PlatformCanvas* canvas = NULL;
  // dib may be NULL if the renderer has already destroyed the TransportDIB by
  // the time we receive the handle, e.g. in case of multiple resizes.
  if (dib) {
    canvas = dib->GetPlatformCanvas(window_rect.width(), window_rect.height());
  }
  dib_out->reset(dib);
  canvas_out->reset(canvas);
}

void WebPluginProxy::CreateShmPixmapFromDIB(
    TransportDIB* dib,
    const gfx::Rect& window_rect,
    XID* pixmap_out) {
  if (dib) {
    Display* display = ui::GetXDisplay();
    XID root_window = ui::GetX11RootWindow();
    XShmSegmentInfo shminfo = {0};

    if (*pixmap_out != None)
      XFreePixmap(display, *pixmap_out);

    shminfo.shmseg = dib->MapToX(display);
    // Create a shared memory pixmap based on the image buffer.
    *pixmap_out = XShmCreatePixmap(display, root_window,
                                   NULL, &shminfo,
                                   window_rect.width(), window_rect.height(),
                                   DefaultDepth(display,
                                                DefaultScreen(display)));
  }
}

void WebPluginProxy::SetWindowlessBuffers(
    const TransportDIB::Handle& windowless_buffer0,
    const TransportDIB::Handle& windowless_buffer1,
    const TransportDIB::Handle& background_buffer,
    const gfx::Rect& window_rect) {
  CreateDIBAndCanvasFromHandle(windowless_buffer0,
                               window_rect,
                               &windowless_dibs_[0],
                               &windowless_canvases_[0]);
  CreateDIBAndCanvasFromHandle(windowless_buffer1,
                               window_rect,
                               &windowless_dibs_[1],
                               &windowless_canvases_[1]);
  CreateDIBAndCanvasFromHandle(background_buffer,
                               window_rect,
                               &background_dib_,
                               &background_canvas_);

  // If SHM pixmaps support is available, create SHM pixmaps to pass to the
  // delegate for windowless plugin painting.
  if (delegate_->IsWindowless() && use_shm_pixmap_) {
    CreateShmPixmapFromDIB(windowless_dibs_[0].get(),
                           window_rect,
                           &windowless_shm_pixmaps_[0]);
    CreateShmPixmapFromDIB(windowless_dibs_[1].get(),
                           window_rect,
                           &windowless_shm_pixmaps_[1]);
  }
}

#elif defined(OS_ANDROID)

void WebPluginProxy::SetWindowlessBuffers(
    const TransportDIB::Handle& windowless_buffer0,
    const TransportDIB::Handle& windowless_buffer1,
    const TransportDIB::Handle& background_buffer,
    const gfx::Rect& window_rect) {
  NOTIMPLEMENTED();
}

#endif

void WebPluginProxy::CancelDocumentLoad() {
  Send(new PluginHostMsg_CancelDocumentLoad(route_id_));
}

void WebPluginProxy::InitiateHTTPRangeRequest(
    const char* url, const char* range_info, int range_request_id) {
  Send(new PluginHostMsg_InitiateHTTPRangeRequest(
      route_id_, url, range_info, range_request_id));
}

void WebPluginProxy::SetDeferResourceLoading(unsigned long resource_id,
                                             bool defer) {
  Send(new PluginHostMsg_DeferResourceLoading(route_id_, resource_id, defer));
}

#if defined(OS_MACOSX)
void WebPluginProxy::FocusChanged(bool focused) {
  IPC::Message* msg = new PluginHostMsg_FocusChanged(route_id_, focused);
  Send(msg);
}

void WebPluginProxy::StartIme() {
  IPC::Message* msg = new PluginHostMsg_StartIme(route_id_);
  // This message can be sent during event-handling, and needs to be delivered
  // within that context.
  msg->set_unblock(true);
  Send(msg);
}

void WebPluginProxy::BindFakePluginWindowHandle(bool opaque) {
  Send(new PluginHostMsg_BindFakePluginWindowHandle(route_id_, opaque));
}

WebPluginAcceleratedSurface* WebPluginProxy::GetAcceleratedSurface(
    gfx::GpuPreference gpu_preference) {
  if (!accelerated_surface_.get())
    accelerated_surface_.reset(
        WebPluginAcceleratedSurfaceProxy::Create(this, gpu_preference));
  return accelerated_surface_.get();
}

void WebPluginProxy::AcceleratedFrameBuffersDidSwap(
    gfx::PluginWindowHandle window, uint64 surface_handle) {
  Send(new PluginHostMsg_AcceleratedSurfaceBuffersSwapped(
        route_id_, window, surface_handle));
}

void WebPluginProxy::SetAcceleratedSurface(
    gfx::PluginWindowHandle window,
    const gfx::Size& size,
    uint64 accelerated_surface_identifier) {
  Send(new PluginHostMsg_AcceleratedSurfaceSetIOSurface(
      route_id_, window, size.width(), size.height(),
      accelerated_surface_identifier));
}

void WebPluginProxy::SetAcceleratedDIB(
    gfx::PluginWindowHandle window,
    const gfx::Size& size,
    const TransportDIB::Handle& dib_handle) {
  Send(new PluginHostMsg_AcceleratedSurfaceSetTransportDIB(
      route_id_, window, size.width(), size.height(), dib_handle));
}

void WebPluginProxy::AllocSurfaceDIB(const size_t size,
                                     TransportDIB::Handle* dib_handle) {
  Send(new PluginHostMsg_AllocTransportDIB(route_id_, size, dib_handle));
}

void WebPluginProxy::FreeSurfaceDIB(TransportDIB::Id dib_id) {
  Send(new PluginHostMsg_FreeTransportDIB(route_id_, dib_id));
}

void WebPluginProxy::AcceleratedPluginEnabledRendering() {
  Send(new PluginHostMsg_AcceleratedPluginEnabledRendering(route_id_));
}

void WebPluginProxy::AcceleratedPluginAllocatedIOSurface(int32 width,
                                                         int32 height,
                                                         uint32 surface_id) {
  Send(new PluginHostMsg_AcceleratedPluginAllocatedIOSurface(
      route_id_, width, height, surface_id));
}

void WebPluginProxy::AcceleratedPluginSwappedIOSurface() {
  Send(new PluginHostMsg_AcceleratedPluginSwappedIOSurface(
      route_id_));
}
#endif

void WebPluginProxy::OnPaint(const gfx::Rect& damaged_rect) {
  content::GetContentClient()->SetActiveURL(page_url_);

  Paint(damaged_rect);
  bool allow_buffer_flipping;
#if defined(OS_MACOSX)
  allow_buffer_flipping = delegate_->AllowBufferFlipping();
#else
  allow_buffer_flipping = true;
#endif
  Send(new PluginHostMsg_InvalidateRect(route_id_,
                                        damaged_rect,
                                        allow_buffer_flipping));
}

bool WebPluginProxy::IsOffTheRecord() {
  return channel_->incognito();
}

void WebPluginProxy::ResourceClientDeleted(
    WebPluginResourceClient* resource_client) {
  ResourceClientMap::iterator index = resource_clients_.begin();
  while (index != resource_clients_.end()) {
    WebPluginResourceClient* client = (*index).second;

    if (client == resource_client) {
      resource_clients_.erase(index++);
    } else {
      index++;
    }
  }
}

void WebPluginProxy::URLRedirectResponse(bool allow, int resource_id) {
  Send(new PluginHostMsg_URLRedirectResponse(route_id_, allow, resource_id));
}

#if defined(OS_WIN) && !defined(USE_AURA)
void WebPluginProxy::UpdateIMEStatus() {
  // Retrieve the IME status from a plug-in and send it to a renderer process
  // when the plug-in has updated it.
  int input_type;
  gfx::Rect caret_rect;
  if (!delegate_->GetIMEStatus(&input_type, &caret_rect))
    return;

  Send(new PluginHostMsg_NotifyIMEStatus(route_id_, input_type, caret_rect));
}
#endif
