// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_VIEW_BASE_H_
#define CONTENT_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_VIEW_BASE_H_

#if defined(OS_MACOSX)
#include <OpenGL/OpenGL.h>
#endif

#if defined(TOOLKIT_GTK)
#include <gdk/gdk.h>
#endif

#include <string>
#include <vector>

#include "base/memory/scoped_ptr.h"
#include "base/callback_forward.h"
#include "base/timer/timer.h"
#include "content/common/content_export.h"
#include "content/port/browser/render_widget_host_view_port.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/range/range.h"
#include "ui/gfx/rect.h"

namespace content {

class RenderWidgetHostImpl;

// Basic implementation shared by concrete RenderWidgetHostView
// subclasses.
//
// Note that nothing should use this class, except concrete subclasses
// that are deriving from it, and code that is written specifically to
// use one of these concrete subclasses (i.e. platform-specific code).
//
// To enable embedders that add ports, everything else in content/
// should use the RenderWidgetHostViewPort interface.
//
// RenderWidgetHostView class hierarchy described in render_widget_host_view.h.
class CONTENT_EXPORT RenderWidgetHostViewBase
    : public RenderWidgetHostViewPort {
 public:
  virtual ~RenderWidgetHostViewBase();

  // RenderWidgetHostViewPort implementation.
  virtual bool OnMessageReceived(const IPC::Message& msg) OVERRIDE;
  virtual void SelectionChanged(const base::string16& text,
                                size_t offset,
                                const gfx::Range& range) OVERRIDE;
  virtual void SetBackground(const SkBitmap& background) OVERRIDE;
  virtual const SkBitmap& GetBackground() OVERRIDE;
  virtual gfx::Size GetPhysicalBackingSize() const OVERRIDE;
  virtual float GetOverdrawBottomHeight() const OVERRIDE;
  virtual bool IsShowingContextMenu() const OVERRIDE;
  virtual void SetShowingContextMenu(bool showing_menu) OVERRIDE;
  virtual base::string16 GetSelectedText() const OVERRIDE;
  virtual bool IsMouseLocked() OVERRIDE;
  virtual void HandledWheelEvent(const blink::WebMouseWheelEvent& event,
                                 bool consumed) OVERRIDE;
  virtual InputEventAckState FilterInputEvent(
      const blink::WebInputEvent& input_event) OVERRIDE;
  virtual void OnSetNeedsFlushInput() OVERRIDE;
  virtual void OnDidFlushInput() OVERRIDE;
  virtual void GestureEventAck(const blink::WebGestureEvent& event,
                               InputEventAckState ack_result) OVERRIDE;
  virtual void SetPopupType(blink::WebPopupType popup_type) OVERRIDE;
  virtual blink::WebPopupType GetPopupType() OVERRIDE;
  virtual BrowserAccessibilityManager*
      GetBrowserAccessibilityManager() const OVERRIDE;
  virtual void CreateBrowserAccessibilityManagerIfNeeded() OVERRIDE;
  virtual void ProcessAckedTouchEvent(const TouchEventWithLatencyInfo& touch,
                                      InputEventAckState ack_result) OVERRIDE;
  virtual scoped_ptr<SyntheticGestureTarget> CreateSyntheticGestureTarget()
      OVERRIDE;
  virtual void FocusedNodeChanged(bool is_editable_node) OVERRIDE;
  virtual bool CanSubscribeFrame() const OVERRIDE;
  virtual void BeginFrameSubscription(
      scoped_ptr<RenderWidgetHostViewFrameSubscriber> subscriber) OVERRIDE;
  virtual void EndFrameSubscription() OVERRIDE;
  virtual void OnSwapCompositorFrame(
      uint32 output_surface_id,
      scoped_ptr<cc::CompositorFrame> frame) OVERRIDE {}
  virtual void ResizeCompositingSurface(const gfx::Size&) OVERRIDE {}
  virtual void OnOverscrolled(gfx::Vector2dF accumulated_overscroll,
                              gfx::Vector2dF current_fling_velocity) OVERRIDE;
  virtual void DidStopFlinging() OVERRIDE {}
  virtual uint32 RendererFrameNumber() OVERRIDE;
  virtual void DidReceiveRendererFrame() OVERRIDE;
  virtual void LockCompositingSurface() OVERRIDE;
  virtual void UnlockCompositingSurface() OVERRIDE;

  virtual SkBitmap::Config PreferredReadbackFormat() OVERRIDE;

  void SetBrowserAccessibilityManager(BrowserAccessibilityManager* manager);

  // Notification that a resize or move session ended on the native widget.
  void UpdateScreenInfo(gfx::NativeView view);

  // Tells if the display property (work area/scale factor) has
  // changed since the last time.
  bool HasDisplayPropertyChanged(gfx::NativeView view);

#if defined(OS_WIN)
  // The callback that DetachPluginsHelper calls for each child window. Call
  // this directly if you want to do custom filtering on plugin windows first.
  static void DetachPluginWindowsCallback(HWND window);
#endif

 protected:
  // Interface class only, do not construct.
  RenderWidgetHostViewBase();

#if defined(OS_WIN)
  // Shared implementation of MovePluginWindows for use by win and aura/wina.
  static void MovePluginWindowsHelper(
      HWND parent,
      const std::vector<WebPluginGeometry>& moves);

  static void PaintPluginWindowsHelper(
      HWND parent,
      const gfx::Rect& damaged_screen_rect);

  // Needs to be called before the HWND backing the view goes away to avoid
  // crashes in Windowed plugins.
  static void DetachPluginsHelper(HWND parent);
#endif

  // Whether this view is a popup and what kind of popup it is (select,
  // autofill...).
  blink::WebPopupType popup_type_;

  // A custom background to paint behind the web content. This will be tiled
  // horizontally. Can be null, in which case we fall back to painting white.
  SkBitmap background_;

  // While the mouse is locked, the cursor is hidden from the user. Mouse events
  // are still generated. However, the position they report is the last known
  // mouse position just as mouse lock was entered; the movement they report
  // indicates what the change in position of the mouse would be had it not been
  // locked.
  bool mouse_locked_;

  // Whether we are showing a context menu.
  bool showing_context_menu_;

  // A buffer containing the text inside and around the current selection range.
  base::string16 selection_text_;

  // The offset of the text stored in |selection_text_| relative to the start of
  // the web page.
  size_t selection_text_offset_;

  // The current selection range relative to the start of the web page.
  gfx::Range selection_range_;

protected:
  // The scale factor of the display the renderer is currently on.
  float current_device_scale_factor_;

  // Whether pinch-to-zoom should be enabled and pinch events forwarded to the
  // renderer.
  bool pinch_zoom_enabled_;

 private:
  void FlushInput();

  // Manager of the tree representation of the WebKit render tree.
  scoped_ptr<BrowserAccessibilityManager> browser_accessibility_manager_;

  gfx::Rect current_display_area_;

  uint32 renderer_frame_number_;

  base::OneShotTimer<RenderWidgetHostViewBase> flush_input_timer_;

  DISALLOW_COPY_AND_ASSIGN(RenderWidgetHostViewBase);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_VIEW_BASE_H_
