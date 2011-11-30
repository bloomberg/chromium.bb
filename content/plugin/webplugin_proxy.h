// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PLUGIN_WEBPLUGIN_PROXY_H_
#define CONTENT_PLUGIN_WEBPLUGIN_PROXY_H_
#pragma once

#include <string>

#include "base/hash_tables.h"
#include "base/memory/ref_counted.h"
#if defined(OS_MACOSX)
#include "base/mac/scoped_cftyperef.h"
#endif
#include "base/memory/scoped_handle.h"
#include "base/memory/scoped_ptr.h"
#include "base/shared_memory.h"
#include "base/timer.h"
#include "googleurl/src/gurl.h"
#include "ipc/ipc_message.h"
#if defined(USE_X11)
#include "ui/base/x/x11_util.h"
#endif
#include "ui/gfx/gl/gpu_preference.h"
#include "ui/gfx/surface/transport_dib.h"
#include "webkit/plugins/npapi/webplugin.h"

class PluginChannel;

namespace skia {
class PlatformCanvas;
}

namespace webkit {
namespace npapi {
class WebPluginDelegateImpl;
}
}

#if defined(OS_MACOSX)
class WebPluginAcceleratedSurfaceProxy;
#endif

// This is an implementation of WebPlugin that proxies all calls to the
// renderer.
class WebPluginProxy : public webkit::npapi::WebPlugin {
 public:
  // Creates a new proxy for WebPlugin, using the given sender to send the
  // marshalled WebPlugin calls.
  WebPluginProxy(PluginChannel* channel,
                 int route_id,
                 const GURL& page_url,
                 gfx::NativeViewId containing_window,
                 int host_render_view_routing_id);
  virtual ~WebPluginProxy();

  void set_delegate(webkit::npapi::WebPluginDelegateImpl* d) { delegate_ = d; }

  // WebPlugin overrides
  virtual void SetWindow(gfx::PluginWindowHandle window) OVERRIDE;

  // Whether input events should be sent to the delegate.
  virtual void SetAcceptsInputEvents(bool accepts) OVERRIDE;

  virtual void WillDestroyWindow(gfx::PluginWindowHandle window) OVERRIDE;
#if defined(OS_WIN)
  void SetWindowlessPumpEvent(HANDLE pump_messages_event);
  void ReparentPluginWindow(HWND window, HWND parent);
#endif

  virtual void CancelResource(unsigned long id) OVERRIDE;
  virtual void Invalidate() OVERRIDE;
  virtual void InvalidateRect(const gfx::Rect& rect) OVERRIDE;
  virtual NPObject* GetWindowScriptNPObject() OVERRIDE;
  virtual NPObject* GetPluginElement() OVERRIDE;
  virtual bool FindProxyForUrl(const GURL& url,
                               std::string* proxy_list) OVERRIDE;
  virtual void SetCookie(const GURL& url,
                         const GURL& first_party_for_cookies,
                         const std::string& cookie) OVERRIDE;
  virtual std::string GetCookies(const GURL& url,
                                 const GURL& first_party_for_cookies) OVERRIDE;

  // class-specific methods

  // Returns a WebPluginResourceClient object given its id, or NULL if no
  // object with that id exists.
  webkit::npapi::WebPluginResourceClient* GetResourceClient(int id);

  // Returns the id of the renderer that contains this plugin.
  int GetRendererId();

  // Returns the id of the associated render view.
  int host_render_view_routing_id() const {
    return host_render_view_routing_id_;
  }

  // For windowless plugins, paints the given rectangle into the local buffer.
  void Paint(const gfx::Rect& rect);

  // Callback from the renderer to let us know that a paint occurred.
  void DidPaint();

  // Notification received on a plugin issued resource request creation.
  void OnResourceCreated(int resource_id,
                         webkit::npapi::WebPluginResourceClient* client);

