// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_VIEW_H_
#define CONTENT_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_VIEW_H_
#pragma once

#if defined(OS_MACOSX)
#include <OpenGL/OpenGL.h>
#endif

#include <string>
#include <vector>

#include "base/process_util.h"
#include "base/callback.h"
#include "content/common/content_export.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebInputEvent.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebPopupType.h"
#include "ui/base/ime/text_input_type.h"
#include "ui/base/range/range.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/surface/transport_dib.h"

struct GpuHostMsg_AcceleratedSurfaceBuffersSwapped_Params;

class BackingStore;
class RenderWidgetHost;
class WebCursor;
struct NativeWebKeyboardEvent;
struct ViewHostMsg_AccessibilityNotification_Params;

namespace content {
class RenderProcessHost;
}

namespace gfx {
class Rect;
class Size;
}

namespace webkit {
namespace npapi {
struct WebPluginGeometry;
}
}

#if defined(OS_POSIX) || defined(USE_AURA)
namespace WebKit {
struct WebScreenInfo;
}
#endif

// RenderWidgetHostView is an interface implemented by an object that acts as
// the "View" portion of a RenderWidgetHost. The RenderWidgetHost and its
// associated RenderProcessHost own the "Model" in this case which is the
// child renderer process. The View is responsible for receiving events from
// the surrounding environment and passing them to the RenderWidgetHost, and
// for actually displaying the content of the RenderWidgetHost when it
// changes.
class RenderWidgetHostView {
 public:
  CONTENT_EXPORT virtual ~RenderWidgetHostView();

  // Perform all the initialization steps necessary for this object to represent
  // a popup (such as a <select> dropdown), then shows the popup at |pos|.
  virtual void InitAsPopup(RenderWidgetHostView* parent_host_view,
                           const gfx::Rect& pos) = 0;

  // Perform all the initialization steps necessary for this object to represent
  // a full screen window.
  // |reference_host_view| is the view associated with the creating page that
  // helps to position the full screen widget on the correct monitor.
  virtual void InitAsFullscreen(RenderWidgetHostView* reference_host_view) = 0;

  // Returns the associated RenderWidgetHost.
  virtual RenderWidgetHost* GetRenderWidgetHost() const = 0;

  // Notifies the View that it has become visible.
  virtual void DidBecomeSelected() = 0;

  // Notifies the View that it has been hidden.
  virtual void WasHidden() = 0;

  // Tells the View to size itself to the specified size.
  virtual void SetSize(const gfx::Size& size) = 0;

  // Tells the View to size and move itself to the specified size and point in
  // screen space.
  virtual void SetBounds(const gfx::Rect& rect) = 0;

  // Retrieves the native view used to contain plugins and identify the
  // renderer in IPC messages.
  virtual gfx::NativeView GetNativeView() const = 0;
  virtual gfx::NativeViewId GetNativeViewId() const = 0;

  // Moves all plugin windows as described in the given list.
  virtual void MovePluginWindows(
      const std::vector<webkit::npapi::WebPluginGeometry>& moves) = 0;

  // Actually set/take focus to/from the associated View component.
  virtual void Focus() = 0;
  virtual void Blur() = 0;

  // Returns true if the View currently has the focus.
  virtual bool HasFocus() const = 0;

  // Shows/hides the view.  These must always be called together in pairs.
  // It is not legal to call Hide() multiple times in a row.
  virtual void Show() = 0;
  virtual void Hide() = 0;

  // Whether the view is showing.
  virtual bool IsShowing() = 0;

  // Retrieve the bounds of the View, in screen coordinates.
  virtual gfx::Rect GetViewBounds() const = 0;

  // Sets the cursor to the one associated with the specified cursor_type
  virtual void UpdateCursor(const WebCursor& cursor) = 0;

  // Indicates whether the page has finished loading.
  virtual void SetIsLoading(bool is_loading) = 0;

  // Updates the state of the input method attached to the view.
  virtual void TextInputStateChanged(ui::TextInputType type,
                                     bool can_compose_inline) = 0;

  // Cancel the ongoing composition of the input method attached to the view.
  virtual void ImeCancelComposition() = 0;

  // Updates the range of the marked text in an IME composition.
  virtual void ImeCompositionRangeChanged(const ui::Range& range) {}

