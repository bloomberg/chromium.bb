// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A BrowserPluginGuest is the browser side of a browser <--> embedder
// renderer channel. A BrowserPlugin (a WebPlugin) is on the embedder
// renderer side of browser <--> embedder renderer communication.
//
// BrowserPluginGuest lives on the UI thread of the browser process. It has a
// helper, BrowserPluginGuestHelper, which is a RenderViewHostObserver. The
// helper object intercepts messages (ViewHostMsg_*) directed at the browser
// process and redirects them to this class. Any messages about the guest render
// process that the embedder might be interested in receiving should be listened
// for here.
//
// BrowserPluginGuest is a WebContentsDelegate and WebContentsObserver for the
// guest WebContents. BrowserPluginGuest operates under the assumption that the
// guest will be accessible through only one RenderViewHost for the lifetime of
// the guest WebContents. Thus, cross-process navigation is not supported.

#ifndef CONTENT_BROWSER_BROWSER_PLUGIN_BROWSER_PLUGIN_GUEST_H_
#define CONTENT_BROWSER_BROWSER_PLUGIN_BROWSER_PLUGIN_GUEST_H_

#include <map>

#include "base/compiler_specific.h"
#include "base/id_map.h"
#include "base/shared_memory.h"
#include "base/time.h"
#include "content/port/common/input_event_ack_state.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_contents_observer.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDragStatus.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDragOperation.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebInputEvent.h"
#include "ui/gfx/rect.h"
#include "ui/surface/transport_dib.h"

struct BrowserPluginHostMsg_AutoSize_Params;
struct BrowserPluginHostMsg_CreateGuest_Params;
struct BrowserPluginHostMsg_ResizeGuest_Params;
struct ViewHostMsg_CreateWindow_Params;
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

  bool OnMessageReceivedFromEmbedder(const IPC::Message& message);

  void Initialize(const BrowserPluginHostMsg_CreateGuest_Params& params,
                  content::RenderViewHost* render_view_host);

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

  void UpdateVisibility();

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
      bool is_iframe_srcdoc,
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
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;


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

  // Exposes the protected web_contents() from WebContentsObserver.
  WebContents* GetWebContents();

  // Kill the guest process.
  void Terminate();

  // Overridden in tests.
  virtual void SetDamageBuffer(
      const BrowserPluginHostMsg_ResizeGuest_Params& params);

  gfx::Point GetScreenCoordinates(const gfx::Point& relative_position) const;

  // Helper to send messages to embedder. Overridden in test implementation
  // since we want to intercept certain messages for testing.
  virtual void SendMessageToEmbedder(IPC::Message* msg);

  // Returns the embedder's routing ID.
  int embedder_routing_id() const;
  // Returns the identifier that uniquely identifies a browser plugin guest
  // within an embedder.
  int instance_id() const { return instance_id_; }

 private:
  friend class TestBrowserPluginGuest;

  BrowserPluginGuest(int instance_id,
                     WebContentsImpl* web_contents,
                     const BrowserPluginHostMsg_CreateGuest_Params& params);

  base::SharedMemory* damage_buffer() const { return damage_buffer_.get(); }
  const gfx::Size& damage_view_size() const { return damage_view_size_; }
  float damage_buffer_scale_factor() const {
    return damage_buffer_scale_factor_;
  }
  // Returns the damage buffer corresponding to the handle in resize |params|.
  base::SharedMemory* GetDamageBufferFromEmbedder(
      const BrowserPluginHostMsg_ResizeGuest_Params& params);

  // Called when a redirect notification occurs.
  void LoadRedirect(const GURL& old_url,
                    const GURL& new_url,
                    bool is_top_level);

  bool InAutoSizeBounds(const gfx::Size& size) const;

  // Message handlers for messsages from embedder.

  // If possible, navigate the guest to |relative_index| entries away from the
  // current navigation entry.
  virtual void OnGo(int instance_id, int relative_index);
  // Handles drag events from the embedder.
  // When dragging, the drag events go to the embedder first, and if the drag
  // happens on the browser plugin, then the plugin sends a corresponding
  // drag-message to the guest. This routes the drag-message to the guest
  // renderer.
  void OnDragStatusUpdate(int instance_id,
                          WebKit::WebDragStatus drag_status,
                          const WebDropData& drop_data,
                          WebKit::WebDragOperationsMask drag_mask,
                          const gfx::Point& location);
  // Overriden in tests.
  virtual void OnHandleInputEvent(int instance_id,
                                  const gfx::Rect& guest_window_rect,
                                  const WebKit::WebInputEvent* event);
  // Reload the guest. Overriden in tests.
  virtual void OnReload(int instance_id);
  // Grab the new damage buffer from the embedder, and resize the guest's
  // web contents.
  void OnResizeGuest(int instance_id,
                     const BrowserPluginHostMsg_ResizeGuest_Params& params);
  // Overriden in tests.
  virtual void OnSetFocus(int instance_id, bool focused);
  // Updates the size state of the guest.
  void OnSetSize(
      int instance_id,
      const BrowserPluginHostMsg_AutoSize_Params& auto_size_params,
      const BrowserPluginHostMsg_ResizeGuest_Params& resize_guest_params);
  // The guest WebContents is visible if both its embedder is visible and
  // the browser plugin element is visible. If either one is not then the
  // WebContents is marked as hidden. A hidden WebContents will consume
  // fewer GPU and CPU resources.
  //
  // When every WebContents in a RenderProcessHost is hidden, it will lower
  // the priority of the process (see RenderProcessHostImpl::WidgetHidden).
  //
  // It will also send a message to the guest renderer process to cleanup
  // resources such as dropping back buffers and adjusting memory limits (if in
  // compositing mode, see CCLayerTreeHost::setVisible).
  //
  // Additionally, it will slow down Javascript execution and garbage
  // collection. See RenderThreadImpl::IdleHandler (executed when hidden) and
  // RenderThreadImpl::IdleHandlerInForegroundTab (executed when visible).
  void OnSetVisibility(int instance_id, bool visible);
  // Stop loading the guest. Overriden in tests.
  virtual void OnStop(int instance_id);
  void OnTerminateGuest(int instance_id);
  void OnUpdateRectACK(
      int instance_id,
      const BrowserPluginHostMsg_AutoSize_Params& auto_size_params,
      const BrowserPluginHostMsg_ResizeGuest_Params& resize_guest_params);


  // Message handlers for messages from guest.

  void OnCreateWindow(const ViewHostMsg_CreateWindow_Params& params,
                      int* route_id,
                      int* surface_id,
                      int64* cloned_session_storage_namespace_id);
  void OnHandleInputEventAck(
      WebKit::WebInputEvent::Type event_type,
      InputEventAckState ack_result);
  void OnHasTouchEventHandlers(bool accept);
  void OnSetCursor(const WebCursor& cursor);
  // On MacOSX popups are painted by the browser process. We handle them here
  // so that they are positioned correctly.
