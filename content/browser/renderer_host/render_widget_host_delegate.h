// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_DELEGATE_H_
#define CONTENT_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_DELEGATE_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "base/callback.h"
#include "build/build_config.h"
#include "content/common/content_export.h"
#include "content/common/drag_event_source_info.h"
#include "content/public/common/drop_data.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "third_party/blink/public/common/manifest/web_display_mode.h"
#include "third_party/blink/public/platform/web_drag_operation.h"
#include "third_party/blink/public/platform/web_input_event.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/range/range.h"

namespace blink {
class WebMouseEvent;
class WebMouseWheelEvent;
class WebGestureEvent;
}

namespace gfx {
class Point;
class Size;
}

namespace rappor {
class Sample;
}

namespace content {

class BrowserAccessibilityManager;
class RenderWidgetHostImpl;
class RenderWidgetHostInputEventRouter;
class RenderViewHostDelegateView;
class TextInputManager;
class WebContents;
enum class KeyboardEventProcessingResult;
struct ScreenInfo;
struct NativeWebKeyboardEvent;

//
// RenderWidgetHostDelegate
//
//  An interface implemented by an object interested in knowing about the state
//  of the RenderWidgetHost.
class CONTENT_EXPORT RenderWidgetHostDelegate {
 public:
  // Functions for controlling the browser top controls slide behavior with page
  // gesture scrolling.
  virtual void SetTopControlsShownRatio(
      RenderWidgetHostImpl* render_widget_host,
      float ratio) {}
  virtual bool DoBrowserControlsShrinkRendererSize() const;
  virtual int GetTopControlsHeight() const;
  virtual void SetTopControlsGestureScrollInProgress(bool in_progress) {}

  // The RenderWidgetHost has just been created.
  virtual void RenderWidgetCreated(RenderWidgetHostImpl* render_widget_host) {}

  // The RenderWidgetHost is going to be deleted.
  virtual void RenderWidgetDeleted(RenderWidgetHostImpl* render_widget_host) {}

  // The RenderWidgetHost got the focus.
  virtual void RenderWidgetGotFocus(RenderWidgetHostImpl* render_widget_host) {}

  // If a main frame navigation is in progress, this will return the zoom level
  // for the pending page. Otherwise, this returns the zoom level for the
  // current page. Note that subframe navigations do not affect the zoom level,
  // which is tracked at the level of the page.
  virtual double GetPendingPageZoomLevel() const;

  // The RenderWidgetHost lost the focus.
  virtual void RenderWidgetLostFocus(
      RenderWidgetHostImpl* render_widget_host) {}

  // The RenderWidget was resized.
  virtual void RenderWidgetWasResized(RenderWidgetHostImpl* render_widget_host,
                                      const ScreenInfo& screen_info,
                                      bool width_changed) {}

  // The contents auto-resized and the container should match it.
  virtual void ResizeDueToAutoResize(RenderWidgetHostImpl* render_widget_host,
                                     const gfx::Size& new_size) {}

  // Callback to give the browser a chance to handle the specified keyboard
  // event before sending it to the renderer. See enum for details on return
  // value.
  virtual KeyboardEventProcessingResult PreHandleKeyboardEvent(
      const NativeWebKeyboardEvent& event);

  // Callback to give the browser a chance to handle the specified mouse
  // event before sending it to the renderer.
  // Returns true if the |event| was handled.
  // TODO(carlosil, nasko): remove once committed interstitial pages are
  // fully implemented.
  virtual bool PreHandleMouseEvent(const blink::WebMouseEvent& event);

  // Callback to inform the browser that the renderer did not process the
  // specified events. This gives an opportunity to the browser to process the
  // event (used for keyboard shortcuts).
  virtual void HandleKeyboardEvent(const NativeWebKeyboardEvent& event) {}

  // Callback to inform the browser that the renderer did not process the
  // specified mouse wheel event.  Returns true if the browser has handled
  // the event itself.
  virtual bool HandleWheelEvent(const blink::WebMouseWheelEvent& event);

  // Notification that an input event from the user was dispatched to the
  // widget.
  virtual void DidReceiveInputEvent(RenderWidgetHostImpl* render_widget_host,
                                    const blink::WebInputEvent::Type type) {}

  // Asks whether the page is in a state of ignoring input events.
  virtual bool ShouldIgnoreInputEvents();