  // Informs the view that a portion of the widget's backing store was scrolled
  // and/or painted.  The view should ensure this gets copied to the screen.
  //
  // If the scroll_rect is non-empty, then a portion of the widget's backing
  // store was scrolled by dx pixels horizontally and dy pixels vertically.
  // The exposed rect from the scroll operation is included in copy_rects.
  //
  // There are subtle performance implications here.  The RenderWidget gets sent
  // a paint ack after this returns, so if the view only ever invalidates in
  // response to this, then on Windows, where WM_PAINT has lower priority than
  // events which can cause renderer resizes/paint rect updates, e.g.
  // drag-resizing can starve painting; this function thus provides the view its
  // main chance to ensure it stays painted and not just invalidated.  On the
  // other hand, if this always blindly paints, then if we're already in the
  // midst of a paint on the callstack, we can double-paint unnecessarily.
  // (Worse, we might recursively call RenderWidgetHost::GetBackingStore().)
  // Thus implementers should generally paint as much of |rect| as possible
  // synchronously with as little overpainting as possible.
  virtual void DidUpdateBackingStore(
      const gfx::Rect& scroll_rect, int scroll_dx, int scroll_dy,
      const std::vector<gfx::Rect>& copy_rects) = 0;

  // Notifies the View that the renderer has ceased to exist.
  virtual void RenderViewGone(base::TerminationStatus status,
                              int error_code) = 0;

  // Tells the View to destroy itself.
  virtual void Destroy() = 0;

  // Tells the View that the tooltip text for the current mouse position over
  // the page has changed.
  virtual void SetTooltipText(const string16& tooltip_text) = 0;

  // Notifies the View that the renderer text selection has changed.
  CONTENT_EXPORT virtual void SelectionChanged(const string16& text,
                                               size_t offset,
                                               const ui::Range& range);

  // Notifies the View that the renderer selection bounds has changed.
  // |start_rect| and |end_rect| are the bounds end of the selection in the
  // coordinate system of the render view.
  virtual void SelectionBoundsChanged(const gfx::Rect& start_rect,
                                      const gfx::Rect& end_rect) {}

  // Tells the View whether the context menu is showing. This is used on Linux
  // to suppress updates to webkit focus for the duration of the show.
  virtual void ShowingContextMenu(bool showing) {}

  // Allocate a backing store for this view
  virtual BackingStore* AllocBackingStore(const gfx::Size& size) = 0;

  // Called when accelerated compositing state changes.
  virtual void OnAcceleratedCompositingStateChange() = 0;
  // |params.window| and |params.surface_id| indicate which accelerated
  // surface's buffers swapped. |params.renderer_id| and |params.route_id|
  // are used to formulate a reply to the GPU process to prevent it from getting
  // too far ahead. They may all be zero, in which case no flow control is
  // enforced; this case is currently used for accelerated plugins.
  virtual void AcceleratedSurfaceBuffersSwapped(
      const GpuHostMsg_AcceleratedSurfaceBuffersSwapped_Params& params,
      int gpu_host_id) = 0;

#if defined(OS_MACOSX)
  // Tells the view whether or not to accept first responder status.  If |flag|
  // is true, the view does not accept first responder status and instead
  // manually becomes first responder when it receives a mouse down event.  If
  // |flag| is false, the view participates in the key-view chain as normal.
  virtual void SetTakesFocusOnlyOnMouseDown(bool flag) = 0;

  // Retrieve the bounds of the view, in cocoa view coordinates.
  // If the UI scale factor is 2, |GetViewBounds()| will return a size of e.g.
  // (400, 300) in pixels, while this method will return (200, 150).
  // Even though this returns an gfx::Rect, the result is NOT IN PIXELS.
  virtual gfx::Rect GetViewCocoaBounds() const = 0;

  // Set the view's active state (i.e., tint state of controls).
  virtual void SetActive(bool active) = 0;

  // Notifies the view that its enclosing window has changed visibility
  // (minimized/unminimized, app hidden/unhidden, etc).
  // TODO(stuartmorgan): This is a temporary plugin-specific workaround for
  // <http://crbug.com/34266>. Once that is fixed, this (and the corresponding
  // message and renderer-side handling) can be removed in favor of using
  // WasHidden/DidBecomeSelected.
  virtual void SetWindowVisibility(bool visible) = 0;

  // Informs the view that its containing window's frame changed.
  virtual void WindowFrameChanged() = 0;

  // Informs the view that a plugin gained or lost focus.
  virtual void PluginFocusChanged(bool focused, int plugin_id) = 0;

  // Start plugin IME.
  virtual void StartPluginIme() = 0;

  // Does any event handling necessary for plugin IME; should be called after
  // the plugin has already had a chance to process the event. If plugin IME is
  // not enabled, this is a no-op, so it is always safe to call.
  // Returns true if the event was handled by IME.
  virtual bool PostProcessEventForPluginIme(
      const NativeWebKeyboardEvent& event) = 0;

