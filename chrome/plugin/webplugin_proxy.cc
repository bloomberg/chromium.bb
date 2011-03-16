// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/plugin/webplugin_proxy.h"

#include "build/build_config.h"

#include "base/lazy_instance.h"
#include "base/scoped_handle.h"
#include "base/shared_memory.h"
#include "build/build_config.h"
#include "chrome/plugin/npobject_proxy.h"
#include "chrome/plugin/npobject_util.h"
#include "chrome/plugin/plugin_channel.h"
#include "chrome/plugin/plugin_thread.h"
#include "content/common/content_client.h"
#include "content/common/plugin_messages.h"
#include "skia/ext/platform_device.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebBindings.h"
#include "ui/gfx/blit.h"
#include "ui/gfx/canvas.h"
#include "webkit/plugins/npapi/webplugin_delegate_impl.h"

#if defined(OS_MACOSX)
#include "base/mac/mac_util.h"
#include "base/mac/scoped_cftyperef.h"
#include "chrome/plugin/webplugin_accelerated_surface_proxy_mac.h"
#endif

#if defined(OS_WIN)
#include "content/common/section_util_win.h"
#include "ui/gfx/gdi_util.h"
#endif

#if defined(USE_X11)
#include "ui/base/x/x11_util_internal.h"
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
      host_render_view_routing_id_(host_render_view_routing_id),
      ALLOW_THIS_IN_INITIALIZER_LIST(runnable_method_factory_(this)) {
#if defined(USE_X11)
      windowless_shm_pixmap_ = None;
      use_shm_pixmap_ = false;

      // If the X server supports SHM pixmaps
      // and the color depth and masks match,
      // then consider using SHM pixmaps for windowless plugin painting.
      Display* display = ui::GetXDisplay();
      if (ui::QuerySharedMemorySupport(display) == ui::SHARED_MEMORY_PIXMAP &&
          ui::BitsPerPixelForPixmapDepth(
              display, DefaultDepth(display, 0)) == 32) {
        Visual* vis = DefaultVisual(display, 0);

        if (vis->red_mask == 0xff0000 &&
            vis->green_mask == 0xff00 &&
            vis->blue_mask == 0xff)
          use_shm_pixmap_ = true;
      }
#endif
}

WebPluginProxy::~WebPluginProxy() {
#if defined(USE_X11)
  if (windowless_shm_pixmap_ != None)
    XFreePixmap(ui::GetXDisplay(), windowless_shm_pixmap_);
#endif

#if defined(OS_MACOSX)
  // Destroy the surface early, since it may send messages during cleanup.
  if (accelerated_surface_.get())
    accelerated_surface_.reset();
#endif
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
  DuplicateHandle(GetCurrentProcess(), pump_messages_event,
                  channel_->renderer_handle(),
                  &pump_messages_event_for_renderer,
                  0, FALSE, DUPLICATE_SAME_ACCESS);
  DCHECK(pump_messages_event_for_renderer != NULL);
  Send(new PluginHostMsg_SetWindowlessPumpEvent(
      route_id_, pump_messages_event_for_renderer));
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
  if (!windowless_context_.get()) {
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
        runnable_method_factory_.NewRunnableMethod(
            &WebPluginProxy::OnPaint, damaged_rect_));
    damaged_rect_ = gfx::Rect();
  }
}

