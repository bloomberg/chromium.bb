// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FRAME_HOST_RENDER_WIDGET_HOST_VIEW_CHILD_FRAME_H_
#define CONTENT_BROWSER_FRAME_HOST_RENDER_WIDGET_HOST_VIEW_CHILD_FRAME_H_

#include "base/memory/scoped_ptr.h"
#include "content/browser/renderer_host/render_widget_host_view_base.h"
#include "content/common/content_export.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/rect.h"

struct ViewHostMsg_TextInputState_Params;

namespace content {
class CrossProcessFrameConnector;
class RenderWidgetHost;
class RenderWidgetHostImpl;

// RenderWidgetHostViewChildFrame implements the view for a RenderWidgetHost
// associated with content being rendered in a separate process from
// content that is embedding it. This is not a platform-specific class; rather,
// the embedding renderer process implements the platform containing the
// widget, and the top-level frame's RenderWidgetHostView will ultimately
// manage all native widget interaction.
//
// See comments in render_widget_host_view.h about this class and its members.
class CONTENT_EXPORT RenderWidgetHostViewChildFrame
    : public RenderWidgetHostViewBase {
 public:
  explicit RenderWidgetHostViewChildFrame(RenderWidgetHost* widget);
  virtual ~RenderWidgetHostViewChildFrame();

  void set_cross_process_frame_connector(
      CrossProcessFrameConnector* frame_connector) {
    frame_connector_ = frame_connector;
  }

  // RenderWidgetHostView implementation.
  virtual void InitAsChild(gfx::NativeView parent_view) override;
  virtual RenderWidgetHost* GetRenderWidgetHost() const override;
  virtual void SetSize(const gfx::Size& size) override;
  virtual void SetBounds(const gfx::Rect& rect) override;
  virtual void Focus() override;
  virtual bool HasFocus() const override;
  virtual bool IsSurfaceAvailableForCopy() const override;
  virtual void Show() override;
  virtual void Hide() override;
  virtual bool IsShowing() override;
  virtual gfx::Rect GetViewBounds() const override;
  virtual gfx::Vector2dF GetLastScrollOffset() const override;
  virtual gfx::NativeView GetNativeView() const override;
  virtual gfx::NativeViewId GetNativeViewId() const override;
  virtual gfx::NativeViewAccessible GetNativeViewAccessible() override;
  virtual void SetBackgroundOpaque(bool opaque) override;
  virtual gfx::Size GetPhysicalBackingSize() const override;

  // RenderWidgetHostViewBase implementation.
  virtual void InitAsPopup(RenderWidgetHostView* parent_host_view,
                           const gfx::Rect& pos) override;
  virtual void InitAsFullscreen(
      RenderWidgetHostView* reference_host_view) override;
  virtual void WasShown() override;
  virtual void WasHidden() override;
  virtual void MovePluginWindows(
      const std::vector<WebPluginGeometry>& moves) override;
  virtual void Blur() override;
  virtual void UpdateCursor(const WebCursor& cursor) override;
  virtual void SetIsLoading(bool is_loading) override;
  virtual void TextInputStateChanged(
      const ViewHostMsg_TextInputState_Params& params) override;
  virtual void ImeCancelComposition() override;
#if defined(OS_MACOSX) || defined(USE_AURA)
  virtual void ImeCompositionRangeChanged(
      const gfx::Range& range,
      const std::vector<gfx::Rect>& character_bounds) override;
#endif
  virtual void RenderProcessGone(base::TerminationStatus status,
                                 int error_code) override;
  virtual void Destroy() override;
  virtual void SetTooltipText(const base::string16& tooltip_text) override;
  virtual void SelectionChanged(const base::string16& text,
                                size_t offset,
                                const gfx::Range& range) override;
  virtual void SelectionBoundsChanged(
      const ViewHostMsg_SelectionBounds_Params& params) override;
  virtual void CopyFromCompositingSurface(
      const gfx::Rect& src_subrect,
      const gfx::Size& dst_size,
      const base::Callback<void(bool, const SkBitmap&)>& callback,
      const SkColorType color_type) override;
  virtual void CopyFromCompositingSurfaceToVideoFrame(
      const gfx::Rect& src_subrect,
      const scoped_refptr<media::VideoFrame>& target,
      const base::Callback<void(bool)>& callback) override;
  virtual bool CanCopyToVideoFrame() const override;
  virtual bool HasAcceleratedSurface(const gfx::Size& desired_size) override;
  virtual void OnSwapCompositorFrame(
      uint32 output_surface_id,
      scoped_ptr<cc::CompositorFrame> frame) override;
  virtual void GetScreenInfo(blink::WebScreenInfo* results) override;
  virtual gfx::Rect GetBoundsInRootWindow() override;
  virtual gfx::GLSurfaceHandle GetCompositingSurface() override;
#if defined(USE_AURA)
  virtual void ProcessAckedTouchEvent(
      const TouchEventWithLatencyInfo& touch,
      InputEventAckState ack_result) override;
#endif  // defined(USE_AURA)
  virtual bool LockMouse() override;
  virtual void UnlockMouse() override;

#if defined(OS_MACOSX)
  // RenderWidgetHostView implementation.
  virtual void SetActive(bool active) override;
  virtual void SetTakesFocusOnlyOnMouseDown(bool flag) override;
  virtual void SetWindowVisibility(bool visible) override;
  virtual void WindowFrameChanged() override;
  virtual void ShowDefinitionForSelection() override;
  virtual bool SupportsSpeech() const override;
  virtual void SpeakSelection() override;
  virtual bool IsSpeaking() const override;
  virtual void StopSpeaking() override;

  // RenderWidgetHostViewBase implementation.
  virtual bool PostProcessEventForPluginIme(
      const NativeWebKeyboardEvent& event) override;
#endif  // defined(OS_MACOSX)

  // RenderWidgetHostViewBase implementation.
#if defined(OS_ANDROID)
  virtual void LockCompositingSurface() override;
  virtual void UnlockCompositingSurface() override;
#endif  // defined(OS_ANDROID)

#if defined(OS_WIN)
  virtual void SetParentNativeViewAccessible(
      gfx::NativeViewAccessible accessible_parent) override;
  virtual gfx::NativeViewId GetParentForWindowlessPlugin() const override;
#endif
  virtual BrowserAccessibilityManager* CreateBrowserAccessibilityManager(
      BrowserAccessibilityDelegate* delegate) override;

  virtual SkColorType PreferredReadbackFormat() override;

 protected:
  friend class RenderWidgetHostView;

  // The last scroll offset of the view.
  gfx::Vector2dF last_scroll_offset_;

  // Members will become private when RenderWidgetHostViewGuest is removed.
  // The model object.
  RenderWidgetHostImpl* host_;

  // frame_connector_ provides a platform abstraction. Messages
  // sent through it are routed to the embedding renderer process.
  CrossProcessFrameConnector* frame_connector_;

 private:
  DISALLOW_COPY_AND_ASSIGN(RenderWidgetHostViewChildFrame);
};

}  // namespace content

#endif  // CONTENT_BROWSER_FRAME_HOST_RENDER_WIDGET_HOST_VIEW_CHILD_FRAME_H_
