// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_VIEW_H_
#define CHROME_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_VIEW_H_
#pragma once

#if defined(OS_MACOSX)
#include <OpenGL/OpenGL.h>
#endif

#include <string>
#include <vector>

#include "app/surface/transport_dib.h"
#include "base/process_util.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebPopupType.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebTextInputType.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/rect.h"

namespace gfx {
class Rect;
class Size;
}
namespace IPC {
class Message;
}

class BackingStore;
class RenderProcessHost;
class RenderWidgetHost;
class WebCursor;
struct NativeWebKeyboardEvent;
struct ViewHostMsg_AccessibilityNotification_Params;

namespace webkit_glue {
struct WebAccessibility;
}

namespace webkit {
namespace npapi {
struct WebPluginGeometry;
}
}

// RenderWidgetHostView is an interface implemented by an object that acts as
// the "View" portion of a RenderWidgetHost. The RenderWidgetHost and its
// associated RenderProcessHost own the "Model" in this case which is the
// child renderer process. The View is responsible for receiving events from
// the surrounding environment and passing them to the RenderWidgetHost, and
// for actually displaying the content of the RenderWidgetHost when it
// changes.
class RenderWidgetHostView {
 public:
  virtual ~RenderWidgetHostView();

  // Platform-specific creator. Use this to construct new RenderWidgetHostViews
  // rather than using RenderWidgetHostViewWin & friends.
  //
  // This function must NOT size it, because the RenderView in the renderer
  // wounldn't have been created yet. The widget would set its "waiting for
  // resize ack" flag, and the ack would never come becasue no RenderView
  // received it.
  //
  // The RenderWidgetHost must already be created (because we can't know if it's
  // going to be a regular RenderWidgetHost or a RenderViewHost (a subclass).
  static RenderWidgetHostView* CreateViewForWidget(RenderWidgetHost* widget);

  // Retrieves the RenderWidgetHostView corresponding to the specified
  // |native_view|, or NULL if there is no such instance.
  static RenderWidgetHostView* GetRenderWidgetHostViewFromNativeView(
      gfx::NativeView native_view);

  // Perform all the initialization steps necessary for this object to represent
  // a popup (such as a <select> dropdown), then shows the popup at |pos|.
  virtual void InitAsPopup(RenderWidgetHostView* parent_host_view,
                           const gfx::Rect& pos) = 0;

  // Perform all the initialization steps necessary for this object to represent
  // a full screen window.
  virtual void InitAsFullscreen() = 0;

  // Returns the associated RenderWidgetHost.
  virtual RenderWidgetHost* GetRenderWidgetHost() const = 0;

  // Notifies the View that it has become visible.
  virtual void DidBecomeSelected() = 0;

  // Notifies the View that it has been hidden.
  virtual void WasHidden() = 0;

  // Tells the View to size itself to the specified size.
  virtual void SetSize(const gfx::Size& size) = 0;

  // Retrieves the native view used to contain plugins and identify the
  // renderer in IPC messages.
  virtual gfx::NativeView GetNativeView() = 0;

  // Moves all plugin windows as described in the given list.
  virtual void MovePluginWindows(
      const std::vector<webkit::npapi::WebPluginGeometry>& moves) = 0;

  // Actually set/take focus to/from the associated View component.
  virtual void Focus() = 0;
  virtual void Blur() = 0;

  // Returns true if the View currently has the focus.
  virtual bool HasFocus() = 0;

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
  virtual void ImeUpdateTextInputState(WebKit::WebTextInputType type,
                                       const gfx::Rect& caret_rect) = 0;

  // Cancel the ongoing composition of the input method attached to the view.
  virtual void ImeCancelComposition() = 0;

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

  // Notifies the View that the renderer will be delete soon.
  virtual void WillDestroyRenderWidget(RenderWidgetHost* rwh) = 0;

  // Tells the View to destroy itself.
  virtual void Destroy() = 0;

  // Tells the View that the tooltip text for the current mouse position over
  // the page has changed.
  virtual void SetTooltipText(const std::wstring& tooltip_text) = 0;