NPObject* WebPluginProxy::GetWindowScriptNPObject() {
  if (window_npobject_)
    return WebBindings::retainObject(window_npobject_);

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
    return WebBindings::retainObject(plugin_element_);

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

void WebPluginProxy::OnMissingPluginStatus(int status) {
  Send(new PluginHostMsg_MissingPluginStatus(route_id_, status));
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
      if (!request_url.SchemeIs("http") &&
          !request_url.SchemeIs("https") &&
          !request_url.SchemeIs("ftp")) {
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
  if (!windowless_context_.get())
    return;
#else
  if (!windowless_canvas_.get())
    return;
#endif

  // Clear the damaged area so that if the plugin doesn't paint there we won't
  // end up with the old values.
  gfx::Rect offset_rect = rect;
  offset_rect.Offset(delegate_->GetRect().origin());
#if defined(OS_MACOSX)
  CGContextSaveGState(windowless_context_);
  // It is possible for windowless_context_ to change during plugin painting
  // (since the plugin can make a synchronous call during paint event handling),
  // in which case we don't want to try to restore it later. Not an owning ref
  // since owning the ref without owning the shared backing memory doesn't make
  // sense, so this should only be used for pointer comparisons.
  CGContextRef saved_context_weak = windowless_context_.get();

  if (background_context_.get()) {
    base::mac::ScopedCFTypeRef<CGImageRef> image(
        CGBitmapContextCreateImage(background_context_));
    CGRect source_rect = rect.ToCGRect();
    // Flip the rect we use to pull from the canvas, since it's upside-down.
    source_rect.origin.y = CGImageGetHeight(image) - rect.y() - rect.height();
    base::mac::ScopedCFTypeRef<CGImageRef> sub_image(
        CGImageCreateWithImageInRect(image, source_rect));
    CGContextDrawImage(windowless_context_, rect.ToCGRect(), sub_image);
  } else if (transparent_) {
    CGContextClearRect(windowless_context_, rect.ToCGRect());
  }
  CGContextClipToRect(windowless_context_, rect.ToCGRect());
  delegate_->Paint(windowless_context_, rect);
  if (windowless_context_.get() == saved_context_weak)
    CGContextRestoreGState(windowless_context_);
#else
  windowless_canvas_->save();

  // The given clip rect is relative to the plugin coordinate system.
  SkRect sk_rect = { SkIntToScalar(rect.x()),
                     SkIntToScalar(rect.y()),
                     SkIntToScalar(rect.right()),
                     SkIntToScalar(rect.bottom()) };
  windowless_canvas_->clipRect(sk_rect);

  // Setup the background.
  if (background_canvas_.get()) {
    // When a background canvas is given, we're in transparent mode. This means
    // the plugin wants to have the image of the page in the canvas it's drawing
    // into (which is windowless_canvas_) so it can do blending. So we copy the
    // background bitmap into the windowless_canvas_.
    const SkBitmap& background_bitmap =
        background_canvas_->getTopPlatformDevice().accessBitmap(false);
    windowless_canvas_->drawBitmap(background_bitmap, 0, 0);
  } else {
    // In non-transparent mode, the plugin doesn't care what's underneath, so we
    // can just give it black.
    SkPaint black_fill_paint;
    black_fill_paint.setARGB(0xFF, 0x00, 0x00, 0x00);
    windowless_canvas_->drawPaint(black_fill_paint);
  }

  // Bring the windowless_canvas_ into the window coordinate system, which is
  // how the plugin expects to draw (since the windowless API was originally
  // designed just for scribbling over the web page).
  windowless_canvas_->translate(SkIntToScalar(-delegate_->GetRect().x()),
                                SkIntToScalar(-delegate_->GetRect().y()));

  // Before we send the invalidate, paint so that renderer uses the updated
  // bitmap.
  delegate_->Paint(windowless_canvas_.get(), offset_rect);

  windowless_canvas_->restore();
#endif
}

void WebPluginProxy::UpdateGeometry(
    const gfx::Rect& window_rect,
    const gfx::Rect& clip_rect,
    const TransportDIB::Handle& windowless_buffer,
    const TransportDIB::Handle& background_buffer,
    bool transparent
#if defined(OS_MACOSX)
    ,
    int ack_key
#endif
    ) {
  gfx::Rect old = delegate_->GetRect();
  gfx::Rect old_clip_rect = delegate_->GetClipRect();
  transparent_ = transparent;

  // Update the buffers before doing anything that could call into plugin code,
  // so that we don't process buffer changes out of order if plugins make
  // synchronous calls that lead to nested UpdateGeometry calls.
  if (TransportDIB::is_valid(windowless_buffer)) {
    // The plugin's rect changed, so now we have a new buffer to draw into.
    SetWindowlessBuffer(windowless_buffer, background_buffer, window_rect);
  }

#if defined(OS_MACOSX)
  delegate_->UpdateGeometryAndContext(window_rect, clip_rect,
                                      windowless_context_);
#else
  delegate_->UpdateGeometry(window_rect, clip_rect);
#endif

  // Send over any pending invalidates which occured when the plugin was
  // off screen.
  if (delegate_->IsWindowless() && !clip_rect.IsEmpty() &&
      !damaged_rect_.IsEmpty()) {
    InvalidateRect(damaged_rect_);
  }

#if defined(OS_MACOSX)
  // The renderer is expecting an ACK message if ack_key is not -1.
  if (ack_key != -1) {
    Send(new PluginHostMsg_UpdateGeometry_ACK(route_id_, ack_key));
  }
#endif
}

#if defined(OS_WIN)
void WebPluginProxy::SetWindowlessBuffer(
    const TransportDIB::Handle& windowless_buffer,
    const TransportDIB::Handle& background_buffer,
    const gfx::Rect& window_rect) {
  // Create a canvas that will reference the shared bits. We have to handle
  // errors here since we're mapping a large amount of memory that may not fit
  // in our address space, or go wrong in some other way.
  windowless_canvas_.reset(new skia::PlatformCanvas);
  if (!windowless_canvas_->initialize(
          window_rect.width(),
          window_rect.height(),
          true,
          chrome::GetSectionFromProcess(windowless_buffer,
              channel_->renderer_handle(), false))) {
    windowless_canvas_.reset();
    background_canvas_.reset();
    return;
  }

  if (background_buffer) {
    background_canvas_.reset(new skia::PlatformCanvas);
    if (!background_canvas_->initialize(
            window_rect.width(),
            window_rect.height(),
            true,
            chrome::GetSectionFromProcess(background_buffer,
                channel_->renderer_handle(), false))) {
      windowless_canvas_.reset();
      background_canvas_.reset();
      return;
    }
  }
}

#elif defined(OS_MACOSX)

void WebPluginProxy::SetWindowlessBuffer(
    const TransportDIB::Handle& windowless_buffer,
    const TransportDIB::Handle& background_buffer,
    const gfx::Rect& window_rect) {
  // Convert the shared memory handle to a handle that works in our process,
  // and then use that to create a CGContextRef.
  windowless_dib_.reset(TransportDIB::Map(windowless_buffer));
  background_dib_.reset(TransportDIB::Map(background_buffer));
  windowless_context_.reset(CGBitmapContextCreate(
      windowless_dib_->memory(),
      window_rect.width(),
      window_rect.height(),
      8, 4 * window_rect.width(),
      base::mac::GetSystemColorSpace(),
      kCGImageAlphaPremultipliedFirst |
      kCGBitmapByteOrder32Host));
  CGContextTranslateCTM(windowless_context_, 0, window_rect.height());
  CGContextScaleCTM(windowless_context_, 1, -1);
  if (background_dib_.get()) {
    background_context_.reset(CGBitmapContextCreate(
        background_dib_->memory(),
        window_rect.width(),
        window_rect.height(),
        8, 4 * window_rect.width(),
        base::mac::GetSystemColorSpace(),
        kCGImageAlphaPremultipliedFirst |
        kCGBitmapByteOrder32Host));
    CGContextTranslateCTM(background_context_, 0, window_rect.height());
    CGContextScaleCTM(background_context_, 1, -1);
  }
}

#elif defined(USE_X11)

void WebPluginProxy::SetWindowlessBuffer(
    const TransportDIB::Handle& windowless_buffer,
    const TransportDIB::Handle& background_buffer,
    const gfx::Rect& window_rect) {
  int width = window_rect.width();
  int height = window_rect.height();
  windowless_dib_.reset(TransportDIB::Map(windowless_buffer));
  if (windowless_dib_.get()) {
    windowless_canvas_.reset(windowless_dib_->GetPlatformCanvas(width, height));
  } else {
    // This can happen if the renderer has already destroyed the TransportDIB
    // by the time we receive the handle, e.g. in case of multiple resizes.
    windowless_canvas_.reset();
  }
  background_dib_.reset(TransportDIB::Map(background_buffer));
  if (background_dib_.get()) {
    background_canvas_.reset(background_dib_->GetPlatformCanvas(width, height));
  } else {
    background_canvas_.reset();
  }

  // If SHM pixmaps support is available, create a SHM pixmap and
  // pass it to the delegate for windowless plugin painting.
  if (delegate_->IsWindowless() && use_shm_pixmap_ && windowless_dib_.get()) {
    Display* display = ui::GetXDisplay();
    XID root_window = ui::GetX11RootWindow();
    XShmSegmentInfo shminfo = {0};

    if (windowless_shm_pixmap_ != None)
      XFreePixmap(display, windowless_shm_pixmap_);

    shminfo.shmseg = windowless_dib_->MapToX(display);
    // Create a shared memory pixmap based on the image buffer.
    windowless_shm_pixmap_ = XShmCreatePixmap(display, root_window,
                                              NULL, &shminfo,
                                              width, height,
                                              DefaultDepth(display, 0));

    delegate_->SetWindowlessShmPixmap(windowless_shm_pixmap_);
  }
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

WebPluginAcceleratedSurface* WebPluginProxy::GetAcceleratedSurface() {
  if (!accelerated_surface_.get())
    accelerated_surface_.reset(new WebPluginAcceleratedSurfaceProxy(this));
  return accelerated_surface_.get();
}

void WebPluginProxy::AcceleratedFrameBuffersDidSwap(
    gfx::PluginWindowHandle window, uint64 surface_id) {
  Send(new PluginHostMsg_AcceleratedSurfaceBuffersSwapped(
        route_id_, window, surface_id));
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
#endif

void WebPluginProxy::OnPaint(const gfx::Rect& damaged_rect) {
  content::GetContentClient()->SetActiveURL(page_url_);

  Paint(damaged_rect);
  Send(new PluginHostMsg_InvalidateRect(route_id_, damaged_rect));
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
