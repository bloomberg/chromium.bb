// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_VIEW_AURA_H_
#define CONTENT_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_VIEW_AURA_H_

#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/linked_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "cc/layers/delegated_frame_provider.h"
#include "cc/layers/delegated_frame_resource_collection.h"
#include "cc/resources/single_release_callback.h"
#include "cc/resources/texture_mailbox.h"
#include "content/browser/accessibility/browser_accessibility_manager.h"
#include "content/browser/compositor/image_transport_factory.h"
#include "content/browser/compositor/owned_mailbox.h"
#include "content/browser/renderer_host/delegated_frame_evictor.h"
#include "content/browser/renderer_host/render_widget_host_view_base.h"
#include "content/common/content_export.h"
#include "content/common/cursors/webcursor.h"
#include "content/common/gpu/client/gl_helper.h"
#include "third_party/skia/include/core/SkRegion.h"
#include "ui/aura/client/cursor_client_observer.h"
#include "ui/aura/client/focus_change_observer.h"
#include "ui/aura/window_delegate.h"
#include "ui/aura/window_tree_host_observer.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/compositor_observer.h"
#include "ui/compositor/compositor_vsync_manager.h"
#include "ui/compositor/layer_owner_delegate.h"
#include "ui/gfx/display_observer.h"
#include "ui/gfx/rect.h"
#include "ui/wm/public/activation_change_observer.h"
#include "ui/wm/public/activation_delegate.h"

namespace aura {
class WindowTracker;
namespace client {
class ScopedTooltipDisabler;
}
}

namespace cc {
class CopyOutputRequest;
class CopyOutputResult;
class DelegatedFrameData;
}

namespace gfx {
class Canvas;
class Display;
}

namespace gpu {
struct Mailbox;
}

namespace ui {
class CompositorLock;
class CompositorVSyncManager;
class InputMethod;
class LocatedEvent;
class Texture;
}

