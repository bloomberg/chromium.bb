// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FRAME_HOST_RENDER_WIDGET_HOST_VIEW_GUEST_H_
#define CONTENT_BROWSER_FRAME_HOST_RENDER_WIDGET_HOST_VIEW_GUEST_H_

#include <vector>

#include "base/memory/scoped_ptr.h"
#include "content/browser/frame_host/render_widget_host_view_child_frame.h"
#include "content/common/content_export.h"
#include "content/common/cursors/webcursor.h"
#include "ui/events/event.h"
#include "ui/events/gestures/gesture_recognizer.h"
#include "ui/events/gestures/gesture_types.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/vector2d_f.h"

struct ViewHostMsg_TextInputState_Params;

namespace content {
class BrowserPluginGuest;
class RenderWidgetHost;
class RenderWidgetHostImpl;
struct NativeWebKeyboardEvent;

// See comments in render_widget_host_view.h about this class and its members.
// This version is for the BrowserPlugin which handles a lot of the
// functionality in a diffent place and isn't platform specific.
// The BrowserPlugin is currently a special case for out-of-process rendered
// content and therefore inherits from RenderWidgetHostViewChildFrame.
// Eventually all RenderWidgetHostViewGuest code will be subsumed by
// RenderWidgetHostViewChildFrame and this class will be removed.
//
// Some elements that are platform specific will be deal with by delegating
// the relevant calls to the platform view.
class CONTENT_EXPORT RenderWidgetHostViewGuest
    : public RenderWidgetHostViewChildFrame,
      public ui::GestureConsumer,
      public ui::GestureEventHelper {
 public:
  RenderWidgetHostViewGuest(RenderWidgetHost* widget,
                            BrowserPluginGuest* guest,
                            RenderWidgetHostViewBase* platform_view);
  virtual ~RenderWidgetHostViewGuest();

  bool OnMessageReceivedFromEmbedder(const IPC::Message& message,
                                     RenderWidgetHostImpl* embedder);

  // RenderWidgetHostView implementation.
  virtual bool OnMessageReceived(const IPC::Message& msg) override;
  virtual void InitAsChild(gfx::NativeView parent_view) override;
  virtual void SetSize(const gfx::Size& size) override;
  virtual void SetBounds(const gfx::Rect& rect) override;
  virtual void Focus() override;
  virtual bool HasFocus() const override;
  virtual gfx::NativeView GetNativeView() const override;
  virtual gfx::NativeViewId GetNativeViewId() const override;
  virtual gfx::NativeViewAccessible GetNativeViewAccessible() override;
  virtual gfx::Rect GetViewBounds() const override;
  virtual void SetBackgroundOpaque(bool opaque) override;
  virtual gfx::Size GetPhysicalBackingSize() const override;
  virtual base::string16 GetSelectedText() const override;

  // RenderWidgetHostViewBase implementation.
  virtual void InitAsPopup(RenderWidgetHostView* parent_host_view,
                           const gfx::Rect& pos) override;
  virtual void InitAsFullscreen(
      RenderWidgetHostView* reference_host_view) override;
  virtual void WasShown() override;
  virtual void WasHidden() override;
  virtual void MovePluginWindows(
      const std::vector<WebPluginGeometry>& moves) override;
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
      CopyFromCompositingSurfaceCallback& callback,
      const SkColorType color_type) override;
  virtual void OnSwapCompositorFrame(
      uint32 output_surface_id,
      scoped_ptr<cc::CompositorFrame> frame) override;
#if defined(USE_AURA)
  virtual void ProcessAckedTouchEvent(
      const TouchEventWithLatencyInfo& touch,
      InputEventAckState ack_result) override;
#endif
  virtual bool LockMouse() override;
  virtual void UnlockMouse() override;
  virtual void GetScreenInfo(blink::WebScreenInfo* results) override;

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

#if defined(OS_ANDROID) || defined(TOOLKIT_VIEWS)
  // RenderWidgetHostViewBase implementation.
  virtual void ShowDisambiguationPopup(const gfx::Rect& rect_pixels,
                                       const SkBitmap& zoomed_bitmap) override;
#endif  // defined(OS_ANDROID) || defined(TOOLKIT_VIEWS)

#if defined(OS_ANDROID)
  virtual void LockCompositingSurface() override;
  virtual void UnlockCompositingSurface() override;
#endif  // defined(OS_ANDROID)

#if defined(OS_WIN)
  virtual void SetParentNativeViewAccessible(
      gfx::NativeViewAccessible accessible_parent) override;
  virtual gfx::NativeViewId GetParentForWindowlessPlugin() const override;
#endif

  // Overridden from ui::GestureEventHelper.
  virtual bool CanDispatchToConsumer(ui::GestureConsumer* consumer) override;
  virtual void DispatchGestureEvent(ui::GestureEvent* event) override;
  virtual void DispatchCancelTouchEvent(ui::TouchEvent* event) override;

  virtual SkColorType PreferredReadbackFormat() override;

 protected:
  friend class RenderWidgetHostView;

 private:
  // Destroys this view without calling |Destroy| on |platform_view_|.
  void DestroyGuestView();

  // Builds and forwards a WebKitGestureEvent to the renderer.
  bool ForwardGestureEventToRenderer(ui::GestureEvent* gesture);

  // Process all of the given gestures (passes them on to renderer)
  void ProcessGestures(ui::GestureRecognizer::Gestures* gestures);

  RenderWidgetHostViewBase* GetGuestRenderWidgetHostView() const;

  void OnHandleInputEvent(RenderWidgetHostImpl* embedder,
                          int browser_plugin_instance_id,
                          const gfx::Rect& guest_window_rect,
                          const blink::WebInputEvent* event);

  // BrowserPluginGuest and RenderWidgetHostViewGuest's lifetimes are not tied
  // to one another, therefore we access |guest_| through WeakPtr.
  base::WeakPtr<BrowserPluginGuest> guest_;
  gfx::Size size_;
  // The platform view for this RenderWidgetHostView.
  // RenderWidgetHostViewGuest mostly only cares about stuff related to
  // compositing, the rest are directly forwared to this |platform_view_|.
  RenderWidgetHostViewBase* platform_view_;
#if defined(USE_AURA)
  scoped_ptr<ui::GestureRecognizer> gesture_recognizer_;
#endif
  DISALLOW_COPY_AND_ASSIGN(RenderWidgetHostViewGuest);
};

}  // namespace content

#endif  // CONTENT_BROWSER_FRAME_HOST_RENDER_WIDGET_HOST_VIEW_GUEST_H_
