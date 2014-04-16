// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A BrowserPluginGuest is the browser side of a browser <--> embedder
// renderer channel. A BrowserPlugin (a WebPlugin) is on the embedder
// renderer side of browser <--> embedder renderer communication.
//
// BrowserPluginGuest lives on the UI thread of the browser process. Any
// messages about the guest render process that the embedder might be interested
// in receiving should be listened for here.
//
// BrowserPluginGuest is a WebContentsDelegate and WebContentsObserver for the
// guest WebContents. BrowserPluginGuest operates under the assumption that the
// guest will be accessible through only one RenderViewHost for the lifetime of
// the guest WebContents. Thus, cross-process navigation is not supported.

#ifndef CONTENT_BROWSER_BROWSER_PLUGIN_BROWSER_PLUGIN_GUEST_H_
#define CONTENT_BROWSER_BROWSER_PLUGIN_BROWSER_PLUGIN_GUEST_H_

#include <map>
#include <queue>

#include "base/compiler_specific.h"
#include "base/id_map.h"
#include "base/memory/shared_memory.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "content/common/edit_command.h"
#include "content/port/common/input_event_ack_state.h"
#include "content/public/browser/browser_plugin_guest_delegate.h"
#include "content/public/browser/javascript_dialog_manager.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/browser_plugin_permission_type.h"
#include "third_party/WebKit/public/web/WebCompositionUnderline.h"
#include "third_party/WebKit/public/web/WebDragOperation.h"
#include "third_party/WebKit/public/web/WebDragStatus.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"
#include "ui/base/ime/text_input_mode.h"
#include "ui/base/ime/text_input_type.h"
#include "ui/gfx/rect.h"
#include "ui/surface/transport_dib.h"

struct BrowserPluginHostMsg_AutoSize_Params;
struct BrowserPluginHostMsg_Attach_Params;
struct BrowserPluginHostMsg_ResizeGuest_Params;
struct FrameHostMsg_BuffersSwappedACK_Params;
struct FrameHostMsg_CompositorFrameSwappedACK_Params;
struct FrameHostMsg_ReclaimCompositorResources_Params;
struct ViewHostMsg_CreateWindow_Params;
#if defined(OS_MACOSX)
struct ViewHostMsg_ShowPopup_Params;
#endif
struct ViewHostMsg_UpdateRect_Params;

namespace cc {
class CompositorFrameAck;
}

namespace blink {
class WebInputEvent;
}

namespace gfx {
class Range;
}

namespace gpu {
struct Mailbox;
}