namespace content {
#if defined(OS_WIN)
class LegacyRenderWidgetHostHWND;
#endif

class RenderFrameHostImpl;
class RenderWidgetHostImpl;
class RenderWidgetHostView;
class ResizeLock;

// RenderWidgetHostView class hierarchy described in render_widget_host_view.h.
class CONTENT_EXPORT RenderWidgetHostViewAura
    : public RenderWidgetHostViewBase,
      public ui::CompositorObserver,
      public ui::CompositorVSyncManager::Observer,
      public ui::LayerOwnerDelegate,
      public ui::TextInputClient,
      public gfx::DisplayObserver,
      public aura::WindowTreeHostObserver,
      public aura::WindowDelegate,
      public aura::client::ActivationDelegate,
      public aura::client::ActivationChangeObserver,
      public aura::client::FocusChangeObserver,
      public aura::client::CursorClientObserver,
      public ImageTransportFactoryObserver,
      public BrowserAccessibilityDelegate,
      public DelegatedFrameEvictorClient,
      public base::SupportsWeakPtr<RenderWidgetHostViewAura>,
      public cc::DelegatedFrameResourceCollectionClient {
 public:
  // Displays and controls touch editing elements such as selection handles.
  class TouchEditingClient {
   public:
    TouchEditingClient() {}

    // Tells the client to start showing touch editing handles.
    virtual void StartTouchEditing() = 0;

    // Notifies the client that touch editing is no longer needed. |quick|
    // determines whether the handles should fade out quickly or slowly.
    virtual void EndTouchEditing(bool quick) = 0;

    // Notifies the client that the selection bounds need to be updated.
    virtual void OnSelectionOrCursorChanged(const gfx::Rect& anchor,
                                            const gfx::Rect& focus) = 0;

    // Notifies the client that the current text input type as changed.
    virtual void OnTextInputTypeChanged(ui::TextInputType type) = 0;

    // Notifies the client that an input event is about to be sent to the
    // renderer. Returns true if the client wants to stop event propagation.
    virtual bool HandleInputEvent(const ui::Event* event) = 0;

    // Notifies the client that a gesture event ack was received.
    virtual void GestureEventAck(int gesture_event_type) = 0;

    // This is called when the view is destroyed, so that the client can
    // perform any necessary clean-up.
    virtual void OnViewDestroyed() = 0;

   protected:
    virtual ~TouchEditingClient() {}
  };

  void set_touch_editing_client(TouchEditingClient* client) {
    touch_editing_client_ = client;
  }

  // RenderWidgetHostView implementation.
  virtual bool OnMessageReceived(const IPC::Message& msg) OVERRIDE;
  virtual void InitAsChild(gfx::NativeView parent_view) OVERRIDE;
  virtual RenderWidgetHost* GetRenderWidgetHost() const OVERRIDE;
  virtual void SetSize(const gfx::Size& size) OVERRIDE;
  virtual void SetBounds(const gfx::Rect& rect) OVERRIDE;
  virtual gfx::NativeView GetNativeView() const OVERRIDE;
  virtual gfx::NativeViewId GetNativeViewId() const OVERRIDE;
  virtual gfx::NativeViewAccessible GetNativeViewAccessible() OVERRIDE;
  virtual bool HasFocus() const OVERRIDE;
  virtual bool IsSurfaceAvailableForCopy() const OVERRIDE;
  virtual void Show() OVERRIDE;
  virtual void Hide() OVERRIDE;
  virtual bool IsShowing() OVERRIDE;
  virtual gfx::Rect GetViewBounds() const OVERRIDE;
  virtual void SetBackground(const SkBitmap& background) OVERRIDE;

  // Overridden from RenderWidgetHostViewPort:
  virtual void InitAsPopup(RenderWidgetHostView* parent_host_view,
                           const gfx::Rect& pos) OVERRIDE;
  virtual void InitAsFullscreen(
      RenderWidgetHostView* reference_host_view) OVERRIDE;
  virtual void WasShown() OVERRIDE;
  virtual void WasHidden() OVERRIDE;
  virtual void MovePluginWindows(
      const gfx::Vector2d& scroll_offset,
      const std::vector<WebPluginGeometry>& moves) OVERRIDE;
  virtual void Focus() OVERRIDE;
  virtual void Blur() OVERRIDE;
  virtual void UpdateCursor(const WebCursor& cursor) OVERRIDE;
  virtual void SetIsLoading(bool is_loading) OVERRIDE;
  virtual void TextInputTypeChanged(ui::TextInputType type,
                                    ui::TextInputMode input_mode,
                                    bool can_compose_inline) OVERRIDE;
  virtual void ImeCancelComposition() OVERRIDE;
  virtual void ImeCompositionRangeChanged(
      const gfx::Range& range,
      const std::vector<gfx::Rect>& character_bounds) OVERRIDE;
  virtual void DidUpdateBackingStore(
      const gfx::Rect& scroll_rect,
      const gfx::Vector2d& scroll_delta,
      const std::vector<gfx::Rect>& copy_rects,
      const std::vector<ui::LatencyInfo>& latency_info) OVERRIDE;
  virtual void RenderProcessGone(base::TerminationStatus status,
                                 int error_code) OVERRIDE;
  virtual void Destroy() OVERRIDE;
  virtual void SetTooltipText(const base::string16& tooltip_text) OVERRIDE;
  virtual void SelectionChanged(const base::string16& text,
                                size_t offset,
                                const gfx::Range& range) OVERRIDE;
  virtual void SelectionBoundsChanged(
      const ViewHostMsg_SelectionBounds_Params& params) OVERRIDE;
  virtual void ScrollOffsetChanged() OVERRIDE;
  virtual BackingStore* AllocBackingStore(const gfx::Size& size) OVERRIDE;
  virtual void CopyFromCompositingSurface(
      const gfx::Rect& src_subrect,
      const gfx::Size& dst_size,
      const base::Callback<void(bool, const SkBitmap&)>& callback,
      const SkBitmap::Config config) OVERRIDE;
  virtual void CopyFromCompositingSurfaceToVideoFrame(
      const gfx::Rect& src_subrect,
      const scoped_refptr<media::VideoFrame>& target,
      const base::Callback<void(bool)>& callback) OVERRIDE;
  virtual bool CanCopyToVideoFrame() const OVERRIDE;
  virtual bool CanSubscribeFrame() const OVERRIDE;
  virtual void BeginFrameSubscription(
      scoped_ptr<RenderWidgetHostViewFrameSubscriber> subscriber) OVERRIDE;
  virtual void EndFrameSubscription() OVERRIDE;
  virtual void OnAcceleratedCompositingStateChange() OVERRIDE;
  virtual void AcceleratedSurfaceInitialized(int host_id,
                                             int route_id) OVERRIDE;
  virtual void AcceleratedSurfaceBuffersSwapped(
      const GpuHostMsg_AcceleratedSurfaceBuffersSwapped_Params& params_in_pixel,
      int gpu_host_id) OVERRIDE;
  virtual void AcceleratedSurfacePostSubBuffer(
      const GpuHostMsg_AcceleratedSurfacePostSubBuffer_Params& params_in_pixel,
      int gpu_host_id) OVERRIDE;
  virtual void AcceleratedSurfaceSuspend() OVERRIDE;
  virtual void AcceleratedSurfaceRelease() OVERRIDE;
  virtual bool HasAcceleratedSurface(const gfx::Size& desired_size) OVERRIDE;
  virtual void GetScreenInfo(blink::WebScreenInfo* results) OVERRIDE;
  virtual gfx::Rect GetBoundsInRootWindow() OVERRIDE;
  virtual void GestureEventAck(const blink::WebGestureEvent& event,
                               InputEventAckState ack_result) OVERRIDE;
  virtual void ProcessAckedTouchEvent(
      const TouchEventWithLatencyInfo& touch,
      InputEventAckState ack_result) OVERRIDE;
  virtual scoped_ptr<SyntheticGestureTarget> CreateSyntheticGestureTarget()
      OVERRIDE;
  virtual void SetHasHorizontalScrollbar(
      bool has_horizontal_scrollbar) OVERRIDE;
  virtual void SetScrollOffsetPinning(
      bool is_pinned_to_left, bool is_pinned_to_right) OVERRIDE;
  virtual gfx::GLSurfaceHandle GetCompositingSurface() OVERRIDE;
  virtual void CreateBrowserAccessibilityManagerIfNeeded() OVERRIDE;
  virtual bool LockMouse() OVERRIDE;
  virtual void UnlockMouse() OVERRIDE;
  virtual void OnSwapCompositorFrame(
      uint32 output_surface_id,
      scoped_ptr<cc::CompositorFrame> frame) OVERRIDE;
#if defined(OS_WIN)
  virtual void SetParentNativeViewAccessible(
      gfx::NativeViewAccessible accessible_parent) OVERRIDE;
  virtual gfx::NativeViewId GetParentForWindowlessPlugin() const OVERRIDE;
#endif

  // Overridden from ui::TextInputClient:
  virtual void SetCompositionText(
      const ui::CompositionText& composition) OVERRIDE;
  virtual void ConfirmCompositionText() OVERRIDE;
  virtual void ClearCompositionText() OVERRIDE;
  virtual void InsertText(const base::string16& text) OVERRIDE;
  virtual void InsertChar(base::char16 ch, int flags) OVERRIDE;
  virtual gfx::NativeWindow GetAttachedWindow() const OVERRIDE;
  virtual ui::TextInputType GetTextInputType() const OVERRIDE;
  virtual ui::TextInputMode GetTextInputMode() const OVERRIDE;
  virtual bool CanComposeInline() const OVERRIDE;
  virtual gfx::Rect GetCaretBounds() const OVERRIDE;
  virtual bool GetCompositionCharacterBounds(uint32 index,
                                             gfx::Rect* rect) const OVERRIDE;
  virtual bool HasCompositionText() const OVERRIDE;
  virtual bool GetTextRange(gfx::Range* range) const OVERRIDE;
  virtual bool GetCompositionTextRange(gfx::Range* range) const OVERRIDE;
  virtual bool GetSelectionRange(gfx::Range* range) const OVERRIDE;
  virtual bool SetSelectionRange(const gfx::Range& range) OVERRIDE;
  virtual bool DeleteRange(const gfx::Range& range) OVERRIDE;
  virtual bool GetTextFromRange(const gfx::Range& range,
                                base::string16* text) const OVERRIDE;
  virtual void OnInputMethodChanged() OVERRIDE;
  virtual bool ChangeTextDirectionAndLayoutAlignment(
      base::i18n::TextDirection direction) OVERRIDE;
  virtual void ExtendSelectionAndDelete(size_t before, size_t after) OVERRIDE;
  virtual void EnsureCaretInRect(const gfx::Rect& rect) OVERRIDE;
  virtual void OnCandidateWindowShown() OVERRIDE;
  virtual void OnCandidateWindowUpdated() OVERRIDE;
  virtual void OnCandidateWindowHidden() OVERRIDE;

  // Overridden from gfx::DisplayObserver:
  virtual void OnDisplayBoundsChanged(const gfx::Display& display) OVERRIDE;
  virtual void OnDisplayAdded(const gfx::Display& new_display) OVERRIDE;
  virtual void OnDisplayRemoved(const gfx::Display& old_display) OVERRIDE;

  // Overridden from aura::WindowDelegate:
  virtual gfx::Size GetMinimumSize() const OVERRIDE;
  virtual gfx::Size GetMaximumSize() const OVERRIDE;
  virtual void OnBoundsChanged(const gfx::Rect& old_bounds,
                               const gfx::Rect& new_bounds) OVERRIDE;
  virtual gfx::NativeCursor GetCursor(const gfx::Point& point) OVERRIDE;
  virtual int GetNonClientComponent(const gfx::Point& point) const OVERRIDE;
  virtual bool ShouldDescendIntoChildForEventHandling(
      aura::Window* child,
      const gfx::Point& location) OVERRIDE;
  virtual bool CanFocus() OVERRIDE;
  virtual void OnCaptureLost() OVERRIDE;
  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE;
  virtual void OnDeviceScaleFactorChanged(float device_scale_factor) OVERRIDE;
  virtual void OnWindowDestroying(aura::Window* window) OVERRIDE;
  virtual void OnWindowDestroyed(aura::Window* window) OVERRIDE;
  virtual void OnWindowTargetVisibilityChanged(bool visible) OVERRIDE;
  virtual bool HasHitTestMask() const OVERRIDE;
  virtual void GetHitTestMask(gfx::Path* mask) const OVERRIDE;

  // Overridden from ui::EventHandler:
  virtual void OnKeyEvent(ui::KeyEvent* event) OVERRIDE;
  virtual void OnMouseEvent(ui::MouseEvent* event) OVERRIDE;
  virtual void OnScrollEvent(ui::ScrollEvent* event) OVERRIDE;
  virtual void OnTouchEvent(ui::TouchEvent* event) OVERRIDE;
  virtual void OnGestureEvent(ui::GestureEvent* event) OVERRIDE;

  // Overridden from aura::client::ActivationDelegate:
  virtual bool ShouldActivate() const OVERRIDE;

  // Overridden from aura::client::ActivationChangeObserver:
  virtual void OnWindowActivated(aura::Window* gained_activation,
                                 aura::Window* lost_activation) OVERRIDE;

  // Overridden from aura::client::CursorClientObserver:
  virtual void OnCursorVisibilityChanged(bool is_visible) OVERRIDE;

  // Overridden from aura::client::FocusChangeObserver:
  virtual void OnWindowFocused(aura::Window* gained_focus,
                               aura::Window* lost_focus) OVERRIDE;

  // Overridden from aura::WindowTreeHostObserver:
  virtual void OnHostMoved(const aura::WindowTreeHost* host,
                           const gfx::Point& new_origin) OVERRIDE;

  bool CanCopyToBitmap() const;

  void OnTextInputStateChanged(const ViewHostMsg_TextInputState_Params& params);

#if defined(OS_WIN)
  // Sets the cutout rects from constrained windows. These are rectangles that
  // windowed NPAPI plugins shouldn't paint in. Overwrites any previous cutout
  // rects.
  void UpdateConstrainedWindowRects(const std::vector<gfx::Rect>& rects);

  // Updates the cursor clip region. Used for mouse locking.
  void UpdateMouseLockRegion();
#endif

  // Method to indicate if this instance is shutting down or closing.
  // TODO(shrikant): Discuss around to see if it makes sense to add this method
  // as part of RenderWidgetHostView.
  bool IsClosing() const { return in_shutdown_; }

 protected:
  friend class RenderWidgetHostView;
  virtual ~RenderWidgetHostViewAura();

  // Should be constructed via RenderWidgetHostView::CreateViewForWidget.
  explicit RenderWidgetHostViewAura(RenderWidgetHost* host);

  RenderWidgetHostViewFrameSubscriber* frame_subscriber() const {
    return frame_subscriber_.get();
  }

  virtual bool ShouldCreateResizeLock();
  virtual scoped_ptr<ResizeLock> CreateResizeLock(bool defer_compositor_lock);

  virtual void RequestCopyOfOutput(scoped_ptr<cc::CopyOutputRequest> request);

  // Exposed for tests.
  aura::Window* window() { return window_; }
  gfx::Size current_frame_size_in_dip() const {
    return current_frame_size_in_dip_;
  }
  void LockResources();
  void UnlockResources();

  // Overridden from ui::CompositorObserver:
  virtual void OnCompositingDidCommit(ui::Compositor* compositor) OVERRIDE;
  virtual void OnCompositingStarted(ui::Compositor* compositor,
                                    base::TimeTicks start_time) OVERRIDE;
  virtual void OnCompositingEnded(ui::Compositor* compositor) OVERRIDE;
  virtual void OnCompositingAborted(ui::Compositor* compositor) OVERRIDE;
  virtual void OnCompositingLockStateChanged(
      ui::Compositor* compositor) OVERRIDE;

  // Overridden from ui::CompositorVSyncManager::Observer:
  virtual void OnUpdateVSyncParameters(base::TimeTicks timebase,
                                       base::TimeDelta interval) OVERRIDE;
  virtual SkBitmap::Config PreferredReadbackFormat() OVERRIDE;

  // Overridden from ui::LayerOwnerObserver:
  virtual void OnLayerRecreated(ui::Layer* old_layer,
                                ui::Layer* new_layer) OVERRIDE;

 private:
  FRIEND_TEST_ALL_PREFIXES(RenderWidgetHostViewAuraTest, SetCompositionText);
  FRIEND_TEST_ALL_PREFIXES(RenderWidgetHostViewAuraTest, TouchEventState);
  FRIEND_TEST_ALL_PREFIXES(RenderWidgetHostViewAuraTest, TouchEventSyncAsync);
  FRIEND_TEST_ALL_PREFIXES(RenderWidgetHostViewAuraTest, SwapNotifiesWindow);
  FRIEND_TEST_ALL_PREFIXES(RenderWidgetHostViewAuraTest,
                           SkippedDelegatedFrames);
  FRIEND_TEST_ALL_PREFIXES(RenderWidgetHostViewAuraTest, OutputSurfaceIdChange);
  FRIEND_TEST_ALL_PREFIXES(RenderWidgetHostViewAuraTest,
                           DiscardDelegatedFrames);
  FRIEND_TEST_ALL_PREFIXES(RenderWidgetHostViewAuraTest,
                           DiscardDelegatedFramesWithLocking);
  FRIEND_TEST_ALL_PREFIXES(RenderWidgetHostViewAuraTest, SoftwareDPIChange);
  FRIEND_TEST_ALL_PREFIXES(RenderWidgetHostViewAuraTest,
                           UpdateCursorIfOverSelf);
  FRIEND_TEST_ALL_PREFIXES(RenderWidgetHostViewAuraCopyRequestTest,
                           DestroyedAfterCopyRequest);

  class WindowObserver;
  friend class WindowObserver;

  // Overridden from ImageTransportFactoryObserver:
  virtual void OnLostResources() OVERRIDE;

  // Overridden from BrowserAccessibilityDelegate:
  virtual void SetAccessibilityFocus(int acc_obj_id) OVERRIDE;
  virtual void AccessibilityDoDefaultAction(int acc_obj_id) OVERRIDE;
  virtual void AccessibilityScrollToMakeVisible(
      int acc_obj_id, gfx::Rect subfocus) OVERRIDE;
  virtual void AccessibilityScrollToPoint(
      int acc_obj_id, gfx::Point point) OVERRIDE;
  virtual void AccessibilitySetTextSelection(
      int acc_obj_id, int start_offset, int end_offset) OVERRIDE;
  virtual gfx::Point GetLastTouchEventLocation() const OVERRIDE;
  virtual void FatalAccessibilityTreeError() OVERRIDE;

  void UpdateCursorIfOverSelf();
  bool ShouldSkipFrame(gfx::Size size_in_dip) const;

  // Set the bounds of the window and handle size changes.  Assumes the caller
  // has already adjusted the origin of |rect| to conform to whatever coordinate
  // space is required by the aura::Window.
  void InternalSetBounds(const gfx::Rect& rect);

  // Lazily grab a resize lock if the aura window size doesn't match the current
  // frame size, to give time to the renderer.
  void MaybeCreateResizeLock();

  // Checks if the resize lock can be released because we received an new frame.
  void CheckResizeLock();

  ui::InputMethod* GetInputMethod() const;

  // Returns whether the widget needs an input grab to work properly.
  bool NeedsInputGrab();

  // Confirm existing composition text in the webpage and ask the input method
  // to cancel its ongoing composition session.
  void FinishImeCompositionSession();

  // This method computes movementX/Y and keeps track of mouse location for
  // mouse lock on all mouse move events.
  void ModifyEventMovementAndCoords(blink::WebMouseEvent* event);

  // Sends an IPC to the renderer process to communicate whether or not
  // the mouse cursor is visible anywhere on the screen.
  void NotifyRendererOfCursorVisibilityState(bool is_visible);

  // If |clip| is non-empty and and doesn't contain |rect| or |clip| is empty
  // SchedulePaint() is invoked for |rect|.
  void SchedulePaintIfNotInClip(const gfx::Rect& rect, const gfx::Rect& clip);

  // Helper method to determine if, in mouse locked mode, the cursor should be
  // moved to center.
  bool ShouldMoveToCenter();

  // Run all on compositing commit callbacks.
  void RunOnCommitCallbacks();

  // Add on compositing commit callback.
  void AddOnCommitCallbackAndDisableLocks(const base::Closure& callback);

  // Called after |window_| is parented to a WindowEventDispatcher.
  void AddedToRootWindow();

  // Called prior to removing |window_| from a WindowEventDispatcher.
  void RemovingFromRootWindow();

  // Called after commit for the last reference to the texture going away
  // after it was released as the frontbuffer.
  void SetSurfaceNotInUseByCompositor(scoped_refptr<ui::Texture>);

  // Called after async thumbnailer task completes.  Scales and crops the result
  // of the copy.
  static void CopyFromCompositingSurfaceHasResult(
      const gfx::Size& dst_size_in_pixel,
      const SkBitmap::Config config,
      const base::Callback<void(bool, const SkBitmap&)>& callback,
      scoped_ptr<cc::CopyOutputResult> result);
  static void PrepareTextureCopyOutputResult(
      const gfx::Size& dst_size_in_pixel,
      const SkBitmap::Config config,
      const base::Callback<void(bool, const SkBitmap&)>& callback,
      scoped_ptr<cc::CopyOutputResult> result);
  static void PrepareBitmapCopyOutputResult(
      const gfx::Size& dst_size_in_pixel,
      const SkBitmap::Config config,
      const base::Callback<void(bool, const SkBitmap&)>& callback,
      scoped_ptr<cc::CopyOutputResult> result);
  static void CopyFromCompositingSurfaceHasResultForVideo(
      base::WeakPtr<RenderWidgetHostViewAura> rwhva,
      scoped_refptr<OwnedMailbox> subscriber_texture,
      scoped_refptr<media::VideoFrame> video_frame,
      const base::Callback<void(bool)>& callback,
      scoped_ptr<cc::CopyOutputResult> result);
  static void CopyFromCompositingSurfaceFinishedForVideo(
      base::WeakPtr<RenderWidgetHostViewAura> rwhva,
      const base::Callback<void(bool)>& callback,
      scoped_refptr<OwnedMailbox> subscriber_texture,
      scoped_ptr<cc::SingleReleaseCallback> release_callback,
      bool result);
  static void ReturnSubscriberTexture(
      base::WeakPtr<RenderWidgetHostViewAura> rwhva,
      scoped_refptr<OwnedMailbox> subscriber_texture,
      uint32 sync_point);

  ui::Compositor* GetCompositor() const;

  // Detaches |this| from the input method object.
  void DetachFromInputMethod();

  // Before calling RenderWidgetHost::ForwardKeyboardEvent(), this method
  // calls our keybindings handler against the event and send matched
  // edit commands to renderer instead.
  void ForwardKeyboardEvent(const NativeWebKeyboardEvent& event);

  // Dismisses a Web Popup on a mouse or touch press outside the popup and its
  // parent.
  void ApplyEventFilterForPopupExit(ui::LocatedEvent* event);

  // Converts |rect| from window coordinate to screen coordinate.
  gfx::Rect ConvertRectToScreen(const gfx::Rect& rect) const;

  // Converts |rect| from screen coordinate to window coordinate.
  gfx::Rect ConvertRectFromScreen(const gfx::Rect& rect) const;

  void SwapDelegatedFrame(
      uint32 output_surface_id,
      scoped_ptr<cc::DelegatedFrameData> frame_data,
      float frame_device_scale_factor,
      const std::vector<ui::LatencyInfo>& latency_info);
  void SendDelegatedFrameAck(uint32 output_surface_id);
  void SendReturnedDelegatedResources(uint32 output_surface_id);

  // DelegatedFrameEvictorClient implementation.
  virtual void EvictDelegatedFrame() OVERRIDE;

  // cc::DelegatedFrameProviderClient implementation.
  virtual void UnusedResourcesAreAvailable() OVERRIDE;

  void DidReceiveFrameFromRenderer();

  // Helper function to set keyboard focus to the main window.
  void SetKeyboardFocus();

  RenderFrameHostImpl* GetFocusedFrame();

  // The model object.
  RenderWidgetHostImpl* host_;

  aura::Window* window_;

  scoped_ptr<WindowObserver> window_observer_;

  // Are we in the process of closing?  Tracked so fullscreen views can avoid
  // sending a second shutdown request to the host when they lose the focus
  // after requesting shutdown for another reason (e.g. Escape key).
  bool in_shutdown_;

  // True if in the process of handling a window bounds changed notification.
  bool in_bounds_changed_;

  // Is this a fullscreen view?
  bool is_fullscreen_;

  // Our parent host view, if this is a popup.  NULL otherwise.
  RenderWidgetHostViewAura* popup_parent_host_view_;

  // Our child popup host. NULL if we do not have a child popup.
  RenderWidgetHostViewAura* popup_child_host_view_;

  class EventFilterForPopupExit;
  friend class EventFilterForPopupExit;
  scoped_ptr<ui::EventHandler> event_filter_for_popup_exit_;

  // True when content is being loaded. Used to show an hourglass cursor.
  bool is_loading_;

  // The cursor for the page. This is passed up from the renderer.
  WebCursor current_cursor_;

  // The touch-event. Its touch-points are updated as necessary. A new
  // touch-point is added from an ET_TOUCH_PRESSED event, and a touch-point is
  // removed from the list on an ET_TOUCH_RELEASED event.
  blink::WebTouchEvent touch_event_;

  // The current text input type.
  ui::TextInputType text_input_type_;
  // The current text input mode corresponding to HTML5 inputmode attribute.
  ui::TextInputMode text_input_mode_;
  bool can_compose_inline_;

  // Rectangles for the selection anchor and focus.
  gfx::Rect selection_anchor_rect_;
  gfx::Rect selection_focus_rect_;

  // The current composition character bounds.
  std::vector<gfx::Rect> composition_character_bounds_;

  // Indicates if there is onging composition text.
  bool has_composition_text_;

  // Whether return characters should be passed on to the RenderWidgetHostImpl.
  bool accept_return_character_;

  // Current tooltip text.
  base::string16 tooltip_;

  std::vector<base::Closure> on_compositing_did_commit_callbacks_;

  // The vsync manager we are observing for changes, if any.
  scoped_refptr<ui::CompositorVSyncManager> vsync_manager_;

  // With delegated renderer, this is the last output surface, used to
  // disambiguate resources with the same id coming from different output
  // surfaces.
  uint32 last_output_surface_id_;

  // The number of delegated frame acks that are pending, to delay resource
  // returns until the acks are sent.
  int pending_delegated_ack_count_;

  // True after a delegated frame has been skipped, until a frame is not
  // skipped.
  bool skipped_frames_;

  // Holds delegated resources that have been given to a DelegatedFrameProvider,
  // and gives back resources when they are no longer in use for return to the
  // renderer.
  scoped_refptr<cc::DelegatedFrameResourceCollection> resource_collection_;

  // Provides delegated frame updates to the cc::DelegatedRendererLayer.
  scoped_refptr<cc::DelegatedFrameProvider> frame_provider_;

  // The size and scale of the last software compositing frame that was swapped.
  gfx::Size last_swapped_software_frame_size_;
  float last_swapped_software_frame_scale_factor_;

  // If non-NULL we're in OnPaint() and this is the supplied canvas.
  gfx::Canvas* paint_canvas_;

  // Used to record the last position of the mouse.
  // While the mouse is locked, they store the last known position just as mouse
  // lock was entered.
  // Relative to the upper-left corner of the view.
  gfx::Point unlocked_mouse_position_;
  // Relative to the upper-left corner of the screen.
  gfx::Point unlocked_global_mouse_position_;
  // Last cursor position relative to screen. Used to compute movementX/Y.
  gfx::Point global_mouse_position_;
  // In mouse locked mode, we syntheticaly move the mouse cursor to the center
  // of the window when it reaches the window borders to avoid it going outside.
  // This flag is used to differentiate between these synthetic mouse move
  // events vs. normal mouse move events.
  bool synthetic_move_sent_;

  // This lock is the one waiting for a frame of the right size to come back
  // from the renderer/GPU process. It is set from the moment the aura window
  // got resized, to the moment we committed the renderer frame of the same
  // size. It keeps track of the size we expect from the renderer, and locks the
  // compositor, as well as the UI for a short time to give a chance to the
  // renderer of producing a frame of the right size.
  scoped_ptr<ResizeLock> resize_lock_;

  // Keeps track of the current frame size.
  gfx::Size current_frame_size_in_dip_;

  // This lock is for waiting for a front surface to become available to draw.
  scoped_refptr<ui::CompositorLock> released_front_lock_;

  // Used to track the state of the window we're created from. Only used when
  // created fullscreen.
  scoped_ptr<aura::WindowTracker> host_tracker_;

  enum CanLockCompositorState {
    YES,
    // We locked, so at some point we'll need to kick a frame.
    YES_DID_LOCK,
    // No. A lock timed out, we need to kick a new frame before locking again.
    NO_PENDING_RENDERER_FRAME,
    // No. We've got a frame, but it hasn't been committed.
    NO_PENDING_COMMIT,
  };
  CanLockCompositorState can_lock_compositor_;

  // Used to track the last cursor visibility update that was sent to the
  // renderer via NotifyRendererOfCursorVisibilityState().
  enum CursorVisibilityState {
    UNKNOWN,
    VISIBLE,
    NOT_VISIBLE,
  };
  CursorVisibilityState cursor_visibility_state_in_renderer_;

#if defined(OS_WIN)
  // The list of rectangles from constrained windows over this view. Windowed
  // NPAPI plugins shouldn't draw over them.
  std::vector<gfx::Rect> constrained_rects_;

  typedef std::map<HWND, WebPluginGeometry> PluginWindowMoves;
  // Contains information about each windowed plugin's clip and cutout rects (
  // from the renderer). This is needed because when the transient windoiws
  // over this view changes, we need this information in order to create a new
  // region for the HWND.
  PluginWindowMoves plugin_window_moves_;
#endif

  base::TimeTicks last_draw_ended_;

  // Subscriber that listens to frame presentation events.
  scoped_ptr<RenderWidgetHostViewFrameSubscriber> frame_subscriber_;
  std::vector<scoped_refptr<OwnedMailbox> > idle_frame_subscriber_textures_;
  std::set<OwnedMailbox*> active_frame_subscriber_textures_;

  // YUV readback pipeline.
  scoped_ptr<content::ReadbackYUVInterface>
      yuv_readback_pipeline_;

  TouchEditingClient* touch_editing_client_;

  std::vector<ui::LatencyInfo> software_latency_info_;

  scoped_ptr<DelegatedFrameEvictor> delegated_frame_evictor_;

  scoped_ptr<aura::client::ScopedTooltipDisabler> tooltip_disabler_;

  base::WeakPtrFactory<RenderWidgetHostViewAura> weak_ptr_factory_;

#if defined(OS_WIN)
  // The LegacyRenderWidgetHostHWND class provides a dummy HWND which is used
  // for accessibility, as the container for windowless plugins like
  // Flash/Silverlight, etc and for legacy drivers for trackpoints/trackpads,
  // etc.
  scoped_ptr<content::LegacyRenderWidgetHostHWND>
      legacy_render_widget_host_HWND_;
#endif
  DISALLOW_COPY_AND_ASSIGN(RenderWidgetHostViewAura);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_VIEW_AURA_H_