  // Notifies the View that the renderer text selection has changed.
  virtual void SelectionChanged(const std::string& text) {}

  // Tells the View whether the context menu is showing. This is used on Linux
  // to suppress updates to webkit focus for the duration of the show.
  virtual void ShowingContextMenu(bool showing) {}

  // Allocate a backing store for this view
  virtual BackingStore* AllocBackingStore(const gfx::Size& size) = 0;

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

  // Get the view's window's position on the screen.
  virtual gfx::Rect GetRootWindowRect() = 0;

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
  // |window| and |surface_id| indicate which accelerated surface's
  // buffers swapped. |renderer_id|, |route_id| and
  // |swap_buffers_count| are used to formulate a reply to the GPU
  // process to prevent it from getting too far ahead. They may all be
  // zero, in which case no flow control is enforced; this case is
  // currently used for accelerated plugins.
  virtual void AcceleratedSurfaceBuffersSwapped(
      gfx::PluginWindowHandle window,
      uint64 surface_id,
      int renderer_id,
      int32 route_id,
      uint64 swap_buffers_count) = 0;
  virtual void GpuRenderingStateDidChange() = 0;
#endif

#if defined(TOOLKIT_USES_GTK)
  virtual void CreatePluginContainer(gfx::PluginWindowHandle id) = 0;
  virtual void DestroyPluginContainer(gfx::PluginWindowHandle id) = 0;
  virtual void AcceleratedCompositingActivated(bool activated) = 0;
#endif

#if defined(OS_WIN)
  virtual gfx::PluginWindowHandle GetCompositorHostWindow() = 0;
  virtual void WillWmDestroy() = 0;
  virtual void ShowCompositorHostWindow(bool show) = 0;
#endif

  // Toggles visual muting of the render view area. This is on when a
  // constrained window is showing, for example. |color| is the shade of
  // the overlay that covers the render view. If |animate| is true, the overlay
  // gradually fades in; otherwise it takes effect immediately. To remove the
  // fade effect, pass a NULL value for |color|. In this case, |animate| is
  // ignored.
  virtual void SetVisuallyDeemphasized(const SkColor* color, bool animate) = 0;

  void set_popup_type(WebKit::WebPopupType popup_type) {
    popup_type_ = popup_type;
  }
  WebKit::WebPopupType popup_type() const { return popup_type_; }

  // Subclasses should override this method to do what is appropriate to set
  // the custom background for their platform.
  virtual void SetBackground(const SkBitmap& background);
  const SkBitmap& background() const { return background_; }

  // Returns true if the native view, |native_view|, is contained within in the
  // widget associated with this RenderWidgetHostView.
  virtual bool ContainsNativeView(gfx::NativeView native_view) const = 0;

  virtual void UpdateAccessibilityTree(
      const webkit_glue::WebAccessibility& tree) { }
  virtual void OnAccessibilityNotifications(
      const std::vector<ViewHostMsg_AccessibilityNotification_Params>& params) {
  }

  gfx::Rect reserved_contents_rect() const {
    return reserved_rect_;
  }
  void set_reserved_contents_rect(const gfx::Rect& reserved_rect) {
    reserved_rect_ = reserved_rect;
  }

 protected:
  // Interface class only, do not construct.
  RenderWidgetHostView() : popup_type_(WebKit::WebPopupTypeNone) {}

  // Whether this view is a popup and what kind of popup it is (select,
  // autofill...).
  WebKit::WebPopupType popup_type_;

  // A custom background to paint behind the web content. This will be tiled
  // horizontally. Can be null, in which case we fall back to painting white.
  SkBitmap background_;

  // The current reserved area in view coordinates where contents should not be
  // rendered to draw the resize corner, sidebar mini tabs etc.
  gfx::Rect reserved_rect_;

 private:
  DISALLOW_COPY_AND_ASSIGN(RenderWidgetHostView);
};

#endif  // CHROME_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_VIEW_H_
