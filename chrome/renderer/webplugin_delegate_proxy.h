// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_WEBPLUGIN_DELEGATE_PROXY_H_
#define CHROME_RENDERER_WEBPLUGIN_DELEGATE_PROXY_H_
#pragma once

#include <string>
#include <vector>

#include "app/surface/transport_dib.h"
#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "base/weak_ptr.h"
#include "gfx/native_widget_types.h"
#include "gfx/rect.h"
#include "googleurl/src/gurl.h"
#include "ipc/ipc_channel.h"
#include "ipc/ipc_message.h"
#include "webkit/plugins/npapi/webplugininfo.h"
#include "webkit/plugins/npapi/webplugin_delegate.h"

#if defined(OS_MACOSX)
#include "base/hash_tables.h"
#include "base/linked_ptr.h"
#endif

class CommandBufferProxy;
struct NPObject;
class NPObjectStub;
struct NPVariant_Param;
class PluginChannelHost;
struct PluginHostMsg_URLRequest_Params;
class RenderView;
class SkBitmap;

namespace base {
class SharedMemory;
class WaitableEvent;
}

namespace skia {
class PlatformCanvas;
}

namespace webkit {
namespace npapi {
class WebPlugin;
}
}

// An implementation of WebPluginDelegate that proxies all calls to
// the plugin process.
class WebPluginDelegateProxy
    : public webkit::npapi::WebPluginDelegate,
      public IPC::Channel::Listener,
      public IPC::Message::Sender,
      public base::SupportsWeakPtr<WebPluginDelegateProxy> {
 public:
  WebPluginDelegateProxy(const std::string& mime_type,
                         const base::WeakPtr<RenderView>& render_view);

  // WebPluginDelegate implementation:
  virtual void PluginDestroyed();
  virtual bool Initialize(const GURL& url,
                          const std::vector<std::string>& arg_names,
                          const std::vector<std::string>& arg_values,
                          webkit::npapi::WebPlugin* plugin,
                          bool load_manually);
  virtual void UpdateGeometry(const gfx::Rect& window_rect,
                              const gfx::Rect& clip_rect);
  virtual void Paint(WebKit::WebCanvas* canvas, const gfx::Rect& rect);
  virtual void Print(gfx::NativeDrawingContext context);
  virtual NPObject* GetPluginScriptableObject();
  virtual void DidFinishLoadWithReason(const GURL& url, NPReason reason,
                                       int notify_id);
  virtual void SetFocus(bool focused);
  virtual bool HandleInputEvent(const WebKit::WebInputEvent& event,
                                WebKit::WebCursorInfo* cursor);
  virtual int GetProcessId();

  // Informs the plugin that its containing content view has gained or lost
  // first responder status.
  virtual void SetContentAreaFocus(bool has_focus);
#if defined(OS_MACOSX)
  // Informs the plugin that its enclosing window has gained or lost focus.
  virtual void SetWindowFocus(bool window_has_focus);
  // Informs the plugin that its container (window/tab) has changed visibility.
  virtual void SetContainerVisibility(bool is_visible);
  // Informs the plugin that its enclosing window's frame has changed.
  virtual void WindowFrameChanged(gfx::Rect window_frame, gfx::Rect view_frame);
  // Informs the plugin that plugin IME has completed.
  // If |text| is empty, composition was cancelled.
  virtual void ImeCompositionCompleted(const string16& text, int plugin_id);
#endif

  // IPC::Channel::Listener implementation:
  virtual bool OnMessageReceived(const IPC::Message& msg);
  virtual void OnChannelError();

  // IPC::Message::Sender implementation:
  virtual bool Send(IPC::Message* msg);

  virtual void SendJavaScriptStream(const GURL& url,
                                    const std::string& result,
                                    bool success,
                                    int notify_id);

  virtual void DidReceiveManualResponse(const GURL& url,
                                        const std::string& mime_type,
                                        const std::string& headers,
                                        uint32 expected_length,
                                        uint32 last_modified);
  virtual void DidReceiveManualData(const char* buffer, int length);
  virtual void DidFinishManualLoading();
  virtual void DidManualLoadFail();
  virtual void InstallMissingPlugin();
  virtual webkit::npapi::WebPluginResourceClient* CreateResourceClient(
      unsigned long resource_id, const GURL& url, int notify_id);
  virtual webkit::npapi::WebPluginResourceClient* CreateSeekableResourceClient(
      unsigned long resource_id, int range_request_id);

  CommandBufferProxy* CreateCommandBuffer();
  void DestroyCommandBuffer(CommandBufferProxy* command_buffer);

  gfx::PluginWindowHandle GetPluginWindowHandle();

 protected:
  template<class WebPluginDelegateProxy> friend class DeleteTask;
  ~WebPluginDelegateProxy();

 private:
  // Message handlers for messages that proxy WebPlugin methods, which
  // we translate into calls to the real WebPlugin.
  void OnSetWindow(gfx::PluginWindowHandle window);
#if defined(OS_WIN)
  void OnSetWindowlessPumpEvent(HANDLE modal_loop_pump_messages_event);
#endif
  void OnCompleteURL(const std::string& url_in, std::string* url_out,
                     bool* result);
  void OnHandleURLRequest(const PluginHostMsg_URLRequest_Params& params);
  void OnCancelResource(int id);
  void OnInvalidateRect(const gfx::Rect& rect);
  void OnGetWindowScriptNPObject(int route_id, bool* success);
  void OnGetPluginElement(int route_id, bool* success);
  void OnSetCookie(const GURL& url,
                   const GURL& first_party_for_cookies,
                   const std::string& cookie);
  void OnGetCookies(const GURL& url, const GURL& first_party_for_cookies,
                    std::string* cookies);
  void OnShowModalHTMLDialog(const GURL& url, int width, int height,
                             const std::string& json_arguments,
                             std::string* json_retval);
  void OnGetDragData(const NPVariant_Param& event, bool add_data,
                     std::vector<NPVariant_Param>* values, bool* success);
  void OnSetDropEffect(const NPVariant_Param& event, int effect,
                       bool* success);
  void OnMissingPluginStatus(int status);
  void OnGetCPBrowsingContext(uint32* context);
  void OnCancelDocumentLoad();
  void OnInitiateHTTPRangeRequest(const std::string& url,
                                  const std::string& range_info,
                                  int range_request_id);
  void OnDeferResourceLoading(unsigned long resource_id, bool defer);

#if defined(OS_MACOSX)
  void OnFocusChanged(bool focused);
  void OnStartIme();
  void OnBindFakePluginWindowHandle(bool opaque);
  void OnUpdateGeometry_ACK(int ack_key);
  void OnAcceleratedSurfaceSetIOSurface(gfx::PluginWindowHandle window,
                                        int32 width,
                                        int32 height,
                                        uint64 io_surface_identifier);
  void OnAcceleratedSurfaceSetTransportDIB(gfx::PluginWindowHandle window,
                                           int32 width,
                                           int32 height,
                                           TransportDIB::Handle transport_dib);
  void OnAcceleratedSurfaceAllocTransportDIB(size_t size,
                                             TransportDIB::Handle* dib_handle);
  void OnAcceleratedSurfaceFreeTransportDIB(TransportDIB::Id dib_id);
  void OnAcceleratedSurfaceBuffersSwapped(gfx::PluginWindowHandle window,
                                          uint64 surface_id);
#endif

  void OnURLRedirectResponse(bool allow, int resource_id);

  // Draw a graphic indicating a crashed plugin.
  void PaintSadPlugin(WebKit::WebCanvas* canvas, const gfx::Rect& rect);

  // Returns true if the given rectangle is different in the native drawing
  // context and the current background bitmap.
  bool BackgroundChanged(gfx::NativeDrawingContext context,
                         const gfx::Rect& rect);

  // Copies the given rectangle from the transport bitmap to the backing store.
  void CopyFromTransportToBacking(const gfx::Rect& rect);

  // Clears the shared memory section and canvases used for windowless plugins.
  void ResetWindowlessBitmaps();

#if !defined(OS_WIN)
  // Creates a process-local memory section and canvas. PlatformCanvas on
  // Windows only works with a DIB, not arbitrary memory.
  bool CreateLocalBitmap(std::vector<uint8>* memory,
                         scoped_ptr<skia::PlatformCanvas>* canvas);
#endif

  // Creates a shared memory section and canvas.
  bool CreateSharedBitmap(scoped_ptr<TransportDIB>* memory,
                          scoped_ptr<skia::PlatformCanvas>* canvas);

  // Called for cleanup during plugin destruction. Normally right before the
  // plugin window gets destroyed, or when the plugin has crashed (at which
  // point the window has already been destroyed).
  void WillDestroyWindow();

#if defined(OS_MACOSX)
  // Synthesize a fake window handle for the plug-in to identify the instance
  // to the browser, allowing mapping to a surface for hardware acceleration
  // of plug-in content. The browser generates the handle which is then set on
  // the plug-in. Returns true if it successfully sets the window handle on the
  // plug-in.
  bool BindFakePluginWindowHandle(bool opaque);

  typedef base::hash_map<int, linked_ptr<TransportDIB> > OldTransportDIBMap;

  OldTransportDIBMap old_transport_dibs_;
#endif  // OS_MACOSX

#if defined(OS_WIN)
  // Returns true if we should update the plugin geometry synchronously.
  bool UseSynchronousGeometryUpdates();
#endif

  base::WeakPtr<RenderView> render_view_;
  webkit::npapi::WebPlugin* plugin_;
  bool uses_shared_bitmaps_;
  gfx::PluginWindowHandle window_;
  scoped_refptr<PluginChannelHost> channel_host_;
  std::string mime_type_;
  int instance_id_;
  webkit::npapi::WebPluginInfo info_;

  gfx::Rect plugin_rect_;
  gfx::Rect clip_rect_;

  NPObject* npobject_;
  base::WeakPtr<NPObjectStub> window_script_object_;

  // Event passed in by the plugin process and is used to decide if
  // messages need to be pumped in the NPP_HandleEvent sync call.
  scoped_ptr<base::WaitableEvent> modal_loop_pump_messages_event_;

  // Bitmap for crashed plugin
  SkBitmap* sad_plugin_;

  // True if we got an invalidate from the plugin and are waiting for a paint.
  bool invalidate_pending_;

  // Used to desynchronize windowless painting.  When WebKit paints, we bitblt
  // from our backing store of what the plugin rectangle looks like.  The
  // plugin paints into the transport store, and we copy that to our backing
  // store when we get an invalidate from it.  The background bitmap is used
  // for transparent plugins, as they need the backgroud data during painting.
  bool transparent_;
#if defined(OS_WIN)
  scoped_ptr<TransportDIB> backing_store_;
#else
  std::vector<uint8> backing_store_;
#endif
  scoped_ptr<skia::PlatformCanvas> backing_store_canvas_;
  scoped_ptr<TransportDIB> transport_store_;
  scoped_ptr<skia::PlatformCanvas> transport_store_canvas_;
  scoped_ptr<TransportDIB> background_store_;
  scoped_ptr<skia::PlatformCanvas> background_store_canvas_;
  // This lets us know which portion of the backing store has been painted into.
  gfx::Rect backing_store_painted_;

  // The url of the main frame hosting the plugin.
  GURL page_url_;

  DISALLOW_COPY_AND_ASSIGN(WebPluginDelegateProxy);
};

#endif  // CHROME_RENDERER_WEBPLUGIN_DELEGATE_PROXY_H_
