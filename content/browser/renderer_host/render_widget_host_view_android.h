// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_VIEW_ANDROID_H_
#define CONTENT_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_VIEW_ANDROID_H_

#include <map>

#include "base/compiler_specific.h"
#include "base/i18n/rtl.h"
#include "base/memory/scoped_ptr.h"
#include "base/process.h"
#include "content/browser/renderer_host/ime_adapter_android.h"
#include "content/browser/renderer_host/render_widget_host_view_base.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebGraphicsContext3D.h"
#include "ui/gfx/size.h"
#include "ui/gfx/vector2d_f.h"

struct ViewHostMsg_TextInputState_Params;

struct GpuHostMsg_AcceleratedSurfaceBuffersSwapped_Params;
struct GpuHostMsg_AcceleratedSurfacePostSubBuffer_Params;

namespace cc {
class Layer;
class TextureLayer;
}

namespace WebKit {
class WebExternalTextureLayer;
class WebTouchEvent;
class WebMouseEvent;
}

namespace content {
class ContentViewCoreImpl;
class RenderWidgetHost;
class RenderWidgetHostImpl;
class SurfaceTextureTransportClient;
struct NativeWebKeyboardEvent;

// -----------------------------------------------------------------------------
// See comments in render_widget_host_view.h about this class and its members.
// -----------------------------------------------------------------------------
class RenderWidgetHostViewAndroid : public RenderWidgetHostViewBase {
 public:
  RenderWidgetHostViewAndroid(RenderWidgetHostImpl* widget,
                              ContentViewCoreImpl* content_view_core);
  virtual ~RenderWidgetHostViewAndroid();

  // RenderWidgetHostView implementation.
  virtual void InitAsChild(gfx::NativeView parent_view) OVERRIDE;
  virtual void InitAsPopup(RenderWidgetHostView* parent_host_view,
                           const gfx::Rect& pos) OVERRIDE;
  virtual void InitAsFullscreen(
      RenderWidgetHostView* reference_host_view) OVERRIDE;
  virtual RenderWidgetHost* GetRenderWidgetHost() const OVERRIDE;
  virtual void WasShown() OVERRIDE;
  virtual void WasHidden() OVERRIDE;
  virtual void SetSize(const gfx::Size& size) OVERRIDE;
  virtual void SetBounds(const gfx::Rect& rect) OVERRIDE;
  virtual gfx::NativeView GetNativeView() const OVERRIDE;
  virtual gfx::NativeViewId GetNativeViewId() const OVERRIDE;
  virtual gfx::NativeViewAccessible GetNativeViewAccessible() OVERRIDE;
  virtual void MovePluginWindows(
      const gfx::Vector2d& scroll_offset,
      const std::vector<webkit::npapi::WebPluginGeometry>& moves) OVERRIDE;
  virtual void Focus() OVERRIDE;
  virtual void Blur() OVERRIDE;
  virtual bool HasFocus() const OVERRIDE;
  virtual bool IsSurfaceAvailableForCopy() const OVERRIDE;
  virtual void Show() OVERRIDE;
  virtual void Hide() OVERRIDE;
  virtual bool IsShowing() OVERRIDE;
  virtual gfx::Rect GetViewBounds() const OVERRIDE;
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
  virtual void SetTooltipText(const string16& tooltip_text) OVERRIDE;
  virtual void SelectionChanged(const string16& text,
                                size_t offset,
                                const ui::Range& range) OVERRIDE;
  virtual void SelectionBoundsChanged(
      const ViewHostMsg_SelectionBounds_Params& params) OVERRIDE;
  virtual void ScrollOffsetChanged() OVERRIDE;
  virtual BackingStore* AllocBackingStore(const gfx::Size& size) OVERRIDE;
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
  virtual void SetBackground(const SkBitmap& background) OVERRIDE;
  virtual void CopyFromCompositingSurface(
      const gfx::Rect& src_subrect,
      const gfx::Size& dst_size,
      const base::Callback<void(bool, const SkBitmap&)>& callback) OVERRIDE;
  virtual void CopyFromCompositingSurfaceToVideoFrame(
      const gfx::Rect& src_subrect,
      const scoped_refptr<media::VideoFrame>& target,
      const base::Callback<void(bool)>& callback) OVERRIDE;
  virtual bool CanCopyToVideoFrame() const OVERRIDE;
  virtual void GetScreenInfo(WebKit::WebScreenInfo* results) OVERRIDE;
  virtual gfx::Rect GetBoundsInRootWindow() OVERRIDE;
  virtual gfx::GLSurfaceHandle GetCompositingSurface() OVERRIDE;
  virtual void ProcessAckedTouchEvent(const WebKit::WebTouchEvent& touch,
                                      InputEventAckState ack_result) OVERRIDE;
  virtual void SetHasHorizontalScrollbar(
      bool has_horizontal_scrollbar) OVERRIDE;
  virtual void SetScrollOffsetPinning(
      bool is_pinned_to_left, bool is_pinned_to_right) OVERRIDE;
  virtual void UnhandledWheelEvent(
      const WebKit::WebMouseWheelEvent& event) OVERRIDE;
  virtual void OnAccessibilityNotifications(
      const std::vector<AccessibilityHostMsg_NotificationParams>&
          params) OVERRIDE;
  virtual bool LockMouse() OVERRIDE;
  virtual void UnlockMouse() OVERRIDE;
  virtual void StartContentIntent(const GURL& content_url) OVERRIDE;
  virtual void HasTouchEventHandlers(bool need_touch_events) OVERRIDE;
  virtual void SetCachedBackgroundColor(SkColor color) OVERRIDE;
  virtual void SetCachedPageScaleFactorLimits(float minimum_scale,
                                              float maximum_scale) OVERRIDE;
  virtual void UpdateFrameInfo(const gfx::Vector2d& scroll_offset,
                               float page_scale_factor,
                               float min_page_scale_factor,
                               float max_page_scale_factor,
                               const gfx::Size& content_size,
                               const gfx::Vector2dF& controls_offset,
                               const gfx::Vector2dF& content_offset) OVERRIDE;
  virtual void ShowDisambiguationPopup(const gfx::Rect& target_rect,
                                       const SkBitmap& zoomed_bitmap) OVERRIDE;

