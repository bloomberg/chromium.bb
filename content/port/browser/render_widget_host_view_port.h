// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PORT_BROWSER_RENDER_WIDGET_HOST_VIEW_PORT_H_
#define CONTENT_PORT_BROWSER_RENDER_WIDGET_HOST_VIEW_PORT_H_
#pragma once

#include "base/callback.h"
#include "base/process_util.h"
#include "base/string16.h"
#include "content/common/content_export.h"
#include "content/public/browser/render_widget_host_view.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebPopupType.h"
#include "ui/base/ime/text_input_type.h"
#include "ui/base/range/range.h"
#include "ui/surface/transport_dib.h"

class BackingStore;
class WebCursor;

struct AccessibilityHostMsg_NotificationParams;
struct GpuHostMsg_AcceleratedSurfaceBuffersSwapped_Params;
struct GpuHostMsg_AcceleratedSurfacePostSubBuffer_Params;
struct NativeWebKeyboardEvent;

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

namespace content {

// This is the larger RenderWidgetHostView interface exposed only
// within content/ and to embedders looking to port to new platforms.
// RenderWidgetHostView class hierarchy described in render_widget_host_view.h.
class CONTENT_EXPORT RenderWidgetHostViewPort : public RenderWidgetHostView {
 public:
  virtual ~RenderWidgetHostViewPort() {}

  // Does the cast for you.
  static RenderWidgetHostViewPort* FromRWHV(RenderWidgetHostView* rwhv);

  // Like RenderWidgetHostView::CreateViewForWidget, with cast.
  static RenderWidgetHostViewPort* CreateViewForWidget(
      content::RenderWidgetHost* widget);

  // Perform all the initialization steps necessary for this object to represent
  // a popup (such as a <select> dropdown), then shows the popup at |pos|.
  virtual void InitAsPopup(RenderWidgetHostView* parent_host_view,
                           const gfx::Rect& pos) = 0;

  // Perform all the initialization steps necessary for this object to represent
  // a full screen window.
  // |reference_host_view| is the view associated with the creating page that
  // helps to position the full screen widget on the correct monitor.
  virtual void InitAsFullscreen(
      RenderWidgetHostView* reference_host_view) = 0;

  // Notifies the View that it has become visible.
  virtual void DidBecomeSelected() = 0;

  // Notifies the View that it has been hidden.
  virtual void WasHidden() = 0;

  // Moves all plugin windows as described in the given list.
  virtual void MovePluginWindows(
      const std::vector<webkit::npapi::WebPluginGeometry>& moves) = 0;

  // Take focus from the associated View component.
  virtual void Blur() = 0;

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
  virtual void SelectionChanged(const string16& text,
                                size_t offset,
                                const ui::Range& range) = 0;

  // Notifies the View that the renderer selection bounds has changed.
  // |start_rect| and |end_rect| are the bounds end of the selection in the
  // coordinate system of the render view.
  virtual void SelectionBoundsChanged(const gfx::Rect& start_rect,
                                      const gfx::Rect& end_rect) {}

  // Allocate a backing store for this view.
  virtual BackingStore* AllocBackingStore(const gfx::Size& size) = 0;

  // Copies the contents of the compositing surface into the given
  // (uninitialized) PlatformCanvas if any.
  // |callback| is invoked with true on success, false otherwise. |output| can
  // be initialized even on failure.
  // NOTE: |callback| is called asynchronously on Aura and synchronously on the
  // other platforms.
  virtual void CopyFromCompositingSurface(
      const gfx::Size& size,
      skia::PlatformCanvas* output,
      base::Callback<void(bool)> callback) = 0;

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
  // Similar to above, except |params.(x|y|width|height)| define the region
  // of the surface that changed.
  virtual void AcceleratedSurfacePostSubBuffer(
      const GpuHostMsg_AcceleratedSurfacePostSubBuffer_Params& params,
      int gpu_host_id) = 0;

  // Release the accelerated surface temporarily. It will be recreated on the
  // next swap buffers or post sub buffer.
  virtual void AcceleratedSurfaceSuspend() = 0;

  // Return true if the view has an accelerated surface that contains the last
  // presented frame for the view. If |desired_size| is non-empty, true is
  // returned only if the accelerated surface size matches.
  virtual bool HasAcceleratedSurface(const gfx::Size& desired_size) = 0;

#if defined(OS_MACOSX)
  // Retrieve the bounds of the view, in cocoa view coordinates.
  // If the UI scale factor is 2, |GetViewBounds()| will return a size of e.g.
  // (400, 300) in pixels, while this method will return (200, 150).
  // Even though this returns an gfx::Rect, the result is NOT IN PIXELS.
  virtual gfx::Rect GetViewCocoaBounds() const = 0;

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

#if defined(USE_AURA)
  virtual void AcceleratedSurfaceNew(
      int32 width,
      int32 height,
      uint64* surface_id,
      TransportDIB::Handle* surface_handle) = 0;
  virtual void AcceleratedSurfaceRelease(uint64 surface_id) = 0;
#endif

#if defined(TOOLKIT_GTK)
  virtual void CreatePluginContainer(gfx::PluginWindowHandle id) = 0;
  virtual void DestroyPluginContainer(gfx::PluginWindowHandle id) = 0;
#endif  // defined(TOOLKIT_GTK)

#if defined(OS_WIN) && !defined(USE_AURA)
  virtual void WillWmDestroy() = 0;
#endif

#if defined(OS_POSIX) || defined(USE_AURA)
  static void GetDefaultScreenInfo(
      WebKit::WebScreenInfo* results);
  virtual void GetScreenInfo(WebKit::WebScreenInfo* results) = 0;
  virtual gfx::Rect GetRootWindowBounds() = 0;
#endif

  virtual gfx::GLSurfaceHandle GetCompositingSurface() = 0;

  // Because the associated remote WebKit instance can asynchronously
  // prevent-default on a dispatched touch event, the touch events are queued in
  // the GestureRecognizer until invocation of ProcessTouchAck releases it to be
  // processed (when |processed| is false) or ignored (when |processed| is true)
  virtual void ProcessTouchAck(WebKit::WebInputEvent::Type type,
                               bool processed) = 0;

  virtual void SetHasHorizontalScrollbar(bool has_horizontal_scrollbar) = 0;
  virtual void SetScrollOffsetPinning(
      bool is_pinned_to_left, bool is_pinned_to_right) = 0;

  // Called when a mousewheel event was not processed by the renderer.
  virtual void UnhandledWheelEvent(const WebKit::WebMouseWheelEvent& event) = 0;

  virtual void SetPopupType(WebKit::WebPopupType popup_type) = 0;
  virtual WebKit::WebPopupType GetPopupType() = 0;

  virtual BrowserAccessibilityManager*
      GetBrowserAccessibilityManager() const = 0;
  virtual void OnAccessibilityNotifications(
      const std::vector<AccessibilityHostMsg_NotificationParams>& params) {
  }
};

}  // namespace content

#endif  // CONTENT_PORT_BROWSER_RENDER_WIDGET_HOST_VIEW_PORT_H_