  // Callback to give the browser a chance to handle the specified gesture
  // event before sending it to the renderer.
  // Returns true if the |event| was handled.
  virtual bool PreHandleGestureEvent(const blink::WebGestureEvent& event);

  // Notifies that screen rects were sent to renderer process.
  virtual void DidSendScreenRects(RenderWidgetHostImpl* rwh) {}

  // Get the root BrowserAccessibilityManager for this frame tree.
  virtual BrowserAccessibilityManager* GetRootBrowserAccessibilityManager();

  // Get the root BrowserAccessibilityManager for this frame tree,
  // or create it if it doesn't exist.
  virtual BrowserAccessibilityManager*
      GetOrCreateRootBrowserAccessibilityManager();

  // Send OS Cut/Copy/Paste actions to the focused frame.
  virtual void ExecuteEditCommand(
      const std::string& command,
      const base::Optional<base::string16>& value) = 0;
  virtual void Cut() = 0;
  virtual void Copy() = 0;
  virtual void Paste() = 0;
  virtual void SelectAll() = 0;

  // Requests the renderer to move the selection extent to a new position.
  virtual void MoveRangeSelectionExtent(const gfx::Point& extent) {}

  // Requests the renderer to select the region between two points in the
  // currently focused frame.
  virtual void SelectRange(const gfx::Point& base, const gfx::Point& extent) {}

#if defined(OS_MACOSX)
  virtual void DidChangeTextSelection(const base::string16& text,
                                      const gfx::Range& range,
                                      size_t offset) {}
#endif

  // Request the renderer to Move the caret to the new position.
  virtual void MoveCaret(const gfx::Point& extent) {}

  virtual RenderWidgetHostInputEventRouter* GetInputEventRouter();

  // Send page-level focus state to all SiteInstances involved in rendering the
  // current FrameTree, not including the main frame's SiteInstance.
  virtual void ReplicatePageFocus(bool is_focused) {}

  // Get the focused RenderWidgetHost associated with |receiving_widget|. A
  // RenderWidgetHostView, upon receiving a keyboard event, will pass its
  // RenderWidgetHost to this function to determine who should ultimately
  // consume the event.  This facilitates keyboard event routing with
  // out-of-process iframes, where multiple RenderWidgetHosts may be involved
  // in rendering a page, yet keyboard events all arrive at the main frame's
  // RenderWidgetHostView.  When a main frame's RenderWidgetHost is passed in,
  // the function returns the focused frame that should consume keyboard
  // events. In all other cases, the function returns back |receiving_widget|.
  virtual RenderWidgetHostImpl* GetFocusedRenderWidgetHost(
      RenderWidgetHostImpl* receiving_widget);

  // Notification that the renderer has become unresponsive. The
  // delegate can use this notification to show a warning to the user.
  // See also WebContentsDelegate::RendererUnresponsive.
  virtual void RendererUnresponsive(
      RenderWidgetHostImpl* render_widget_host,
      base::RepeatingClosure hang_monitor_restarter) {}

  // Notification that a previously unresponsive renderer has become
  // responsive again. The delegate can use this notification to end the
  // warning shown to the user.
  virtual void RendererResponsive(RenderWidgetHostImpl* render_widget_host) {}

  // Requests to lock the mouse. Once the request is approved or rejected,
  // GotResponseToLockMouseRequest() will be called on the requesting render
  // widget host. |privileged| means that the request is always granted, used
  // for Pepper Flash.
  virtual void RequestToLockMouse(RenderWidgetHostImpl* render_widget_host,
                                  bool user_gesture,
                                  bool last_unlocked_by_target,
                                  bool privileged) {}

  // Returns whether the associated tab is in fullscreen mode.
  virtual bool IsFullscreenForCurrentTab() const;

  // Returns the display mode for the view.
  virtual blink::WebDisplayMode GetDisplayMode(
      RenderWidgetHostImpl* render_widget_host) const;

  // Notification that the widget has lost capture.
  virtual void LostCapture(RenderWidgetHostImpl* render_widget_host) {}

  // Notification that the widget has lost the mouse lock.
  virtual void LostMouseLock(RenderWidgetHostImpl* render_widget_host) {}

  // Returns true if |render_widget_host| holds the mouse lock.
  virtual bool HasMouseLock(RenderWidgetHostImpl* render_widget_host);