  virtual void HandleURLRequest(const char* url,
                                const char* method,
                                const char* target,
                                const char* buf,
                                unsigned int len,
                                int notify_id,
                                bool popups_allowed,
                                bool notify_redirects) OVERRIDE;
  void UpdateGeometry(const gfx::Rect& window_rect,
                      const gfx::Rect& clip_rect,
                      const TransportDIB::Handle& windowless_buffer0,
                      const TransportDIB::Handle& windowless_buffer1,
                      int windowless_buffer_index,
                      const TransportDIB::Handle& background_buffer,
                      bool transparent);
  virtual void CancelDocumentLoad() OVERRIDE;
  virtual void InitiateHTTPRangeRequest(
      const char* url, const char* range_info, int range_request_id) OVERRIDE;
  virtual void SetDeferResourceLoading(unsigned long resource_id,
                                       bool defer) OVERRIDE;
  virtual bool IsOffTheRecord() OVERRIDE;
  virtual void ResourceClientDeleted(
      webkit::npapi::WebPluginResourceClient* resource_client) OVERRIDE;
  gfx::NativeViewId containing_window() { return containing_window_; }

#if defined(OS_MACOSX)
  virtual void FocusChanged(bool focused) OVERRIDE;

  virtual void StartIme() OVERRIDE;

  virtual webkit::npapi::WebPluginAcceleratedSurface*
      GetAcceleratedSurface(gfx::GpuPreference gpu_preference) OVERRIDE;

  //----------------------------------------------------------------------
  // Legacy Core Animation plugin implementation rendering directly to screen.

  virtual void BindFakePluginWindowHandle(bool opaque) OVERRIDE;

  // Tell the browser (via the renderer) to invalidate because the
  // accelerated buffers have changed.
  virtual void AcceleratedFrameBuffersDidSwap(
      gfx::PluginWindowHandle window, uint64 surface_id);

  // Tell the renderer and browser to associate the given plugin handle with
  // |accelerated_surface_identifier|. The geometry is used to resize any
  // native "window" (which on the Mac is a just a view).
  // This method is used when IOSurface support is available.
  virtual void SetAcceleratedSurface(gfx::PluginWindowHandle window,
                                     const gfx::Size& size,
                                     uint64 accelerated_surface_identifier);

  // Tell the renderer and browser to associate the given plugin handle with
  // |dib_handle|. The geometry is used to resize any native "window" (which
  // on the Mac is just a view).
  // This method is used when IOSurface support is not available.
  virtual void SetAcceleratedDIB(
      gfx::PluginWindowHandle window,
      const gfx::Size& size,
      const TransportDIB::Handle& dib_handle);

  // Create/destroy TranportDIBs via messages to the browser process.
  // These are only used when IOSurface support is not available.
  virtual void AllocSurfaceDIB(const size_t size,
                               TransportDIB::Handle* dib_handle);
  virtual void FreeSurfaceDIB(TransportDIB::Id dib_id);

  //----------------------------------------------------------------------
  // New accelerated plugin implementation which renders via the compositor.

  // Tells the renderer, and from there the GPU process, that the plugin
  // is using accelerated rather than software rendering.
  virtual void AcceleratedPluginEnabledRendering() OVERRIDE;

  // Tells the renderer, and from there the GPU process, that the plugin
  // allocated the given IOSurface to be used as its backing store.
  virtual void AcceleratedPluginAllocatedIOSurface(int32 width,
                                                   int32 height,
                                                   uint32 surface_id) OVERRIDE;
  virtual void AcceleratedPluginSwappedIOSurface() OVERRIDE;
#endif

  virtual void URLRedirectResponse(bool allow, int resource_id) OVERRIDE;

#if defined(OS_WIN) && !defined(USE_AURA)
  // Retrieves the IME status from a windowless plug-in and sends it to a
  // renderer process. A renderer process will convert the coordinates from
  // local to the window coordinates and send the converted coordinates to a
  // browser process.
  void UpdateIMEStatus();
#endif

 private:
  bool Send(IPC::Message* msg);