#if defined(OS_MACOSX)
  void OnShowPopup(const ViewHostMsg_ShowPopup_Params& params);
#endif
  void OnShowWidget(int route_id, const gfx::Rect& initial_pos);
  // Overriden in tests.
  virtual void OnTakeFocus(bool reverse);
  void OnUpdateDragCursor(WebKit::WebDragOperation operation);
  void OnUpdateRect(const ViewHostMsg_UpdateRect_Params& params);

  // Static factory instance (always NULL for non-test).
  static content::BrowserPluginHostFactory* factory_;

  NotificationRegistrar notification_registrar_;
  WebContentsImpl* embedder_web_contents_;
  // An identifier that uniquely identifies a browser plugin guest within an
  // embedder.
  int instance_id_;
  scoped_ptr<base::SharedMemory> damage_buffer_;
  // An identifier that uniquely identifies a damage buffer.
  uint32 damage_buffer_sequence_id_;
  size_t damage_buffer_size_;
  gfx::Size damage_view_size_;
  float damage_buffer_scale_factor_;
  gfx::Rect guest_window_rect_;
  gfx::Rect guest_screen_rect_;
  base::TimeDelta guest_hang_timeout_;
  bool focused_;
  bool visible_;
  bool auto_size_enabled_;
  gfx::Size max_auto_size_;
  gfx::Size min_auto_size_;

  DISALLOW_COPY_AND_ASSIGN(BrowserPluginGuest);
};

}  // namespace content

#endif  // CONTENT_BROWSER_BROWSER_PLUGIN_BROWSER_PLUGIN_GUEST_H_