  // Returns the widget that holds the mouse lock or nullptr if the mouse isn't
  // locked.
  virtual RenderWidgetHostImpl* GetMouseLockWidget();

  // Requests to lock the keyboard. Once the request is approved or rejected,
  // GotResponseToKeyboardLockRequest() will be called on the requesting render
  // widget host.
  virtual bool RequestKeyboardLock(RenderWidgetHostImpl* render_widget_host,
                                   bool esc_key_locked);

  // Cancels a previous keyboard lock request.
  virtual void CancelKeyboardLock(RenderWidgetHostImpl* render_widget_host) {}

  // Returns the widget that holds the keyboard lock or nullptr if not locked.
  virtual RenderWidgetHostImpl* GetKeyboardLockWidget();

  // Called when the visibility of the RenderFrameProxyHost in outer
  // WebContents changes. This method is only called on an inner WebContents and
  // will eventually notify all the RenderWidgetHostViews belonging to that
  // WebContents.
  virtual void OnRenderFrameProxyVisibilityChanged(bool visible) {}

  // Update the renderer's cache of the screen rect of the view and window.
  virtual void SendScreenRects() {}

  // Returns the TextInputManager tracking text input state.
  virtual TextInputManager* GetTextInputManager();

  // Returns true if this RenderWidgetHost should remain hidden. This is used by
  // the RenderWidgetHost to ask the delegate if it can be shown in the event of
  // something other than the WebContents attempting to enable visibility of
  // this RenderWidgetHost.
  virtual bool IsHidden();

  // Returns the associated RenderViewHostDelegateView*, if possible.
  virtual RenderViewHostDelegateView* GetDelegateView();

  // Returns the current Flash fullscreen RenderWidgetHostImpl if any. This is
  // not intended for use with other types of fullscreen, such as HTML
  // fullscreen, and will return nullptr for those cases.
  virtual RenderWidgetHostImpl* GetFullscreenRenderWidgetHost() const;

  // Allow the delegate to handle the cursor update. Returns true if handled.
  virtual bool OnUpdateDragCursor();

  // Returns true if the provided RenderWidgetHostImpl matches the current
  // RenderWidgetHost on the main frame, and false otherwise.
  virtual bool IsWidgetForMainFrame(RenderWidgetHostImpl*);

  // Inner WebContents Helpers -------------------------------------------------
  //
  // These functions are helpers in managing a hierharchy of WebContents
  // involved in rendering inner WebContents.

  // Get the RenderWidgetHost that should receive page level focus events. This
  // will be the widget that is rendering the main frame of the currently
  // focused WebContents.
  virtual RenderWidgetHostImpl* GetRenderWidgetHostWithPageFocus();

  // In cases with multiple RenderWidgetHosts involved in rendering a page, only
  // one widget should be focused and active. This ensures that
  // |render_widget_host| is focused and that its owning WebContents is also
  // the focused WebContents.
  virtual void FocusOwningWebContents(
      RenderWidgetHostImpl* render_widget_host) {}

  // Augment a Rappor sample with eTLD+1 context. The caller is still
  // responsible for logging the sample to the RapporService. Returns false
  // if the eTLD+1 is not known for |render_widget_host|.
  virtual bool AddDomainInfoToRapporSample(rappor::Sample* sample);

  // Get the UKM source ID for current content. This is used for providing
  // data about the content to the URL-keyed metrics service.
  // Note: This is also exposed by the RenderFrameHostDelegate.
  virtual ukm::SourceId GetUkmSourceIdForLastCommittedSource() const;

  // Notifies the delegate that a focused editable element has been touched
  // inside this RenderWidgetHost. If |editable| is true then the focused
  // element accepts text input.
  virtual void FocusedNodeTouched(bool editable) {}

  // Return this object cast to a WebContents, if it is one. If the object is
  // not a WebContents, returns nullptr.
  virtual WebContents* GetAsWebContents();

  // Gets the size set by a top-level frame with auto-resize enabled.
  virtual gfx::Size GetAutoResizeSize();

  // Reset the auto-size value, to indicate that auto-size is no longer active.
  virtual void ResetAutoResizeSize() {}

  // Returns true if there is context menu shown on page.
  virtual bool IsShowingContextMenuOnPage() const;

 protected:
  virtual ~RenderWidgetHostDelegate() {}
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_DELEGATE_H_
