// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PLUGIN_WEBPLUGIN_PROXY_H_
#define CONTENT_PLUGIN_WEBPLUGIN_PROXY_H_

#include <stdint.h>

#include <string>

#include "base/containers/hash_tables.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "build/build_config.h"
#include "content/child/npapi/webplugin.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_sender.h"
#include "skia/ext/refptr.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "ui/gl/gpu_preference.h"
#include "ui/surface/transport_dib.h"
#include "url/gurl.h"

#if defined(OS_MACOSX)
#include <ApplicationServices/ApplicationServices.h>

#include "base/mac/scoped_cftyperef.h"
#endif

namespace content {
class PluginChannel;
class WebPluginDelegateImpl;

#if defined(OS_MACOSX)
class WebPluginAcceleratedSurfaceProxy;
#endif

// This is an implementation of WebPlugin that proxies all calls to the
// renderer.
class WebPluginProxy : public WebPlugin,
                       public IPC::Sender {
 public:
  // Creates a new proxy for WebPlugin, using the given sender to send the
  // marshalled WebPlugin calls.
  WebPluginProxy(PluginChannel* channel,
                 int route_id,
                 const GURL& page_url,
                 int host_render_view_routing_id);
  ~WebPluginProxy() override;

  void set_delegate(WebPluginDelegateImpl* d) { delegate_ = d; }

  // WebPlugin overrides
  void SetWindow(gfx::PluginWindowHandle window) override;
  void SetAcceptsInputEvents(bool accepts) override;
  void WillDestroyWindow(gfx::PluginWindowHandle window) override;
  void Invalidate() override;
  void InvalidateRect(const gfx::Rect& rect) override;
  NPObject* GetWindowScriptNPObject() override;
  NPObject* GetPluginElement() override;
  bool FindProxyForUrl(const GURL& url, std::string* proxy_list) override;
  void SetCookie(const GURL& url,
                 const GURL& first_party_for_cookies,
                 const std::string& cookie) override;
  std::string GetCookies(const GURL& url,
                         const GURL& first_party_for_cookies) override;
  void UpdateGeometry(const gfx::Rect& window_rect,
                      const gfx::Rect& clip_rect,
                      const TransportDIB::Handle& windowless_buffer0,
                      const TransportDIB::Handle& windowless_buffer1,
                      int windowless_buffer_index);
  void CancelDocumentLoad() override;
  void DidStartLoading() override;
  void DidStopLoading() override;
  bool IsOffTheRecord() override;
#if defined(OS_WIN)
  void SetWindowlessData(HANDLE pump_messages_event,
                         gfx::NativeViewId dummy_activation_window) override;
#endif
#if defined(OS_MACOSX)
  void FocusChanged(bool focused) override;
  void StartIme() override;
  WebPluginAcceleratedSurface* GetAcceleratedSurface(
      gfx::GpuPreference gpu_preference) override;
  void AcceleratedPluginEnabledRendering() override;
  void AcceleratedPluginAllocatedIOSurface(int32_t width,
                                           int32_t height,
                                           uint32_t surface_id) override;
  void AcceleratedPluginSwappedIOSurface() override;
#endif

  // IPC::Sender implementation.
  bool Send(IPC::Message* msg) override;

  // class-specific methods

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

#if defined(OS_WIN) && !defined(USE_AURA)
  // Retrieves the IME status from a windowless plugin and sends it to a
  // renderer process. A renderer process will convert the coordinates from
  // local to the window coordinates and send the converted coordinates to a
  // browser process.
  void UpdateIMEStatus();
#endif

 private:
  class SharedTransportDIB : public base::RefCounted<SharedTransportDIB> {
   public:
    explicit SharedTransportDIB(TransportDIB* dib);
    TransportDIB* dib() { return dib_.get(); }
   private:
    friend class base::RefCounted<SharedTransportDIB>;
    ~SharedTransportDIB();

    scoped_ptr<TransportDIB> dib_;
  };

  // Handler for sending over the paint event to the plugin.
  void OnPaint(const gfx::Rect& damaged_rect);

#if defined(OS_WIN)
  void CreateCanvasFromHandle(const TransportDIB::Handle& dib_handle,
                              const gfx::Rect& window_rect,
                              skia::RefPtr<SkCanvas>* canvas);
#elif defined(OS_MACOSX)
  static void CreateDIBAndCGContextFromHandle(
      const TransportDIB::Handle& dib_handle,
      const gfx::Rect& window_rect,
      scoped_ptr<TransportDIB>* dib_out,
      base::ScopedCFTypeRef<CGContextRef>* cg_context_out);
#endif

  // Updates the shared memory sections where windowless plugins paint.
  void SetWindowlessBuffers(const TransportDIB::Handle& windowless_buffer0,
                            const TransportDIB::Handle& windowless_buffer1,
                            const gfx::Rect& window_rect);

#if defined(OS_MACOSX)
  CGContextRef windowless_context() const {
    return windowless_contexts_[windowless_buffer_index_].get();
  }
#else
  skia::RefPtr<SkCanvas> windowless_canvas() const {
    return windowless_canvases_[windowless_buffer_index_];
  }
#endif

  scoped_refptr<PluginChannel> channel_;
  int route_id_;
  NPObject* window_npobject_;
  NPObject* plugin_element_;
  WebPluginDelegateImpl* delegate_;
  gfx::Rect damaged_rect_;
  bool waiting_for_paint_;
  // The url of the main frame hosting the plugin.
  GURL page_url_;

  // Variables used for desynchronized windowless plugin painting. See note in
  // webplugin_delegate_proxy.h for how this works. The two sets of windowless_*
  // fields are for the front-buffer and back-buffer of a buffer flipping system
  // and windowless_buffer_index_ identifies which set we are using as the
  // back-buffer at any given time.
  int windowless_buffer_index_;
#if defined(OS_MACOSX)
  scoped_ptr<TransportDIB> windowless_dibs_[2];
  base::ScopedCFTypeRef<CGContextRef> windowless_contexts_[2];
  scoped_ptr<WebPluginAcceleratedSurfaceProxy> accelerated_surface_;
#else
  skia::RefPtr<SkCanvas> windowless_canvases_[2];
#endif

  // Contains the routing id of the host render view.
  int host_render_view_routing_id_;

  base::WeakPtrFactory<WebPluginProxy> weak_factory_;
};

}  // namespace content

#endif  // CONTENT_PLUGIN_WEBPLUGIN_PROXY_H_
