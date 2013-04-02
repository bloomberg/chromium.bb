// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef  CONTENT_RENDERER_BROWSER_PLUGIN_BROWSER_PLUGIN_H_
#define  CONTENT_RENDERER_BROWSER_PLUGIN_BROWSER_PLUGIN_H_

#include "third_party/WebKit/Source/WebKit/chromium/public/WebPlugin.h"

#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/process_util.h"
#include "base/sequenced_task_runner_helpers.h"
#if defined(OS_WIN)
#include "base/shared_memory.h"
#endif
#include "content/common/browser_plugin/browser_plugin_message_enums.h"
#include "content/renderer/browser_plugin/browser_plugin_backing_store.h"
#include "content/renderer/browser_plugin/browser_plugin_bindings.h"
#include "content/renderer/mouse_lock_dispatcher.h"
#include "content/renderer/render_view_impl.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDragStatus.h"

struct BrowserPluginHostMsg_AutoSize_Params;
struct BrowserPluginHostMsg_ResizeGuest_Params;
struct BrowserPluginMsg_LoadCommit_Params;
struct BrowserPluginMsg_UpdateRect_Params;

namespace content {

class BrowserPluginCompositingHelper;
class BrowserPluginManager;
class MockBrowserPlugin;

class CONTENT_EXPORT BrowserPlugin :
    NON_EXPORTED_BASE(public WebKit::WebPlugin),
    public MouseLockDispatcher::LockTarget {
 public:
  RenderViewImpl* render_view() const { return render_view_.get(); }
  int render_view_routing_id() const { return render_view_routing_id_; }
  int instance_id() const { return instance_id_; }

  static BrowserPlugin* FromContainer(WebKit::WebPluginContainer* container);

  bool OnMessageReceived(const IPC::Message& msg);

  // Update Browser Plugin's DOM Node attribute |attribute_name| with the value
  // |attribute_value|.
  void UpdateDOMAttribute(const std::string& attribute_name,
                          const std::string& attribute_value);
  // Remove the DOM Node attribute with the name |attribute_name|.
  void RemoveDOMAttribute(const std::string& attribute_name);
  // Get Browser Plugin's DOM Node attribute |attribute_name|'s value.
  std::string GetDOMAttributeValue(const std::string& attribute_name) const;
  // Checks if the attribute |attribute_name| exists in the DOM.
  bool HasDOMAttribute(const std::string& attribute_name) const;

  // Get the name attribute value.
  std::string GetNameAttribute() const;
  // Parse the name attribute value.
  void ParseNameAttribute();
  // Get the src attribute value of the BrowserPlugin instance.
  std::string GetSrcAttribute() const;
  // Parse the src attribute value of the BrowserPlugin instance.
  bool ParseSrcAttribute(std::string* error_message);
  // Get the autosize attribute value.
  bool GetAutoSizeAttribute() const;
  // Parses the autosize attribute value.
  void ParseAutoSizeAttribute();
  // Get the maxheight attribute value.
  int GetMaxHeightAttribute() const;
  // Get the maxwidth attribute value.
  int GetMaxWidthAttribute() const;
  // Get the minheight attribute value.
  int GetMinHeightAttribute() const;
  // Get the minwidth attribute value.
  int GetMinWidthAttribute() const;
  // Parse the minwidth, maxwidth, minheight, and maxheight attribute values.
  void ParseSizeContraintsChanged();
  // The partition identifier string is stored as UTF-8.
  std::string GetPartitionAttribute() const;
  // This method can be successfully called only before the first navigation for
  // this instance of BrowserPlugin. If an error occurs, the |error_message| is
  // set appropriately to indicate the failure reason.
  bool ParsePartitionAttribute(std::string* error_message);
  // True if the partition attribute can be removed.
  bool CanRemovePartitionAttribute(std::string* error_message);

  bool InAutoSizeBounds(const gfx::Size& size) const;

  // Get the guest's DOMWindow proxy.
  NPObject* GetContentWindow() const;

  // Returns Chrome's process ID for the current guest.
  int guest_process_id() const { return guest_process_id_; }
  // Returns Chrome's route ID for the current guest.
  int guest_route_id() const { return guest_route_id_; }
  // Returns whether the guest process has crashed.
  bool guest_crashed() const { return guest_crashed_; }
  bool HasGuest() const;

  // Attaches the window identified by |window_id| to the the given node
  // encapsulating a BrowserPlugin.
  static bool AttachWindowTo(const WebKit::WebNode& node,
                             int window_id);

  // Query whether the guest can navigate back to the previous entry.
  bool CanGoBack() const;
  // Query whether the guest can navigation forward to the next entry.
  bool CanGoForward() const;

  // Informs the guest of an updated focus state.
  void UpdateGuestFocusState();
  // Indicates whether the guest should be focused.
  bool ShouldGuestBeFocused() const;

  // Tells the BrowserPlugin to tell the guest to navigate to the previous
  // navigation entry in the navigation history.
  void Back();
  // Tells the BrowserPlugin to tell the guest to navigate to the next
  // navigation entry in the navigation history.
  void Forward();
  // Tells the BrowserPlugin to tell the guest to navigate to a position
  // relative to the current index in its navigation history.
  void Go(int relativeIndex);
  // Tells the BrowserPlugin to terminate the guest process.
  void TerminateGuest();

  // A request from JavaScript has been made to stop the loading of the page.
  void Stop();
  // A request from JavaScript has been made to reload the page.
  void Reload();
  // A request to enable hardware compositing.
  void EnableCompositing(bool enable);
  // A request from content client to track lifetime of a JavaScript object
  // related to a permission request object.
  // This is used to clean up hanging permission request objects.
  void PersistRequestObject(const NPVariant* request,
                            const std::string& type,
                            int id);

  // Returns true if |point| lies within the bounds of the plugin rectangle.
  // Not OK to use this function for making security-sensitive decision since it
  // can return false positives when the plugin has rotation transformation
  // applied.
  bool InBounds(const gfx::Point& point) const;

  gfx::Point ToLocalCoordinates(const gfx::Point& point) const;
  // Called by browser plugin binding.
  void OnEmbedderDecidedPermission(int request_id, bool allow);

  // Sets the instance ID of the BrowserPlugin and requests a guest from the
  // browser process.
  void SetInstanceID(int instance_id, bool new_guest);

  // Notify the plugin about a compositor commit so that frame ACKs could be
  // sent, if needed.
  void DidCommitCompositorFrame();

  // Returns whether a message should be forwarded to BrowserPlugin.
  static bool ShouldForwardToBrowserPlugin(const IPC::Message& message);

  // WebKit::WebPlugin implementation.
  virtual WebKit::WebPluginContainer* container() const OVERRIDE;
  virtual bool initialize(WebKit::WebPluginContainer* container) OVERRIDE;
  virtual void destroy() OVERRIDE;
  virtual NPObject* scriptableObject() OVERRIDE;
  virtual bool supportsKeyboardFocus() const OVERRIDE;
  virtual bool canProcessDrag() const OVERRIDE;
  virtual void paint(
      WebKit::WebCanvas* canvas,
      const WebKit::WebRect& rect) OVERRIDE;
  virtual void updateGeometry(
      const WebKit::WebRect& frame_rect,
      const WebKit::WebRect& clip_rect,
      const WebKit::WebVector<WebKit::WebRect>& cut_outs_rects,
      bool is_visible) OVERRIDE;
  virtual void updateFocus(bool focused) OVERRIDE;
  virtual void updateVisibility(bool visible) OVERRIDE;
  virtual bool acceptsInputEvents() OVERRIDE;
  virtual bool handleInputEvent(
      const WebKit::WebInputEvent& event,
      WebKit::WebCursorInfo& cursor_info) OVERRIDE;
  virtual bool handleDragStatusUpdate(WebKit::WebDragStatus drag_status,
                                      const WebKit::WebDragData& drag_data,
                                      WebKit::WebDragOperationsMask mask,
                                      const WebKit::WebPoint& position,
                                      const WebKit::WebPoint& screen) OVERRIDE;
  virtual void didReceiveResponse(
      const WebKit::WebURLResponse& response) OVERRIDE;
  virtual void didReceiveData(const char* data, int data_length) OVERRIDE;
  virtual void didFinishLoading() OVERRIDE;
  virtual void didFailLoading(const WebKit::WebURLError& error) OVERRIDE;
  virtual void didFinishLoadingFrameRequest(
      const WebKit::WebURL& url,
      void* notify_data) OVERRIDE;
  virtual void didFailLoadingFrameRequest(
      const WebKit::WebURL& url,
      void* notify_data,
      const WebKit::WebURLError& error) OVERRIDE;

  // MouseLockDispatcher::LockTarget implementation.
  virtual void OnLockMouseACK(bool succeeded) OVERRIDE;
  virtual void OnMouseLockLost() OVERRIDE;
  virtual bool HandleMouseLockedInputEvent(
          const WebKit::WebMouseEvent& event) OVERRIDE;

 private:
  friend class base::DeleteHelper<BrowserPlugin>;
  // Only the manager is allowed to create a BrowserPlugin.
  friend class BrowserPluginManagerImpl;
  friend class MockBrowserPluginManager;

  // For unit/integration tests.
  friend class MockBrowserPlugin;

  // A BrowserPlugin object is a controller that represents an instance of a
  // browser plugin within the embedder renderer process. Each BrowserPlugin
  // within a process has a unique instance_id that is used to route messages
  // to it. It takes in a RenderViewImpl that it's associated with along
  // with the frame within which it lives and the initial attributes assigned
  // to it on creation.
  BrowserPlugin(
      RenderViewImpl* render_view,
      WebKit::WebFrame* frame,
      const WebKit::WebPluginParams& params);

  virtual ~BrowserPlugin();

  int width() const { return plugin_rect_.width(); }
  int height() const { return plugin_rect_.height(); }
  // Gets the Max Height value used for auto size.
  int GetAdjustedMaxHeight() const;
  // Gets the Max Width value used for auto size.
  int GetAdjustedMaxWidth() const;
  // Gets the Min Height value used for auto size.
  int GetAdjustedMinHeight() const;
  // Gets the Min Width value used for auto size.
  int GetAdjustedMinWidth() const;
  BrowserPluginManager* browser_plugin_manager() const {
    return browser_plugin_manager_;
  }

  // Virtual to allow for mocking in tests.
  virtual float GetDeviceScaleFactor() const;

  // Parses the attributes of the browser plugin from the element's attributes
  // and sets them appropriately.
  void ParseAttributes();

  // Triggers the event-listeners for |event_name|. Note that the function
  // frees all the values in |props|.
  void TriggerEvent(const std::string& event_name,
                    std::map<std::string, base::Value*>* props);

  // Creates and maps a shared damage buffer.
  virtual base::SharedMemory* CreateDamageBuffer(
      const size_t size,
      base::SharedMemoryHandle* shared_memory_handle);
  // Swaps out the |current_damage_buffer_| with the |pending_damage_buffer_|.
  void SwapDamageBuffers();

  // Populates BrowserPluginHostMsg_ResizeGuest_Params with resize state and
  // allocates a new |pending_damage_buffer_| if in software rendering mode.
  void PopulateResizeGuestParameters(
      BrowserPluginHostMsg_ResizeGuest_Params* params,
      const gfx::Size& view_size);

  // Populates BrowserPluginHostMsg_AutoSize_Params object with autosize state.
  void PopulateAutoSizeParameters(
      BrowserPluginHostMsg_AutoSize_Params* params, bool current_auto_size);

  // Populates both AutoSize and ResizeGuest parameters based on the current
  // autosize state.
  void GetDamageBufferWithSizeParams(
      BrowserPluginHostMsg_AutoSize_Params* auto_size_params,
      BrowserPluginHostMsg_ResizeGuest_Params* resize_guest_params);

  // Informs the guest of an updated autosize state.
  void UpdateGuestAutoSizeState(bool current_auto_size);

  // Informs the BrowserPlugin that guest has changed its size in autosize mode.
  void SizeChangedDueToAutoSize(const gfx::Size& old_view_size);

  // Indicates whether a damage buffer was used by the guest process for the
  // provided |params|.
  static bool UsesDamageBuffer(
      const BrowserPluginMsg_UpdateRect_Params& params);

  // Indicates whether the |pending_damage_buffer_| was used to copy over pixels
  // given the provided |params|.
  bool UsesPendingDamageBuffer(
      const BrowserPluginMsg_UpdateRect_Params& params);

  // Sets the instance ID of the BrowserPlugin and requests a guest from the
  // browser process.
  void SetInstanceID(int instance_id);

  void AddPermissionRequestToMap(int request_id,
                                 BrowserPluginPermissionType type);

  // Informs the BrowserPlugin that the guest's permission request has been
  // allowed or denied by the embedder.
  void RespondPermission(BrowserPluginPermissionType permission_type,
                         int request_id,
                         bool allow);

  // Handles the response to the pointerLock permission request.
  void RespondPermissionPointerLock(bool allow);

  // If the request with id |request_id| is pending then informs the
  // BrowserPlugin that the guest's permission request has been allowed or
  // denied by the embedder.
  void RespondPermissionIfRequestIsPending(int request_id, bool allow);
  // Cleans up pending permission request once the associated event.request
  // object goes out of scope in JavaScript.
  void OnRequestObjectGarbageCollected(int request_id);
  // V8 garbage collection callback for |object|.
  static void WeakCallbackForPersistObject(v8::Isolate* isolate,
                                           v8::Persistent<v8::Value> object,
                                           void* param);

  // IPC message handlers.
  // Please keep in alphabetical order.
  void OnAdvanceFocus(int instance_id, bool reverse);
  void OnBuffersSwapped(int instance_id,
                        const gfx::Size& size,
                        std::string mailbox_name,
                        int gpu_route_id,
                        int gpu_host_id);
  void OnCompositorFrameSwapped(const IPC::Message& message);
  void OnGuestContentWindowReady(int instance_id,
                                 int content_window_routing_id);
  void OnGuestGone(int instance_id, int process_id, int status);
  void OnGuestResponsive(int instance_id, int process_id);
  void OnGuestUnresponsive(int instance_id, int process_id);
  void OnLoadAbort(int instance_id,
                   const GURL& url,
                   bool is_top_level,
                   const std::string& type);
  void OnLoadCommit(int instance_id,
                    const BrowserPluginMsg_LoadCommit_Params& params);
  void OnLoadRedirect(int instance_id,
                      const GURL& old_url,
                      const GURL& new_url,
                      bool is_top_level);
  void OnLoadStart(int instance_id, const GURL& url, bool is_top_level);
  void OnLoadStop(int instance_id);
  // Requests permission from the embedder.
  void OnRequestPermission(int instance_id,
                           BrowserPluginPermissionType permission_type,
                           int request_id,
                           const base::DictionaryValue& request_info);
  void OnSetCursor(int instance_id, const WebCursor& cursor);
  void OnShouldAcceptTouchEvents(int instance_id, bool accept);
  void OnUnlockMouse(int instance_id);
  void OnUpdatedName(int instance_id, const std::string& name);
  void OnUpdateRect(int instance_id,
                    const BrowserPluginMsg_UpdateRect_Params& params);

  int instance_id_;
  base::WeakPtr<RenderViewImpl> render_view_;
  // We cache the |render_view_|'s routing ID because we need it on destruction.
  // If the |render_view_| is destroyed before the BrowserPlugin is destroyed
  // then we will attempt to access a NULL pointer.
  int render_view_routing_id_;
  WebKit::WebPluginContainer* container_;
  scoped_ptr<BrowserPluginBindings> bindings_;
  scoped_ptr<BrowserPluginBackingStore> backing_store_;
  scoped_ptr<base::SharedMemory> current_damage_buffer_;
  scoped_ptr<base::SharedMemory> pending_damage_buffer_;
  uint32 damage_buffer_sequence_id_;
  bool resize_ack_received_;
  gfx::Rect plugin_rect_;
  // Bitmap for crashed plugin. Lazily initialized, non-owning pointer.
  SkBitmap* sad_guest_;
  bool guest_crashed_;
  scoped_ptr<BrowserPluginHostMsg_ResizeGuest_Params> pending_resize_params_;
  bool auto_size_ack_pending_;
  int guest_process_id_;
  int guest_route_id_;
  std::string storage_partition_id_;
  bool persist_storage_;
  bool valid_partition_id_;
  int content_window_routing_id_;
  bool plugin_focused_;
  // Tracks the visibility of the browser plugin regardless of the whole
  // embedder RenderView's visibility.
  bool visible_;

  WebCursor cursor_;

  gfx::Size last_view_size_;
  bool size_changed_in_flight_;
  bool allocate_instance_id_sent_;

  // Each permission request item in the map is a pair of request id and
  // permission type.
  typedef std::map<int, BrowserPluginPermissionType> PendingPermissionRequests;
  PendingPermissionRequests pending_permission_requests_;

  typedef std::pair<int, base::WeakPtr<BrowserPlugin> >
      AliveV8PermissionRequestItem;
  std::map<int, AliveV8PermissionRequestItem*>
      alive_v8_permission_request_objects_;

  // BrowserPlugin outlives RenderViewImpl in Chrome Apps and so we need to
  // store the BrowserPlugin's BrowserPluginManager in a member variable to
  // avoid accessing the RenderViewImpl.
  scoped_refptr<BrowserPluginManager> browser_plugin_manager_;

  // Important: Do not add more history state here.
  // We strongly discourage storing additional history state (such as page IDs)
  // in the embedder process, at the risk of having incorrect information that
  // can lead to broken back/forward logic in apps.
  // It's also important that this state does not get modified by any logic in
  // the embedder process. It should only be updated in response to navigation
  // events in the guest.  No assumptions should be made about how the index
  // will change after a navigation (e.g., for back, forward, or go), because
  // the changes are not always obvious.  For example, there is a maximum
  // number of entries and earlier ones will automatically be pruned.
  int current_nav_entry_index_;
  int nav_entry_count_;

  // Used for HW compositing.
  bool compositing_enabled_;
  scoped_refptr<BrowserPluginCompositingHelper> compositing_helper_;

  // Weak factory used in v8 |MakeWeak| callback, since the v8 callback might
  // get called after BrowserPlugin has been destroyed.
  base::WeakPtrFactory<BrowserPlugin> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(BrowserPlugin);
};

}  // namespace content

#endif  // CONTENT_RENDERER_BROWSER_PLUGIN_BROWSER_PLUGIN_H_