  // Methods associated with GPU-accelerated plug-in instances.
  virtual gfx::PluginWindowHandle AllocateFakePluginWindowHandle(
      bool opaque, bool root) = 0;
  virtual void DestroyFakePluginWindowHandle(
      gfx::PluginWindowHandle window) = 0;
  virtual void AcceleratedSurfaceSetIOSurface(
      gfx::PluginWindowHandle window,
      int32 width,
      int32 height,
      uint64 io_surface_identifier) = 0;
  virtual void AcceleratedSurfaceSetTransportDIB(
      gfx::PluginWindowHandle window,
      int32 width,
      int32 height,
      TransportDIB::Handle transport_dib) = 0;
#endif

#if defined(UI_COMPOSITOR_IMAGE_TRANSPORT)
  virtual void AcceleratedSurfaceNew(
      int32 width,
      int32 height,
      uint64* surface_id,
      TransportDIB::Handle* surface_handle) = 0;
  virtual void AcceleratedSurfaceRelease(uint64 surface_id) = 0;
#endif

#if defined(TOOLKIT_USES_GTK)
  virtual void CreatePluginContainer(gfx::PluginWindowHandle id) = 0;
  virtual void DestroyPluginContainer(gfx::PluginWindowHandle id) = 0;
#endif

#if defined(OS_WIN) && !defined(USE_AURA)
  virtual void WillWmDestroy() = 0;
#endif

#if defined(OS_POSIX) || defined(USE_AURA)
  CONTENT_EXPORT static void GetDefaultScreenInfo(
      WebKit::WebScreenInfo* results);
  virtual void GetScreenInfo(WebKit::WebScreenInfo* results) = 0;
  virtual gfx::Rect GetRootWindowBounds() = 0;
#endif

  virtual gfx::PluginWindowHandle GetCompositingSurface() = 0;

  // Toggles visual muting of the render view area. This is on when a
  // constrained window is showing, for example. |color| is the shade of
  // the overlay that covers the render view. If |animate| is true, the overlay
  // gradually fades in; otherwise it takes effect immediately. To remove the
  // fade effect, pass a NULL value for |color|. In this case, |animate| is
  // ignored.
  virtual void SetVisuallyDeemphasized(const SkColor* color, bool animate) = 0;

  virtual void UnhandledWheelEvent(const WebKit::WebMouseWheelEvent& event) = 0;

  virtual void SetHasHorizontalScrollbar(bool has_horizontal_scrollbar) = 0;
  virtual void SetScrollOffsetPinning(
      bool is_pinned_to_left, bool is_pinned_to_right) = 0;

  // Return value indicates whether the mouse is locked successfully or not.
  virtual bool LockMouse() = 0;
  virtual void UnlockMouse() = 0;

  void set_popup_type(WebKit::WebPopupType popup_type) {
    popup_type_ = popup_type;
  }
  WebKit::WebPopupType popup_type() const { return popup_type_; }

  // Subclasses should override this method to do what is appropriate to set
  // the custom background for their platform.
  CONTENT_EXPORT virtual void SetBackground(const SkBitmap& background);
  const SkBitmap& background() const { return background_; }

  virtual void OnAccessibilityNotifications(
      const std::vector<ViewHostMsg_AccessibilityNotification_Params>& params) {
  }

  gfx::Rect reserved_contents_rect() const {
    return reserved_rect_;
  }
  void set_reserved_contents_rect(const gfx::Rect& reserved_rect) {
    reserved_rect_ = reserved_rect;
  }

  bool mouse_locked() const { return mouse_locked_; }

 protected:
  // Interface class only, do not construct.
  CONTENT_EXPORT RenderWidgetHostView();

  // Whether this view is a popup and what kind of popup it is (select,
  // autofill...).
  WebKit::WebPopupType popup_type_;

  // A custom background to paint behind the web content. This will be tiled
  // horizontally. Can be null, in which case we fall back to painting white.
  SkBitmap background_;

  // The current reserved area in view coordinates where contents should not be
  // rendered to draw the resize corner, sidebar mini tabs etc.
  gfx::Rect reserved_rect_;

  // While the mouse is locked, the cursor is hidden from the user. Mouse events
  // are still generated. However, the position they report is the last known
  // mouse position just as mouse lock was entered; the movement they report
  // indicates what the change in position of the mouse would be had it not been
  // locked.
  bool mouse_locked_;

  // A buffer containing the text inside and around the current selection range.
  string16 selection_text_;

  // The offset of the text stored in |selection_text_| relative to the start of
  // the web page.
  size_t selection_text_offset_;

  // The current selection range relative to the start of the web page.
  ui::Range selection_range_;

 private:
  DISALLOW_COPY_AND_ASSIGN(RenderWidgetHostView);
};

#endif  // CONTENT_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_VIEW_H_