  // Non-virtual methods
  void SetContentViewCore(ContentViewCoreImpl* content_view_core);
  SkColor GetCachedBackgroundColor() const;
  void SendKeyEvent(const NativeWebKeyboardEvent& event);
  void SendTouchEvent(const WebKit::WebTouchEvent& event);
  void SendMouseEvent(const WebKit::WebMouseEvent& event);
  void SendMouseWheelEvent(const WebKit::WebMouseWheelEvent& event);
  void SendGestureEvent(const WebKit::WebGestureEvent& event);

  int GetNativeImeAdapter();

  WebKit::WebGLId GetScaledContentTexture(float scale, gfx::Size* out_size);
  bool PopulateBitmapWithContents(jobject jbitmap);

  bool HasValidFrame() const;

  // Select all text between the given coordinates.
  void SelectRange(const gfx::Point& start, const gfx::Point& end);

  void MoveCaret(const gfx::Point& point);

 private:
  // The model object.
  RenderWidgetHostImpl* host_;

  // Whether or not this widget is potentially attached to the view hierarchy.
  // This view may not actually be attached if this is true, but it should be
  // treated as such, because as soon as a ContentViewCore is set the layer
  // will be attached automatically.
  bool is_layer_attached_;

  // ContentViewCoreImpl is our interface to the view system.
  ContentViewCoreImpl* content_view_core_;

  ImeAdapterAndroid ime_adapter_android_;

  // Body background color of the underlying document.
  SkColor cached_background_color_;

  // The texture layer for this view when using browser-side compositing.
  scoped_refptr<cc::TextureLayer> texture_layer_;

  // The layer used for rendering the contents of this view.
  // It is either owned by texture_layer_ or surface_texture_transport_
  // depending on the mode.
  scoped_refptr<cc::Layer> layer_;

  // The most recent texture id that was pushed to the texture layer.
  unsigned int texture_id_in_layer_;

  // The most recent texture size that was pushed to the texture layer.
  gfx::Size texture_size_in_layer_;

  // Used for image transport when needing to share resources across threads.
  scoped_ptr<SurfaceTextureTransportClient> surface_texture_transport_;

  // The mailbox name of the previously received frame.
  std::string current_mailbox_name_;

  DISALLOW_COPY_AND_ASSIGN(RenderWidgetHostViewAndroid);
};

} // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_VIEW_ANDROID_H_
