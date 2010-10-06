// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_PLUGIN_WEBPLUGIN_PROXY_H_
#define CHROME_PLUGIN_WEBPLUGIN_PROXY_H_
#pragma once

#include <string>

#if defined(USE_X11)
#include "app/x11_util.h"
#endif
#include "app/surface/transport_dib.h"
#include "base/hash_tables.h"
#include "base/ref_counted.h"
#if defined(OS_MACOSX)
#include "base/scoped_cftyperef.h"
#endif
#include "base/scoped_handle.h"
#include "base/scoped_ptr.h"
#include "base/shared_memory.h"
#include "base/timer.h"
#include "chrome/common/chrome_plugin_api.h"
#include "googleurl/src/gurl.h"
#include "ipc/ipc_message.h"
#include "webkit/glue/plugins/webplugin.h"

class PluginChannel;
class WebPluginDelegateImpl;
#if defined(OS_MACOSX)
class WebPluginAcceleratedSurfaceProxy;
#endif

// This is an implementation of WebPlugin that proxies all calls to the
// renderer.
class WebPluginProxy : public webkit_glue::WebPlugin {
 public:
  // Creates a new proxy for WebPlugin, using the given sender to send the
  // marshalled WebPlugin calls.
  WebPluginProxy(PluginChannel* channel,
                 int route_id,
                 const GURL& page_url,
                 gfx::NativeViewId containing_window,
                 int host_render_view_routing_id);
  ~WebPluginProxy();

  void set_delegate(WebPluginDelegateImpl* d) { delegate_ = d; }

  // WebPlugin overrides
  void SetWindow(gfx::PluginWindowHandle window);

  // Whether input events should be sent to the delegate.
  virtual void SetAcceptsInputEvents(bool accepts) {
    NOTREACHED();
  }

  void WillDestroyWindow(gfx::PluginWindowHandle window);
#if defined(OS_WIN)
  void SetWindowlessPumpEvent(HANDLE pump_messages_event);
#endif

  void CancelResource(unsigned long id);
  void Invalidate();
  void InvalidateRect(const gfx::Rect& rect);
  NPObject* GetWindowScriptNPObject();
  NPObject* GetPluginElement();
  void SetCookie(const GURL& url,
                 const GURL& first_party_for_cookies,
                 const std::string& cookie);
  std::string GetCookies(const GURL& url, const GURL& first_party_for_cookies);

  void ShowModalHTMLDialog(const GURL& url, int width, int height,
                           const std::string& json_arguments,
                           std::string* json_retval);

  // Called by gears over the CPAPI interface to verify that the given event is
  // the current (javascript) drag event the browser is dispatching, and return
  // the drag data, or control the drop effect (drag cursor), if so.
  bool GetDragData(struct NPObject* event, bool add_data, int32* identity,
                   int32* event_id, std::string* type, std::string* data);
  bool SetDropEffect(struct NPObject* event, int effect);

  void OnMissingPluginStatus(int status);
  // class-specific methods

  // Retrieves the browsing context associated with the renderer this plugin
  // is in.  Calling multiple times will return the same value.
  CPBrowsingContext GetCPBrowsingContext();

  // Retrieves the WebPluginProxy for the given context that was returned by
  // GetCPBrowsingContext, or NULL if not found.
  static WebPluginProxy* FromCPBrowsingContext(CPBrowsingContext context);

  // Returns a WebPluginResourceClient object given its id, or NULL if no
  // object with that id exists.
  webkit_glue::WebPluginResourceClient* GetResourceClient(int id);

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
                         webkit_glue::WebPluginResourceClient* client);

  void HandleURLRequest(const char* url,
                        const char* method,
                        const char* target,
                        const char* buf,
                        unsigned int len,
                        int notify_id,
                        bool popups_allowed);
  void UpdateGeometry(const gfx::Rect& window_rect,
                      const gfx::Rect& clip_rect,
                      const TransportDIB::Handle& windowless_buffer,
                      const TransportDIB::Handle& background_buffer,
                      bool transparent
#if defined(OS_MACOSX)
                      ,
                      int ack_key
#endif
                      );
  void CancelDocumentLoad();
  void InitiateHTTPRangeRequest(
      const char* url, const char* range_info, int range_request_id);
  void SetDeferResourceLoading(unsigned long resource_id, bool defer);
  bool IsOffTheRecord();
  void ResourceClientDeleted(
      webkit_glue::WebPluginResourceClient* resource_client);
  gfx::NativeViewId containing_window() { return containing_window_; }

#if defined(OS_MACOSX)
  virtual void BindFakePluginWindowHandle(bool opaque);

  virtual webkit_glue::WebPluginAcceleratedSurface* GetAcceleratedSurface();

  // Tell the browser (via the renderer) to invalidate because the
  // accelerated buffers have changed.
  virtual void AcceleratedFrameBuffersDidSwap(gfx::PluginWindowHandle window);

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
#endif

 private:
  bool Send(IPC::Message* msg);

  // Handler for sending over the paint event to the plugin.
  void OnPaint(const gfx::Rect& damaged_rect);

  // Updates the shared memory section where windowless plugins paint.
  void SetWindowlessBuffer(const TransportDIB::Handle& windowless_buffer,
                           const TransportDIB::Handle& background_buffer,
                           const gfx::Rect& window_rect);

  typedef base::hash_map<int, webkit_glue::WebPluginResourceClient*>
      ResourceClientMap;
  ResourceClientMap resource_clients_;

  scoped_refptr<PluginChannel> channel_;
  int route_id_;
  uint32 cp_browsing_context_;
  NPObject* window_npobject_;
  NPObject* plugin_element_;
  WebPluginDelegateImpl* delegate_;
  gfx::Rect damaged_rect_;
  bool waiting_for_paint_;
  gfx::NativeViewId containing_window_;
  // The url of the main frame hosting the plugin.
  GURL page_url_;

  // Variables used for desynchronized windowless plugin painting.  See note in
  // webplugin_delegate_proxy.h for how this works.
  bool transparent_;
#if defined(OS_MACOSX)
  scoped_ptr<TransportDIB> windowless_dib_;
  scoped_ptr<TransportDIB> background_dib_;
  scoped_cftyperef<CGContextRef> windowless_context_;
  scoped_cftyperef<CGContextRef> background_context_;
  scoped_ptr<WebPluginAcceleratedSurfaceProxy> accelerated_surface_;
#else
  scoped_ptr<skia::PlatformCanvas> windowless_canvas_;
  scoped_ptr<skia::PlatformCanvas> background_canvas_;

#if defined(USE_X11)
  scoped_ptr<TransportDIB> windowless_dib_;
  scoped_ptr<TransportDIB> background_dib_;
  // If we can use SHM pixmaps for windowless plugin painting or not.
  bool use_shm_pixmap_;
  // The SHM pixmap for windowless plugin painting.
  XID windowless_shm_pixmap_;
#endif

#endif

  // Contains the routing id of the host render view.
  int host_render_view_routing_id_;

  ScopedRunnableMethodFactory<WebPluginProxy> runnable_method_factory_;
};

#endif  // CHROME_PLUGIN_WEBPLUGIN_PROXY_H_