  // Handler for sending over the paint event to the plugin.
  void OnPaint(const gfx::Rect& damaged_rect);

#if defined(OS_WIN)
  void CreateCanvasFromHandle(const TransportDIB::Handle& dib_handle,
                              const gfx::Rect& window_rect,
                              scoped_ptr<skia::PlatformCanvas>* canvas_out);
#elif defined(OS_MACOSX)
  static void CreateDIBAndCGContextFromHandle(
      const TransportDIB::Handle& dib_handle,
      const gfx::Rect& window_rect,
      scoped_ptr<TransportDIB>* dib_out,
      base::mac::ScopedCFTypeRef<CGContextRef>* cg_context_out);
#elif defined(USE_X11)
  static void CreateDIBAndCanvasFromHandle(
      const TransportDIB::Handle& dib_handle,
      const gfx::Rect& window_rect,
      scoped_ptr<TransportDIB>* dib_out,
      scoped_ptr<skia::PlatformCanvas>* canvas_out);

  static void CreateShmPixmapFromDIB(
      TransportDIB* dib,
      const gfx::Rect& window_rect,
      XID* pixmap_out);
#endif

  // Updates the shared memory sections where windowless plugins paint.
  void SetWindowlessBuffers(const TransportDIB::Handle& windowless_buffer0,
                            const TransportDIB::Handle& windowless_buffer1,
                            const TransportDIB::Handle& background_buffer,
                            const gfx::Rect& window_rect);

#if defined(OS_MACOSX)
  CGContextRef windowless_context() const {
    return windowless_contexts_[windowless_buffer_index_].get();
  }
#else
  skia::PlatformCanvas* windowless_canvas() const {
    return windowless_canvases_[windowless_buffer_index_].get();
  }

#if defined(USE_X11)
  XID windowless_shm_pixmap() const {
    return windowless_shm_pixmaps_[windowless_buffer_index_];
  }
#endif

#endif

  typedef base::hash_map<int, webkit::npapi::WebPluginResourceClient*>
      ResourceClientMap;
  ResourceClientMap resource_clients_;

  scoped_refptr<PluginChannel> channel_;
  int route_id_;
  NPObject* window_npobject_;
  NPObject* plugin_element_;
  webkit::npapi::WebPluginDelegateImpl* delegate_;
  gfx::Rect damaged_rect_;
  bool waiting_for_paint_;
  gfx::NativeViewId containing_window_;
  // The url of the main frame hosting the plugin.
  GURL page_url_;

  // Variables used for desynchronized windowless plugin painting. See note in
  // webplugin_delegate_proxy.h for how this works. The two sets of windowless_*
  // fields are for the front-buffer and back-buffer of a buffer flipping system
  // and windowless_buffer_index_ identifies which set we are using as the
  // back-buffer at any given time.
  bool transparent_;
  int windowless_buffer_index_;
#if defined(OS_MACOSX)
  scoped_ptr<TransportDIB> windowless_dibs_[2];
  scoped_ptr<TransportDIB> background_dib_;
  base::mac::ScopedCFTypeRef<CGContextRef> windowless_contexts_[2];
  base::mac::ScopedCFTypeRef<CGContextRef> background_context_;
  scoped_ptr<WebPluginAcceleratedSurfaceProxy> accelerated_surface_;
#else
  scoped_ptr<skia::PlatformCanvas> windowless_canvases_[2];
  scoped_ptr<skia::PlatformCanvas> background_canvas_;

#if defined(USE_X11)
  scoped_ptr<TransportDIB> windowless_dibs_[2];
  scoped_ptr<TransportDIB> background_dib_;
  // If we can use SHM pixmaps for windowless plugin painting or not.
  bool use_shm_pixmap_;
  // The SHM pixmaps for windowless plugin painting.
  XID windowless_shm_pixmaps_[2];
#endif

#endif

  // Contains the routing id of the host render view.
  int host_render_view_routing_id_;

  base::WeakPtrFactory<WebPluginProxy> weak_factory_;
};

#endif  // CONTENT_PLUGIN_WEBPLUGIN_PROXY_H_