namespace content {

class BrowserPluginHostFactory;
class BrowserPluginEmbedder;
class BrowserPluginGuestManager;
class RenderProcessHost;
class RenderWidgetHostView;
class SiteInstance;
class WebCursor;
struct DropData;
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
    : public JavaScriptDialogManager,
      public WebContentsDelegate,
      public WebContentsObserver {
 public:
  virtual ~BrowserPluginGuest();

  // The WebContents passed into the factory method here has not been
  // initialized yet and so it does not yet hold a SiteInstance.
  // BrowserPluginGuest must be constructed and installed into a WebContents
  // prior to its initialization because WebContents needs to determine what
  // type of WebContentsView to construct on initialization. The content
  // embedder needs to be aware of |guest_site_instance| on the guest's
  // construction and so we pass it in here.
  static BrowserPluginGuest* Create(
      int instance_id,
      SiteInstance* guest_site_instance,
      WebContentsImpl* web_contents,
      scoped_ptr<base::DictionaryValue> extra_params);

  static BrowserPluginGuest* CreateWithOpener(
      int instance_id,
      bool has_render_view,
      WebContentsImpl* web_contents,
      BrowserPluginGuest* opener);

  // Returns a WeakPtr to this BrowserPluginGuest.
  base::WeakPtr<BrowserPluginGuest> AsWeakPtr();

  // Called when the embedder WebContents is destroyed to give the
  // BrowserPluginGuest an opportunity to clean up after itself.
  void EmbedderDestroyed();

  // Called when the embedder WebContents changes visibility.
  void EmbedderVisibilityChanged(bool visible);

  // Destroys the guest WebContents and all its associated state, including
  // this BrowserPluginGuest, and its new unattached windows.
  void Destroy();

  // Returns the identifier that uniquely identifies a browser plugin guest
  // within an embedder.
  int instance_id() const { return instance_id_; }

  bool OnMessageReceivedFromEmbedder(const IPC::Message& message);

  void Initialize(const BrowserPluginHostMsg_Attach_Params& params,
                  WebContentsImpl* embedder_web_contents);

  WebContentsImpl* embedder_web_contents() const {
    return embedder_web_contents_;
  }

  // Returns the embedder's RenderWidgetHostView if it is available.
  // Returns NULL otherwise.
  RenderWidgetHostView* GetEmbedderRenderWidgetHostView();

  bool focused() const { return focused_; }
  bool visible() const { return guest_visible_; }
  void clear_damage_buffer() { damage_buffer_.reset(); }
  bool is_in_destruction() { return is_in_destruction_; }

  BrowserPluginGuest* opener() const { return opener_.get(); }

  // Returns whether the mouse pointer was unlocked.
  bool UnlockMouseIfNecessary(const NativeWebKeyboardEvent& event);

  void UpdateVisibility();

  void CopyFromCompositingSurface(
      gfx::Rect src_subrect,
      gfx::Size dst_size,
      const base::Callback<void(bool, const SkBitmap&)>& callback);

  // WebContentsObserver implementation.
  virtual void DidCommitProvisionalLoadForFrame(
      int64 frame_id,
      const base::string16& frame_unique_name,
      bool is_main_frame,
      const GURL& url,
      PageTransition transition_type,
      RenderViewHost* render_view_host) OVERRIDE;
  virtual void DidStopLoading(RenderViewHost* render_view_host) OVERRIDE;

  virtual void RenderViewReady() OVERRIDE;
  virtual void RenderProcessGone(base::TerminationStatus status) OVERRIDE;
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

  // WebContentsDelegate implementation.
  virtual bool AddMessageToConsole(WebContents* source,
                                   int32 level,
                                   const base::string16& message,
                                   int32 line_no,
                                   const base::string16& source_id) OVERRIDE;
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
  virtual void LoadProgressChanged(WebContents* source,
                                   double progress) OVERRIDE;
  virtual void CloseContents(WebContents* source) OVERRIDE;
  virtual JavaScriptDialogManager* GetJavaScriptDialogManager() OVERRIDE;
  virtual ColorChooser* OpenColorChooser(
      WebContents* web_contents,
      SkColor color,
      const std::vector<ColorSuggestion>& suggestions) OVERRIDE;
  virtual bool HandleContextMenu(const ContextMenuParams& params) OVERRIDE;
  virtual void HandleKeyboardEvent(
      WebContents* source,
      const NativeWebKeyboardEvent& event) OVERRIDE;
  virtual void FindReply(WebContents* contents,
                         int request_id,
                         int number_of_matches,
                         const gfx::Rect& selection_rect,
                         int active_match_ordinal,
                         bool final_update) OVERRIDE;
  virtual WebContents* OpenURLFromTab(WebContents* source,
                                      const OpenURLParams& params) OVERRIDE;
  virtual void WebContentsCreated(WebContents* source_contents,
                                  int opener_render_frame_id,
                                  const base::string16& frame_name,
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
  virtual bool PreHandleGestureEvent(
      content::WebContents* source,
      const blink::WebGestureEvent& event) OVERRIDE;

  // JavaScriptDialogManager implementation.
  virtual void RunJavaScriptDialog(
      WebContents* web_contents,
      const GURL& origin_url,
      const std::string& accept_lang,
      JavaScriptMessageType javascript_message_type,
      const base::string16& message_text,
      const base::string16& default_prompt_text,
      const DialogClosedCallback& callback,
      bool* did_suppress_message) OVERRIDE;
  virtual void RunBeforeUnloadDialog(
      WebContents* web_contents,
      const base::string16& message_text,
      bool is_reload,
      const DialogClosedCallback& callback) OVERRIDE;
  virtual bool HandleJavaScriptDialog(
      WebContents* web_contents,
      bool accept,
      const base::string16* prompt_override) OVERRIDE;
  virtual void CancelActiveAndPendingDialogs(
      WebContents* web_contents) OVERRIDE;
  virtual void WebContentsDestroyed(WebContents* web_contents) OVERRIDE;

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
  bool attached() const { return embedder_web_contents_ != NULL; }

  // Attaches this BrowserPluginGuest to the provided |embedder_web_contents|
  // and initializes the guest with the provided |params|. Attaching a guest
  // to an embedder implies that this guest's lifetime is no longer managed
  // by its opener, and it can begin loading resources. |extra_params| are
  // parameters passed into BrowserPlugin from JavaScript to be forwarded to
  // the content embedder.
  void Attach(WebContentsImpl* embedder_web_contents,
              BrowserPluginHostMsg_Attach_Params params,
              const base::DictionaryValue& extra_params);

  // Returns whether BrowserPluginGuest is interested in receiving the given
  // |message|.
  static bool ShouldForwardToBrowserPluginGuest(const IPC::Message& message);
  gfx::Rect ToGuestRect(const gfx::Rect& rect);

  void DragSourceEndedAt(int client_x, int client_y, int screen_x,
      int screen_y, blink::WebDragOperation operation);

  // Called when the drag started by this guest ends at an OS-level.
  void EndSystemDrag();

  // |this| takes ownership of |delegate|.
  void SetDelegate(BrowserPluginGuestDelegate* delegate);

  void RespondToPermissionRequest(int request_id,
                                  bool should_allow,
                                  const std::string& user_input);

  // Overrides factory for testing. Default (NULL) value indicates regular
  // (non-test) environment.
  static void set_factory_for_testing(BrowserPluginHostFactory* factory) {
    BrowserPluginGuest::factory_ = factory;
  }

  void SetZoom(double zoom_factor);

 private:
  class EmbedderWebContentsObserver;
  friend class TestBrowserPluginGuest;

  class DownloadRequest;
  class JavaScriptDialogRequest;
  class NewWindowRequest;
  class PermissionRequest;
  class PointerLockRequest;

  // Tracks the name, and target URL of the new window and whether or not it has
  // changed since the WebContents has been created and before the new window
  // has been attached to a BrowserPlugin. Once the first navigation commits, we
  // no longer track this information.
  struct NewWindowInfo {
    bool changed;
    GURL url;
    std::string name;
    NewWindowInfo(const GURL& url, const std::string& name) :
        changed(false),
        url(url),
        name(name) {}
  };

  // BrowserPluginGuest is a WebContentsObserver of |web_contents| and
  // |web_contents| has to stay valid for the lifetime of BrowserPluginGuest.
  BrowserPluginGuest(int instance_id,
                     bool has_render_view,
                     WebContentsImpl* web_contents,
                     BrowserPluginGuest* opener);

  // Destroy unattached new windows that have been opened by this
  // BrowserPluginGuest.
  void DestroyUnattachedWindows();

  void LoadURLWithParams(const GURL& url,
                         const Referrer& referrer,
                         PageTransition transition_type,
                         WebContents* web_contents);

  // Returns the |request_id| generated for the |request| provided.
  void RequestPermission(
      BrowserPluginPermissionType permission_type,
      scoped_refptr<BrowserPluginGuest::PermissionRequest> request,
      const base::DictionaryValue& request_info);

  // Creates a new guest window, and BrowserPluginGuest that is owned by this
  // BrowserPluginGuest.
  BrowserPluginGuest* CreateNewGuestWindow(const OpenURLParams& params);

  base::SharedMemory* damage_buffer() const { return damage_buffer_.get(); }
  const gfx::Size& damage_view_size() const { return damage_view_size_; }
  float damage_buffer_scale_factor() const {
    return damage_buffer_scale_factor_;
  }
  // Returns the damage buffer corresponding to the handle in resize |params|.
  base::SharedMemory* GetDamageBufferFromEmbedder(
      const BrowserPluginHostMsg_ResizeGuest_Params& params);

  bool InAutoSizeBounds(const gfx::Size& size) const;

  void RequestNewWindowPermission(WindowOpenDisposition disposition,
                                  const gfx::Rect& initial_bounds,
                                  bool user_gesture,
                                  WebContentsImpl* new_contents);

  // Message handlers for messages from embedder.

  void OnCompositorFrameSwappedACK(
      int instance_id,
      const FrameHostMsg_CompositorFrameSwappedACK_Params& params);
  void OnCopyFromCompositingSurfaceAck(int instance_id,
                                       int request_id,
                                       const SkBitmap& bitmap);
  // Handles drag events from the embedder.
  // When dragging, the drag events go to the embedder first, and if the drag
  // happens on the browser plugin, then the plugin sends a corresponding
  // drag-message to the guest. This routes the drag-message to the guest
  // renderer.
  void OnDragStatusUpdate(int instance_id,
                          blink::WebDragStatus drag_status,
                          const DropData& drop_data,
                          blink::WebDragOperationsMask drag_mask,
                          const gfx::Point& location);
  // Instructs the guest to execute an edit command decoded in the embedder.
  void OnExecuteEditCommand(int instance_id,
                            const std::string& command);

  // Returns compositor resources reclaimed in the embedder to the guest.
  void OnReclaimCompositorResources(
      int instance_id,
      const FrameHostMsg_ReclaimCompositorResources_Params& params);

  // Overriden in tests.
  virtual void OnHandleInputEvent(int instance_id,
                                  const gfx::Rect& guest_window_rect,
                                  const blink::WebInputEvent* event);
  void OnLockMouse(bool user_gesture,
                   bool last_unlocked_by_target,
                   bool privileged);
  void OnLockMouseAck(int instance_id, bool succeeded);
  void OnNavigateGuest(int instance_id, const std::string& src);
  void OnPluginDestroyed(int instance_id);
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
  void OnSetEditCommandsForNextKeyEvent(
      int instance_id,
      const std::vector<EditCommand>& edit_commands);
  void OnSetContentsOpaque(int instance_id, bool opaque);
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
  void OnUnlockMouse();
  void OnUnlockMouseAck(int instance_id);
  void OnUpdateGeometry(int instance_id, const gfx::Rect& view_rect);
  void OnUpdateRectACK(
      int instance_id,
      bool needs_ack,
      const BrowserPluginHostMsg_AutoSize_Params& auto_size_params,
      const BrowserPluginHostMsg_ResizeGuest_Params& resize_guest_params);

  void OnTextInputTypeChanged(ui::TextInputType type,
                              ui::TextInputMode input_mode,
                              bool can_compose_inline);
  void OnImeSetComposition(
      int instance_id,
      const std::string& text,
      const std::vector<blink::WebCompositionUnderline>& underlines,
      int selection_start,
      int selection_end);
  void OnImeConfirmComposition(
      int instance_id,
      const std::string& text,
      bool keep_selection);
  void OnExtendSelectionAndDelete(int instance_id, int before, int after);
  // Overridden in tests.
  virtual void OnImeCancelComposition();
#if defined(OS_MACOSX) || defined(USE_AURA)
  void OnImeCompositionRangeChanged(
      const gfx::Range& range,
      const std::vector<gfx::Rect>& character_bounds);
#endif

  // Message handlers for messages from guest.

  void OnDragStopped();
  void OnHandleInputEventAck(
      blink::WebInputEvent::Type event_type,
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
  void DidRetrieveDownloadURLFromRequestId(
      const std::string& request_method,
      const base::Callback<void(bool)>& callback,
      const GURL& url);

  // Forwards all messages from the |pending_messages_| queue to the embedder.
  void SendQueuedMessages();

  // Static factory instance (always NULL for non-test).
  static BrowserPluginHostFactory* factory_;

  scoped_ptr<EmbedderWebContentsObserver> embedder_web_contents_observer_;
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
  float guest_device_scale_factor_;
  gfx::Rect guest_window_rect_;
  gfx::Rect guest_screen_rect_;
  base::TimeDelta guest_hang_timeout_;
  bool focused_;
  bool mouse_locked_;
  bool pending_lock_request_;
  bool guest_visible_;
  bool guest_opaque_;
  bool embedder_visible_;
  std::string name_;
  bool auto_size_enabled_;
  gfx::Size max_auto_size_;
  gfx::Size min_auto_size_;

  // Each copy-request is identified by a unique number. The unique number is
  // used to keep track of the right callback.
  int copy_request_id_;
  typedef base::Callback<void(bool, const SkBitmap&)> CopyRequestCallback;
  typedef std::map<int, const CopyRequestCallback> CopyRequestMap;
  CopyRequestMap copy_request_callbacks_;

  typedef std::map<BrowserPluginGuest*, NewWindowInfo> PendingWindowMap;
  PendingWindowMap pending_new_windows_;
  base::WeakPtr<BrowserPluginGuest> opener_;
  // A counter to generate a unique request id for a permission request.
  // We only need the ids to be unique for a given BrowserPluginGuest.
  int next_permission_request_id_;

  // A map to store relevant info for a request keyed by the request's id.
  typedef std::map<int, scoped_refptr<PermissionRequest> > RequestMap;
  RequestMap permission_request_map_;

  // Indicates that this BrowserPluginGuest has associated renderer-side state.
  // This is used to determine whether or not to create a new RenderView when
  // this guest is attached.
  bool has_render_view_;

  // Last seen size of guest contents (by OnUpdateRect).
  gfx::Size last_seen_view_size_;
  // Last seen autosize attribute state (by OnUpdateRect).
  bool last_seen_auto_size_enabled_;

  bool is_in_destruction_;

  // Text input type states.
  ui::TextInputType last_text_input_type_;
  ui::TextInputMode last_input_mode_;
  bool last_can_compose_inline_;

  // This is a queue of messages that are destined to be sent to the embedder
  // once the guest is attached to a particular embedder.
  std::queue<IPC::Message*> pending_messages_;

  scoped_ptr<BrowserPluginGuestDelegate> delegate_;

  // These are parameters passed from JavaScript on attachment to the content
  // embedder.
  scoped_ptr<base::DictionaryValue> extra_attach_params_;

  // Weak pointer used to ask GeolocationPermissionContext about geolocation
  // permission.
  base::WeakPtrFactory<BrowserPluginGuest> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(BrowserPluginGuest);
};

}  // namespace content

#endif  // CONTENT_BROWSER_BROWSER_PLUGIN_BROWSER_PLUGIN_GUEST_H_
