// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A BrowserPluginGuest represents the browser side of browser <--> renderer
// communication. A BrowserPlugin (a WebPlugin) is on the renderer side of
// browser <--> guest renderer communication. The 'guest' renderer is a
// <browser> tag.
//
// BrowserPluginGuest lives on the UI thread of the browser process. It has a
// helper, BrowserPluginGuestHelper, which is a RenderViewHostObserver. The
// helper object receives messages (ViewHostMsg_*) directed at the browser
// plugin and redirects them to this class. Any messages the embedder might be
// interested in knowing or modifying about the guest should be listened for
// here.
//
// Since BrowserPlugin is a WebPlugin, we need to provide overridden behaviors
// for messages like handleInputEvent, updateGeometry. Such messages get
// routed into BrowserPluginGuest via its embedder (BrowserPluginEmbedder).
// These are BrowserPluginHost_* messages sent from the BrowserPlugin.
//
// BrowserPluginGuest knows about its embedder process. Communication to
// renderer happens through the embedder process.
//
// A BrowserPluginGuest is also associated directly with the WebContents related
// to the BrowserPlugin. BrowserPluginGuest is a WebContentsDelegate and
// WebContentsObserver for the WebContents.

#ifndef CONTENT_BROWSER_BROWSER_PLUGIN_BROWSER_PLUGIN_GUEST_H_
#define CONTENT_BROWSER_BROWSER_PLUGIN_BROWSER_PLUGIN_GUEST_H_

#include <map>

#include "base/compiler_specific.h"
#include "base/id_map.h"
#include "base/time.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_contents_observer.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDragStatus.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDragOperation.h"
#include "ui/gfx/rect.h"
#include "ui/surface/transport_dib.h"

struct BrowserPluginHostMsg_AutoSize_Params;
struct BrowserPluginHostMsg_CreateGuest_Params;
struct BrowserPluginHostMsg_ResizeGuest_Params;
#if defined(OS_MACOSX)
struct ViewHostMsg_ShowPopup_Params;
#endif
struct ViewHostMsg_UpdateRect_Params;
class WebCursor;
struct WebDropData;

namespace WebKit {
class WebInputEvent;
}

