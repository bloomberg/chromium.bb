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
struct BrowserPluginHostMsg_Attach_Params;
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
class CONTENT_EXPORT BrowserPluginGuest
    : public NotificationObserver,
      public WebContentsDelegate,
      public WebContentsObserver,
      public base::SupportsWeakPtr<BrowserPluginGuest> {
 public:
  typedef base::Callback<void(bool)> GeolocationCallback;
  virtual ~BrowserPluginGuest();

  static BrowserPluginGuest* Create(
      int instance_id,
      WebContentsImpl* web_contents);

  static BrowserPluginGuest* CreateWithOpener(
      int instance_id,
      WebContentsImpl* web_contents,
      BrowserPluginGuest* opener,
      bool has_render_view);

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
                  const BrowserPluginHostMsg_Attach_Params& params);

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

  virtual bool AddMessageToConsole(WebContents* source,
                                   int32 level,
                                   const string16& message,
                                   int32 line_no,
                                   const string16& source_id) OVERRIDE;
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
  virtual void CloseContents(WebContents* source) OVERRIDE;
  virtual bool HandleContextMenu(const ContextMenuParams& params) OVERRIDE;
  virtual void HandleKeyboardEvent(
      WebContents* source,
      const NativeWebKeyboardEvent& event) OVERRIDE;
  virtual WebContents* OpenURLFromTab(WebContents* source,
                                      const OpenURLParams& params) OVERRIDE;
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
              BrowserPluginHostMsg_Attach_Params params);

  // Requests geolocation permission through Embedder JavaScript API.
  void AskEmbedderForGeolocationPermission(int bridge_id,
                                           const GURL& requesting_frame,
                                           const GeolocationCallback& callback);
  // Cancels pending geolocation request.
  void CancelGeolocationRequest(int bridge_id);

  // Allow the embedder to call this for unhandled messages when
  // BrowserPluginGuest is already destroyed.
  static void AcknowledgeBufferPresent(int route_id,
                                       int gpu_host_id,
                                       const std::string& mailbox_name,
                                       uint32 sync_point);

  // Returns whether BrowserPluginGuest is interested in receiving the given
  // |message|.
  static bool ShouldForwardToBrowserPluginGuest(const IPC::Message& message);

  void DragSourceEndedAt(int client_x, int client_y, int screen_x,
      int screen_y, WebKit::WebDragOperation operation);

  void DragSourceMovedTo(int client_x, int client_y,
                         int screen_x, int screen_y);

  // Called when the drag started by this guest ends at an OS-level.
  void EndSystemDrag();

 private:
  class EmbedderRenderViewHostObserver;
  friend class TestBrowserPluginGuest;

  class DownloadRequest;
  class GeolocationRequest;
  // MediaRequest because of naming conflicts with MediaStreamRequest.
  class MediaRequest;
  class NewWindowRequest;
  class PermissionRequest;

  BrowserPluginGuest(int instance_id,
                     WebContentsImpl* web_contents,
                     BrowserPluginGuest* opener,
                     bool has_render_view);

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
  // Instructs the guest to execute an edit command decoded in the embedder.
  void OnExecuteEditCommand(int instance_id,
                            const std::string& command);
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

  void OnDragStopped();
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
  void OnUpdateFrameName(int frame_id,
                         bool is_top_level,
                         const std::string& name);
  void OnUpdateRect(const ViewHostMsg_UpdateRect_Params& params);

  // Requests download permission through embedder JavaScript API after
  // retrieving url information from IO thread.
  void DidRetrieveDownloadURLFromRequestId(const std::string& request_method,
                                           int permission_request_id,
                                           const std::string& url);

  // Embedder sets permission to allow or deny geolocation request.
  void SetGeolocationPermission(
      GeolocationCallback callback, int bridge_id, bool allowed);

  // Weak pointer used to ask GeolocationPermissionContext about geolocation
  // permission.
  base::WeakPtrFactory<BrowserPluginGuest> weak_ptr_factory_;

  // Static factory instance (always NULL for non-test).
  static BrowserPluginHostFactory* factory_;

  NotificationRegistrar notification_registrar_;
  scoped_ptr<EmbedderRenderViewHostObserver> embedder_rvh_observer_;
  WebContentsImpl* embedder_web_contents_;

  std::map<int, int> bridge_id_to_request_id_map_;

  // An identifier that uniquely identifies a browser plugin guest within an
  // embedder.
  int instance_id_;
  scoped_ptr<base::SharedMemory> damage_buffer_;
  // An identifier that uniquely identifies a damage buffer.
  uint32 damage_buffer_sequence_id_;
  size_t damage_buffer_size_;
  gfx::Size damage_view_size_;
  float damage_buffer_scale_factor_;
  float guest_device_scale_factor_;
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

  // Tracks the target URL of the new window and whether or not it has changed
  // since the WebContents has been created and before the new window has been
  // attached to a BrowserPlugin. Once the first navigation commits, we no
  // longer track this URL.
  struct TargetURL {
    bool changed;
    GURL url;
    explicit TargetURL(const GURL& url) :
        changed(false),
        url(url) {}
  };
  typedef std::map<BrowserPluginGuest*, TargetURL> PendingWindowMap;
  PendingWindowMap pending_new_windows_;
  base::WeakPtr<BrowserPluginGuest> opener_;
  // A counter to generate a unique request id for a permission request.
  // We only need the ids to be unique for a given BrowserPluginGuest.
  int next_permission_request_id_;

  // A map to store relevant info for a request keyed by the request's id.
  typedef std::map<int, PermissionRequest*> RequestMap;
  RequestMap permission_request_map_;

  // Indicates that this BrowserPluginGuest has associated renderer-side state.
  // This is used to determine whether or not to create a new RenderView when
  // this guest is attached.
  bool has_render_view_;

  DISALLOW_COPY_AND_ASSIGN(BrowserPluginGuest);
};

}  // namespace content

#endif  // CONTENT_BROWSER_BROWSER_PLUGIN_BROWSER_PLUGIN_GUEST_H_
