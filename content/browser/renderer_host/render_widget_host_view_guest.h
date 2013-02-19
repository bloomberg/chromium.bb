// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_VIEW_GUEST_H_
#define CONTENT_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_VIEW_GUEST_H_

#include <vector>

#include "base/memory/scoped_ptr.h"
#include "content/browser/renderer_host/render_widget_host_view_base.h"
#include "content/common/content_export.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/vector2d_f.h"
#include "webkit/glue/webcursor.h"

#if defined(TOOLKIT_GTK)
#include "webkit/plugins/npapi/gtk_plugin_container_manager.h"
#endif  // defined(TOOLKIT_GTK)

namespace content {
class RenderWidgetHost;
class RenderWidgetHostImpl;
class BrowserPluginGuest;
struct NativeWebKeyboardEvent;

// -----------------------------------------------------------------------------
// See comments in render_widget_host_view.h about this class and its members.
// This version is for the webview plugin which handles a lot of the
// functionality in a diffent place and isn't platform specific.
//
// Some elements that are platform specific will be deal with by delegating
// the relevant calls to the platform view.
// -----------------------------------------------------------------------------
class CONTENT_EXPORT RenderWidgetHostViewGuest
    : public RenderWidgetHostViewBase {
 public:
  RenderWidgetHostViewGuest(RenderWidgetHost* widget,
                            BrowserPluginGuest* guest,
                            bool enable_compositing,
                            RenderWidgetHostView* platform_view);
  virtual ~RenderWidgetHostViewGuest();

  // RenderWidgetHostView implementation.
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
#if defined(OS_WIN) && !defined(USE_AURA)
  virtual void SetClickthroughRegion(SkRegion* region) OVERRIDE;
#endif

  // RenderWidgetHostViewPort implementation.
  virtual void InitAsPopup(RenderWidgetHostView* parent_host_view,
                           const gfx::Rect& pos) OVERRIDE;
  virtual void InitAsFullscreen(
      RenderWidgetHostView* reference_host_view) OVERRIDE;
  virtual void WasShown() OVERRIDE;
  virtual void WasHidden() OVERRIDE;
  virtual void MovePluginWindows(
      const gfx::Vector2d& scroll_offset,
      const std::vector<webkit::npapi::WebPluginGeometry>& moves) OVERRIDE;
  virtual void Focus() OVERRIDE;
  virtual void Blur() OVERRIDE;
  virtual void UpdateCursor(const WebCursor& cursor) OVERRIDE;
  virtual void SetIsLoading(bool is_loading) OVERRIDE;
  virtual void TextInputStateChanged(
      const ViewHostMsg_TextInputState_Params& params) OVERRIDE;
  virtual void ImeCancelComposition() OVERRIDE;
  virtual void ImeCompositionRangeChanged(
      const ui::Range& range,
      const std::vector<gfx::Rect>& character_bounds) OVERRIDE;
  virtual void DidUpdateBackingStore(
      const gfx::Rect& scroll_rect,
      const gfx::Vector2d& scroll_delta,
      const std::vector<gfx::Rect>& copy_rects) OVERRIDE;
  virtual void RenderViewGone(base::TerminationStatus status,
                              int error_code) OVERRIDE;
  virtual void Destroy() OVERRIDE;
  virtual void WillDestroyRenderWidget(RenderWidgetHost* rwh) {}
  virtual void SetTooltipText(const string16& tooltip_text) OVERRIDE;
  virtual void SelectionBoundsChanged(
      const ViewHostMsg_SelectionBounds_Params& params) OVERRIDE;
  virtual void ScrollOffsetChanged() OVERRIDE;
  virtual BackingStore* AllocBackingStore(const gfx::Size& size) OVERRIDE;
  virtual void CopyFromCompositingSurface(
      const gfx::Rect& src_subrect,
      const gfx::Size& dst_size,
      const base::Callback<void(bool, const SkBitmap&)>& callback) OVERRIDE;
  virtual void CopyFromCompositingSurfaceToVideoFrame(
      const gfx::Rect& src_subrect,
      const scoped_refptr<media::VideoFrame>& target,
      const base::Callback<void(bool)>& callback) OVERRIDE;
  virtual bool CanCopyToVideoFrame() const OVERRIDE;
  virtual void OnAcceleratedCompositingStateChange() OVERRIDE;
  virtual void AcceleratedSurfaceBuffersSwapped(
      const GpuHostMsg_AcceleratedSurfaceBuffersSwapped_Params& params,
      int gpu_host_id) OVERRIDE;
  virtual void AcceleratedSurfacePostSubBuffer(
      const GpuHostMsg_AcceleratedSurfacePostSubBuffer_Params& params,
      int gpu_host_id) OVERRIDE;
  virtual void AcceleratedSurfaceSuspend() OVERRIDE;
  virtual void AcceleratedSurfaceRelease() OVERRIDE;
  virtual bool HasAcceleratedSurface(const gfx::Size& desired_size) OVERRIDE;
  virtual void SetHasHorizontalScrollbar(
      bool has_horizontal_scrollbar) OVERRIDE;
  virtual void SetScrollOffsetPinning(
      bool is_pinned_to_left, bool is_pinned_to_right) OVERRIDE;
  virtual gfx::Rect GetBoundsInRootWindow() OVERRIDE;
  virtual gfx::GLSurfaceHandle GetCompositingSurface() OVERRIDE;
  virtual bool LockMouse() OVERRIDE;
  virtual void UnlockMouse() OVERRIDE;
  virtual void GetScreenInfo(WebKit::WebScreenInfo* results) OVERRIDE;
  virtual void OnAccessibilityNotifications(
      const std::vector<AccessibilityHostMsg_NotificationParams>&
          params) OVERRIDE;

#if defined(OS_MACOSX)
  // RenderWidgetHostView implementation.
  virtual void SetActive(bool active) OVERRIDE;
  virtual void SetTakesFocusOnlyOnMouseDown(bool flag) OVERRIDE;
  virtual void SetWindowVisibility(bool visible) OVERRIDE;
  virtual void WindowFrameChanged() OVERRIDE;
  virtual void ShowDefinitionForSelection() OVERRIDE;
  virtual bool SupportsSpeech() const OVERRIDE;
  virtual void SpeakSelection() OVERRIDE;
  virtual bool IsSpeaking() const OVERRIDE;
  virtual void StopSpeaking() OVERRIDE;

  // RenderWidgetHostViewPort implementation.
  virtual void AboutToWaitForBackingStoreMsg() OVERRIDE;
  virtual void PluginFocusChanged(bool focused, int plugin_id) OVERRIDE;
  virtual void StartPluginIme() OVERRIDE;
  virtual bool PostProcessEventForPluginIme(
      const NativeWebKeyboardEvent& event) OVERRIDE;
  virtual gfx::PluginWindowHandle AllocateFakePluginWindowHandle(
      bool opaque, bool root) OVERRIDE;
  virtual void DestroyFakePluginWindowHandle(
      gfx::PluginWindowHandle window) OVERRIDE;
  virtual void AcceleratedSurfaceSetIOSurface(
      gfx::PluginWindowHandle window,
      int32 width,
      int32 height,
      uint64 io_surface_identifier) OVERRIDE;
  virtual void AcceleratedSurfaceSetTransportDIB(
      gfx::PluginWindowHandle window,
      int32 width,
      int32 height,
      TransportDIB::Handle transport_dib) OVERRIDE;
#endif  // defined(OS_MACOSX)

#if defined(OS_ANDROID)
  // RenderWidgetHostView implementation.
  virtual void StartContentIntent(const GURL& content_url) OVERRIDE;
  virtual void SetCachedBackgroundColor(SkColor color) OVERRIDE;

  // RenderWidgetHostViewPort implementation.
  virtual void ShowDisambiguationPopup(const gfx::Rect& target_rect,
                                       const SkBitmap& zoomed_bitmap) OVERRIDE;
  virtual void SetCachedPageScaleFactorLimits(float minimum_scale,
                                              float maximum_scale) OVERRIDE;
  virtual void UpdateFrameInfo(const gfx::Vector2d& scroll_offset,
                               float page_scale_factor,
                               float min_page_scale_factor,
                               float max_page_scale_factor,
                               const gfx::Size& content_size,
                               const gfx::Vector2dF& controls_offset,
                               const gfx::Vector2dF& content_offset) OVERRIDE;
  virtual void HasTouchEventHandlers(bool need_touch_events) OVERRIDE;
#endif  // defined(OS_ANDROID)

#if defined(TOOLKIT_GTK)
  virtual GdkEventButton* GetLastMouseDown() OVERRIDE;
  virtual gfx::NativeView BuildInputMethodsGtkMenu() OVERRIDE;
  virtual void CreatePluginContainer(gfx::PluginWindowHandle id) OVERRIDE;
  virtual void DestroyPluginContainer(gfx::PluginWindowHandle id) OVERRIDE;
#endif  // defined(TOOLKIT_GTK)

#if defined(OS_WIN) && !defined(USE_AURA)
  virtual void WillWmDestroy() OVERRIDE;
#endif  // defined(OS_WIN) && !defined(USE_AURA)

 protected:
  friend class RenderWidgetHostView;

 private:
  // The model object.
  RenderWidgetHostImpl* host_;

  BrowserPluginGuest *guest_;
  bool enable_compositing_;
  bool is_hidden_;
  // The platform view for this RenderWidgetHostView.
  // RenderWidgetHostViewGuest mostly only cares about stuff related to
  // compositing, the rest are directly forwared to this |platform_view_|.
  RenderWidgetHostViewPort* platform_view_;

  DISALLOW_COPY_AND_ASSIGN(RenderWidgetHostViewGuest);
};

}  // namespace content

#endif  // CHROME_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_VIEW_GUEST_H_