namespace content {

class BrowserPluginHostFactory;
class BrowserPluginEmbedder;
class RenderProcessHost;

// A browser plugin guest provides functionality for WebContents to operate in
// the guest role and implements guest specific overrides for ViewHostMsg_*
// messages.
//
// BrowserPluginEmbedder is responsible for creating and destroying a guest.
class CONTENT_EXPORT BrowserPluginGuest : public NotificationObserver,
                                          public WebContentsDelegate,
                                          public WebContentsObserver {
 public:
  virtual ~BrowserPluginGuest();

  static BrowserPluginGuest* Create(
      int instance_id,
      WebContentsImpl* web_contents,
      const BrowserPluginHostMsg_CreateGuest_Params& params);

  // Overrides factory for testing. Default (NULL) value indicates regular
  // (non-test) environment.
  static void set_factory_for_testing(BrowserPluginHostFactory* factory) {
    content::BrowserPluginGuest::factory_ = factory;
  }

  void InstallHelper(content::RenderViewHost* render_view_host);

  void set_guest_hang_timeout_for_testing(const base::TimeDelta& timeout) {
    guest_hang_timeout_ = timeout;
  }

  void set_embedder_web_contents(WebContentsImpl* web_contents) {
    embedder_web_contents_ = web_contents;
  }
  WebContentsImpl* embedder_web_contents() const {
    return embedder_web_contents_;
  }

  bool focused() const { return focused_; }
  bool visible() const { return visible_; }

  // NotificationObserver implementation.
  virtual void Observe(int type,
                       const NotificationSource& source,
                       const NotificationDetails& details) OVERRIDE;

  // WebContentsObserver implementation.
  virtual void DidStartProvisionalLoadForFrame(
      int64 frame_id,
      int64 parent_frame_id,
      bool is_main_frame,
      const GURL& validated_url,
      bool is_error_page,
      RenderViewHost* render_view_host) OVERRIDE;
  virtual void DidFailProvisionalLoad(
      int64 frame_id,
      bool is_main_frame,
      const GURL& validated_url,
      int error_code,
      const string16& error_description,
      RenderViewHost* render_view_host) OVERRIDE;
  virtual void DidCommitProvisionalLoadForFrame(
      int64 frame_id,
      bool is_main_frame,
      const GURL& url,
      PageTransition transition_type,
      RenderViewHost* render_view_host) OVERRIDE;
  virtual void DidStopLoading(RenderViewHost* render_view_host) OVERRIDE;

  virtual void RenderViewReady() OVERRIDE;
  virtual void RenderViewGone(base::TerminationStatus status) OVERRIDE;

  // WebContentsDelegate implementation.
  virtual bool CanDownload(RenderViewHost* render_view_host,
                           int request_id,
                           const std::string& request_method) OVERRIDE;
  virtual bool HandleContextMenu(const ContextMenuParams& params) OVERRIDE;
  virtual void RendererUnresponsive(WebContents* source) OVERRIDE;
  virtual void RendererResponsive(WebContents* source) OVERRIDE;
  virtual void RunFileChooser(WebContents* web_contents,
                              const FileChooserParams& params) OVERRIDE;
  virtual bool ShouldFocusPageAfterCrash() OVERRIDE;

  void UpdateRect(RenderViewHost* render_view_host,
                  const ViewHostMsg_UpdateRect_Params& params);
  void UpdateRectACK(
      int message_id,
      const BrowserPluginHostMsg_AutoSize_Params& auto_size_params,
      const BrowserPluginHostMsg_ResizeGuest_Params& resize_guest_params);
  // Overrides default ShowWidget message so we show them on the correct
  // coordinates.
  void ShowWidget(RenderViewHost* render_view_host,
                  int route_id,
                  const gfx::Rect& initial_pos);
  // On MacOSX popups are painted by the browser process. We handle them here
  // so that they are positioned correctly.
#if defined(OS_MACOSX)
  void ShowPopup(RenderViewHost* render_view_host,
                 const ViewHostMsg_ShowPopup_Params& params);
#endif
  void SetCursor(const WebCursor& cursor);
  // Handles input event acks so they are sent to browser plugin host (via
  // embedder) instead of default view/widget host.
  void HandleInputEventAck(RenderViewHost* render_view_host, bool handled);

  // The guest needs to notify the plugin in the embedder to start (or stop)
  // accepting touch events.
  void SetIsAcceptingTouchEvents(bool accept);

  // The guest WebContents is visible if both its embedder is visible and
  // the browser plugin element is visible. If either one is not then the
  // WebContents is marked as hidden. A hidden WebContents will consume
  // fewer GPU and CPU resources.
  //
  // When the every WebContents in a RenderProcessHost is hidden, it will lower
  // the priority of the process (see RenderProcessHostImpl::WidgetHidden).
  //
  // It will also send a message to the guest renderer process to cleanup
  // resources such as dropping back buffers and adjusting memory limits (if in
  // compositing mode, see CCLayerTreeHost::setVisible).
  //
  // Additionally it will slow down Javascript execution and garbage collection.
  // See RenderThreadImpl::IdleHandler (executed when hidden) and
  // RenderThreadImpl::IdleHandlerInForegroundTab (executed when visible).
  void SetVisibility(bool embedder_visible, bool visible);

  // Handles drag events from the embedder.
  // When dragging, the drag events go to the embedder first, and if the drag
  // happens on the browser plugin, then the plugin sends a corresponding
  // drag-message to the guest. This routes the drag-message to the guest
  // renderer.
  void DragStatusUpdate(WebKit::WebDragStatus drag_status,
                        const WebDropData& drop_data,
                        WebKit::WebDragOperationsMask drag_mask,
                        const gfx::Point& location);

  // Updates the size state of the guest.
  void SetSize(
      const BrowserPluginHostMsg_AutoSize_Params& auto_size_params,
      const BrowserPluginHostMsg_ResizeGuest_Params& resize_guest_params);

  // Updates the cursor during dragging.
  // During dragging, if the guest notifies to update the cursor for a drag,
  // then it is necessary to route the cursor update to the embedder correctly
  // so that the cursor updates properly.
  void UpdateDragCursor(WebKit::WebDragOperation operation);

  // Exposes the protected web_contents() from WebContentsObserver.
  WebContents* GetWebContents();

  // Kill the guest process.
  void Terminate();

  // Grab the new damage buffer from the embedder, and resize the guest's
  // web contents.
  void Resize(RenderViewHost* embedder_rvh,
              const BrowserPluginHostMsg_ResizeGuest_Params& params);

  // Overridden in tests.
  // Handles input event routed through the embedder (which is initiated in the
  // browser plugin (renderer side of the embedder)).
  virtual void HandleInputEvent(RenderViewHost* render_view_host,
                                const gfx::Rect& guest_window_rect,
                                const gfx::Rect& guest_screen_rect,
                                const WebKit::WebInputEvent& event);
  virtual bool ViewTakeFocus(bool reverse);
  // If possible, navigate the guest to |relative_index| entries away from the
  // current navigation entry.
  virtual void Go(int relative_index);
  // Overridden in tests.
  virtual void SetFocus(bool focused);
  // Reload the guest.
  virtual void Reload();
  // Stop loading the guest.
  virtual void Stop();
  // Overridden in tests.
  virtual void SetDamageBuffer(TransportDIB* damage_buffer,
#if defined(OS_WIN)
                               int damage_buffer_size,
                               TransportDIB::Handle remote_handle,
#endif
                               const gfx::Size& damage_view_size,
                               float scale_factor);
  // Overridden in tests.
  virtual void SetCompositingBufferData(int gpu_process_id,
                                        uint32 client_id,
                                        uint32 context_id,
                                        uint32 texture_id_0,
                                        uint32 texture_id_1,
                                        uint32 sync_point);

  gfx::Point GetScreenCoordinates(const gfx::Point& relative_position) const;

 private:
  friend class TestBrowserPluginGuest;

  BrowserPluginGuest(int instance_id,
                     WebContentsImpl* web_contents,
                     const BrowserPluginHostMsg_CreateGuest_Params& params);

  // Returns the identifier that uniquely identifies a browser plugin guest
  // within an embedder.
  int instance_id() const { return instance_id_; }
  TransportDIB* damage_buffer() const { return damage_buffer_.get(); }
  const gfx::Size& damage_view_size() const { return damage_view_size_; }
  float damage_buffer_scale_factor() const {
    return damage_buffer_scale_factor_;
  }
  // Returns the transport DIB associated with the dib in resize |params|.
  TransportDIB* GetDamageBufferFromEmbedder(
      RenderViewHost* embedder_rvh,
      const BrowserPluginHostMsg_ResizeGuest_Params& params);

  // Returns the embedder's routing ID.
  int embedder_routing_id() const;

  // Helper to send messages to embedder. Overridden in test implementation
  // since we want to intercept certain messages for testing.
  virtual void SendMessageToEmbedder(IPC::Message* msg);

  // Called when a redirect notification occurs.
  void LoadRedirect(const GURL& old_url,
                    const GURL& new_url,
                    bool is_top_level);

  bool InAutoSizeBounds(const gfx::Size& size) const;
  // Static factory instance (always NULL for non-test).
  static content::BrowserPluginHostFactory* factory_;

  NotificationRegistrar notification_registrar_;
  WebContentsImpl* embedder_web_contents_;
  // An identifier that uniquely identifies a browser plugin guest within an
  // embedder.
  int instance_id_;
  scoped_ptr<TransportDIB> damage_buffer_;
#if defined(OS_WIN)
  size_t damage_buffer_size_;
  TransportDIB::Handle remote_damage_buffer_handle_;
#endif
  gfx::Size damage_view_size_;
  float damage_buffer_scale_factor_;
  gfx::Rect guest_window_rect_;
  gfx::Rect guest_screen_rect_;
  IDMap<RenderViewHost> pending_updates_;
  int pending_update_counter_;
  base::TimeDelta guest_hang_timeout_;
  bool focused_;
  bool visible_;
  bool auto_size_enabled_;
  gfx::Size max_auto_size_;
  gfx::Size min_auto_size_;

  // Hardware Accelerated Surface Params
  gfx::GLSurfaceHandle surface_handle_;

  DISALLOW_COPY_AND_ASSIGN(BrowserPluginGuest);
};

}  // namespace content

#endif  // CONTENT_BROWSER_BROWSER_PLUGIN_BROWSER_PLUGIN_GUEST_H_
