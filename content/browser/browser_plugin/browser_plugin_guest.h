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
#include "base/memory/weak_ptr.h"
#include "base/shared_memory.h"
#include "base/time.h"
#include "content/common/browser_plugin/browser_plugin_message_enums.h"
#include "content/port/common/input_event_ack_state.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/render_view_host_observer.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_contents_observer.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDragOperation.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDragStatus.h"
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

namespace cc {
class CompositorFrameAck;
}

namespace WebKit {
class WebInputEvent;
}

namespace content {

class BrowserPluginHostFactory;
class BrowserPluginEmbedder;
class BrowserPluginGuestManager;
class RenderProcessHost;
class RenderWidgetHostView;
struct MediaStreamRequest;

// A browser plugin guest provides functionality for WebContents to operate in
// the guest role and implements guest-specific overrides for ViewHostMsg_*
// messages.
//
// When a guest is initially created, it is in an unattached state. That is,
// it is not visible anywhere and has no embedder WebContents assigned.
// A BrowserPluginGuest is said to be "attached" if it has an embedder.
// A BrowserPluginGuest can also create a new unattached guest via
// CreateNewWindow. The newly created guest will live in the same partition,
// which means it can share storage and can script this guest.
class CONTENT_EXPORT BrowserPluginGuest : public NotificationObserver,
                                          public WebContentsDelegate,
                                          public WebContentsObserver {
 public:
  typedef base::Callback<void(bool)> GeolocationCallback;
  virtual ~BrowserPluginGuest();

  static BrowserPluginGuest* Create(
      int instance_id,
      WebContentsImpl* web_contents);

  // Destroys the guest WebContents and all its associated state, including
  // this BrowserPluginGuest, and its new unattached windows.
  void Destroy();

  // Returns the identifier that uniquely identifies a browser plugin guest
  // within an embedder.
  int instance_id() const { return instance_id_; }

  // Overrides factory for testing. Default (NULL) value indicates regular
  // (non-test) environment.
  static void set_factory_for_testing(BrowserPluginHostFactory* factory) {
    BrowserPluginGuest::factory_ = factory;
  }

  bool OnMessageReceivedFromEmbedder(const IPC::Message& message);

  void Initialize(WebContentsImpl* embedder_web_contents,
                  const BrowserPluginHostMsg_CreateGuest_Params& params);

  void set_guest_hang_timeout_for_testing(const base::TimeDelta& timeout) {
    guest_hang_timeout_ = timeout;
  }

  WebContentsImpl* embedder_web_contents() const {
    return embedder_web_contents_;
  }

  RenderWidgetHostView* GetEmbedderRenderWidgetHostView();

  bool focused() const { return focused_; }
  bool visible() const { return guest_visible_; }
  void clear_damage_buffer() { damage_buffer_.reset(); }

  BrowserPluginGuest* opener() const { return opener_; }

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

  // If a new window is created with target="_blank" and rel="noreferrer", then
  // this method is called, indicating that the new WebContents is ready to be
  // attached.
  virtual void AddNewContents(WebContents* source,
                              WebContents* new_contents,
                              WindowOpenDisposition disposition,
                              const gfx::Rect& initial_pos,
                              bool user_gesture,
                              bool* was_blocked) OVERRIDE;
  virtual void CanDownload(RenderViewHost* render_view_host,
                           int request_id,
                           const std::string& request_method,
                           const base::Callback<void(bool)>& callback) OVERRIDE;
  virtual bool HandleContextMenu(const ContextMenuParams& params) OVERRIDE;
  virtual void WebContentsCreated(WebContents* source_contents,
                                  int64 source_frame_id,
                                  const string16& frame_name,
                                  const GURL& target_url,
                                  WebContents* new_contents) OVERRIDE;
  virtual void RendererUnresponsive(WebContents* source) OVERRIDE;
  virtual void RendererResponsive(WebContents* source) OVERRIDE;
  virtual void RunFileChooser(WebContents* web_contents,
                              const FileChooserParams& params) OVERRIDE;
  virtual bool ShouldFocusPageAfterCrash() OVERRIDE;
  virtual void RequestMediaAccessPermission(
      WebContents* web_contents,
      const MediaStreamRequest& request,
      const MediaResponseCallback& callback) OVERRIDE;

  // Exposes the protected web_contents() from WebContentsObserver.
  WebContentsImpl* GetWebContents();

  // Kill the guest process.
  void Terminate();

  // Overridden in tests.
  virtual void SetDamageBuffer(
      const BrowserPluginHostMsg_ResizeGuest_Params& params);

  gfx::Point GetScreenCoordinates(const gfx::Point& relative_position) const;

  // Helper to send messages to embedder. This methods fills the message with
  // the correct routing id.
  // Overridden in test implementation since we want to intercept certain
  // messages for testing.
  virtual void SendMessageToEmbedder(IPC::Message* msg);

  // Returns whether the guest is attached to an embedder.
  bool attached() const { return !!embedder_web_contents_; }

  // Attaches this BrowserPluginGuest to the provided |embedder_web_contents|
  // and initializes the guest with the provided |params|. Attaching a guest
  // to an embedder implies that this guest's lifetime is no longer managed
  // by its opener, and it can begin loading resources.
  void Attach(WebContentsImpl* embedder_web_contents,
              BrowserPluginHostMsg_CreateGuest_Params params);

  // Requests geolocation permission through Embedder JavaScript API.
  void AskEmbedderForGeolocationPermission(int bridge_id,
                                           const GURL& requesting_frame,
                                           const GeolocationCallback& callback);
  // Cancels pending geolocation request.
  void CancelGeolocationRequest(int bridge_id);
  // Embedder sets permission to allow or deny geolocation request.
  void SetGeolocationPermission(int request_id, bool allowed);


  // Allow the embedder to call this for unhandled messages when
  // BrowserPluginGuest is already destroyed.
  static void AcknowledgeBufferPresent(int route_id,
                                       int gpu_host_id,
                                       const std::string& mailbox_name,
                                       uint32 sync_point);

  // Returns whether BrowserPluginGuest is interested in receiving the given
  // |message|.
  static bool ShouldForwardToBrowserPluginGuest(const IPC::Message& message);

 private:
  typedef std::pair<MediaStreamRequest, MediaResponseCallback>
      MediaStreamRequestAndCallbackPair;
  typedef std::map<int, MediaStreamRequestAndCallbackPair>
      MediaStreamRequestsMap;

  class EmbedderRenderViewHostObserver;
  friend class TestBrowserPluginGuest;

  BrowserPluginGuest(int instance_id,
                     WebContentsImpl* web_contents);

  // Destroy unattached new windows that have been opened by this
  // BrowserPluginGuest.
  void DestroyUnattachedWindows();

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

  void RequestNewWindowPermission(WebContentsImpl* new_contents,
                                  WindowOpenDisposition disposition,
                                  const gfx::Rect& initial_bounds,
                                  bool user_gesture);

  // Message handlers for messages from embedder.

  void OnCompositorFrameACK(int instance_id,
                            int route_id,
                            int renderer_host_id,
                            const cc::CompositorFrameAck& ack);

  // Allows or denies a permission request access, after the embedder has had a
  // chance to decide.
  void OnRespondPermission(int instance_id,
                           BrowserPluginPermissionType permission_type,
                           int request_id,
                           bool should_allow);
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
  // If possible, navigate the guest to |relative_index| entries away from the
  // current navigation entry.
  virtual void OnGo(int instance_id, int relative_index);
  // Overriden in tests.
  virtual void OnHandleInputEvent(int instance_id,
                                  const gfx::Rect& guest_window_rect,
                                  const WebKit::WebInputEvent* event);
  void OnLockMouse(bool user_gesture,
                   bool last_unlocked_by_target,
                   bool privileged);
  void OnLockMouseAck(int instance_id, bool succeeded);
  void OnNavigateGuest(int instance_id, const std::string& src);
  void OnPluginDestroyed(int instance_id);
  // Reload the guest. Overriden in tests.
  virtual void OnReload(int instance_id);
  // Grab the new damage buffer from the embedder, and resize the guest's
  // web contents.
  void OnResizeGuest(int instance_id,
                     const BrowserPluginHostMsg_ResizeGuest_Params& params);
  // Overriden in tests.
  virtual void OnSetFocus(int instance_id, bool focused);
  // Sets the name of the guest so that other guests in the same partition can
  // access it.
  void OnSetName(int instance_id, const std::string& name);
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
  // Message from embedder acknowledging last HW buffer.
  void OnSwapBuffersACK(int instance_id,
                        int route_id,
                        int gpu_host_id,
                        const std::string& mailbox_name,
                        uint32 sync_point);

  void OnTerminateGuest(int instance_id);
  void OnUnlockMouse();
  void OnUnlockMouseAck(int instance_id);
  void OnUpdateRectACK(
      int instance_id,
      const BrowserPluginHostMsg_AutoSize_Params& auto_size_params,
      const BrowserPluginHostMsg_ResizeGuest_Params& resize_guest_params);


  // Message handlers for messages from guest.

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
  void OnShowView(int route_id,
                  WindowOpenDisposition disposition,
                  const gfx::Rect& initial_bounds,
                  bool user_gesture);
  void OnShowWidget(int route_id, const gfx::Rect& initial_pos);
  // Overriden in tests.
  virtual void OnTakeFocus(bool reverse);
  void OnUpdateDragCursor(WebKit::WebDragOperation operation);
  void OnUpdateFrameName(int frame_id,
                         bool is_top_level,
                         const std::string& name);
  void OnUpdateRect(const ViewHostMsg_UpdateRect_Params& params);

  // Helpers for |OnRespondPermission|.
  void OnRespondPermissionDownload(int request_id, bool should_allow);
  void OnRespondPermissionGeolocation(int request_id, bool should_allow);
  void OnRespondPermissionMedia(int request_id, bool should_allow);
  void OnRespondPermissionNewWindow(int request_id, bool should_allow);

  // Requests download permission through embedder JavaScript API after
  // retrieving url information from IO thread.
  void DidRetrieveDownloadURLFromRequestId(const std::string& request_method,
                                           int permission_request_id,
                                           const std::string& url);

  // Weak pointer used to ask GeolocationPermissionContext about geolocation
  // permission.
  base::WeakPtrFactory<BrowserPluginGuest> weak_ptr_factory_;

  // Static factory instance (always NULL for non-test).
  static BrowserPluginHostFactory* factory_;

  NotificationRegistrar notification_registrar_;
  scoped_ptr<EmbedderRenderViewHostObserver> embedder_rvh_observer_;
  WebContentsImpl* embedder_web_contents_;
  typedef std::map<int, GeolocationCallback> GeolocationRequestsMap;
  GeolocationRequestsMap geolocation_request_callback_map_;
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
  bool mouse_locked_;
  bool pending_lock_request_;
  bool guest_visible_;
  bool embedder_visible_;
  std::string name_;
  bool auto_size_enabled_;
  gfx::Size max_auto_size_;
  gfx::Size min_auto_size_;

  typedef std::map<BrowserPluginGuest*, std::string> PendingWindowMap;
  PendingWindowMap pending_new_windows_;
  BrowserPluginGuest* opener_;
  // A counter to generate a unique request id for a permission request.
  // We only need the ids to be unique for a given BrowserPluginGuest.
  int next_permission_request_id_;
  // A map to store WebContents's media request object and callback.
  // We need to store these because we need a roundtrip to the embedder to know
  // if we allow or disallow the request. The key of the map is unique only for
  // a given BrowserPluginGuest.
  MediaStreamRequestsMap media_requests_map_;
  // A map from request ID to instance ID for use by the New Window API.
  typedef std::map<int, int> NewWindowRequestMap;
  NewWindowRequestMap new_window_request_map_;

  typedef std::map<int, base::Callback<void(bool)> > DownloadRequestMap;
  DownloadRequestMap download_request_callback_map_;

  DISALLOW_COPY_AND_ASSIGN(BrowserPluginGuest);
};

}  // namespace content

#endif  // CONTENT_BROWSER_BROWSER_PLUGIN_BROWSER_PLUGIN_GUEST_H_
