// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "content/browser/browser_plugin/browser_plugin_guest.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_guest.h"
#if defined(OS_WIN) || defined(USE_AURA)
#include "content/browser/renderer_host/ui_events_helper.h"
#endif
#include "content/common/browser_plugin/browser_plugin_messages.h"
#include "content/common/gpu/gpu_messages.h"
#include "content/common/view_messages.h"
#include "content/public/common/content_switches.h"
#include "skia/ext/platform_canvas.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebScreenInfo.h"
#include "webkit/plugins/npapi/webplugin.h"

namespace content {

namespace {

bool ShouldSendPinchGesture() {
  static bool pinch_allowed =
      CommandLine::ForCurrentProcess()->HasSwitch(switches::kEnablePinch);
  return pinch_allowed;
}

WebKit::WebGestureEvent CreateFlingCancelEvent(double time_stamp) {
  WebKit::WebGestureEvent gesture_event;
  gesture_event.timeStampSeconds = time_stamp;
  gesture_event.type = WebKit::WebGestureEvent::GestureFlingCancel;
  gesture_event.sourceDevice = WebKit::WebGestureEvent::Touchscreen;
  return gesture_event;
}

}  // namespace

RenderWidgetHostViewGuest::RenderWidgetHostViewGuest(
    RenderWidgetHost* widget_host,
    BrowserPluginGuest* guest,
    RenderWidgetHostView* platform_view)
    : host_(RenderWidgetHostImpl::From(widget_host)),
      guest_(guest),
      is_hidden_(false),
      platform_view_(static_cast<RenderWidgetHostViewPort*>(platform_view)) {
#if defined(OS_WIN) || defined(USE_AURA)
  gesture_recognizer_.reset(ui::GestureRecognizer::Create(this));
#endif  // defined(OS_WIN) || defined(USE_AURA)
  host_->SetView(this);
}

RenderWidgetHostViewGuest::~RenderWidgetHostViewGuest() {
}

RenderWidgetHost* RenderWidgetHostViewGuest::GetRenderWidgetHost() const {
  return host_;
}

void RenderWidgetHostViewGuest::WasShown() {
  if (!is_hidden_)
    return;
  is_hidden_ = false;
  host_->WasShown();
}

void RenderWidgetHostViewGuest::WasHidden() {
  if (is_hidden_)
    return;
  is_hidden_ = true;
  host_->WasHidden();
}

void RenderWidgetHostViewGuest::SetSize(const gfx::Size& size) {
  platform_view_->SetSize(size);
}

gfx::Rect RenderWidgetHostViewGuest::GetBoundsInRootWindow() {
  return platform_view_->GetBoundsInRootWindow();
}

gfx::GLSurfaceHandle RenderWidgetHostViewGuest::GetCompositingSurface() {
  return gfx::GLSurfaceHandle(gfx::kNullPluginWindow, gfx::TEXTURE_TRANSPORT);
}

#if defined(OS_WIN) || defined(USE_AURA)
void RenderWidgetHostViewGuest::ProcessAckedTouchEvent(
    const WebKit::WebTouchEvent& touch, InputEventAckState ack_result) {
  // TODO(fsamuel): Currently we will only take this codepath if the guest has
  // requested touch events. A better solution is to always forward touchpresses
  // to the embedder process to target a BrowserPlugin, and then route all
  // subsequent touch points of that touchdown to the appropriate guest until
  // that touch point is released.
  ScopedVector<ui::TouchEvent> events;
  if (!MakeUITouchEventsFromWebTouchEvents(touch, &events, LOCAL_COORDINATES))
    return;

  ui::EventResult result = (ack_result ==
      INPUT_EVENT_ACK_STATE_CONSUMED) ? ui::ER_HANDLED : ui::ER_UNHANDLED;
  for (ScopedVector<ui::TouchEvent>::iterator iter = events.begin(),
      end = events.end(); iter != end; ++iter)  {
    scoped_ptr<ui::GestureRecognizer::Gestures> gestures;
    gestures.reset(gesture_recognizer_->ProcessTouchEventForGesture(
        *(*iter), result, this));
    ProcessGestures(gestures.get());
  }
}
#endif

void RenderWidgetHostViewGuest::Show() {
  WasShown();
}

void RenderWidgetHostViewGuest::Hide() {
  WasHidden();
}

bool RenderWidgetHostViewGuest::IsShowing() {
  return !is_hidden_;
}

gfx::Rect RenderWidgetHostViewGuest::GetViewBounds() const {
  return platform_view_->GetViewBounds();
}

void RenderWidgetHostViewGuest::RenderViewGone(base::TerminationStatus status,
                                               int error_code) {
  platform_view_->RenderViewGone(status, error_code);
  // Destroy the guest view instance only, so we don't end up calling
  // platform_view_->Destroy().
  DestroyGuestView();
}

void RenderWidgetHostViewGuest::Destroy() {
  platform_view_->Destroy();
  // The RenderWidgetHost's destruction led here, so don't call it.
  DestroyGuestView();
}

void RenderWidgetHostViewGuest::SetTooltipText(const string16& tooltip_text) {
  platform_view_->SetTooltipText(tooltip_text);
}

void RenderWidgetHostViewGuest::AcceleratedSurfaceBuffersSwapped(
    const GpuHostMsg_AcceleratedSurfaceBuffersSwapped_Params& params,
    int gpu_host_id) {
  // If accelerated surface buffers are getting swapped then we're not using
  // the software path.
  guest_->clear_damage_buffer();
  guest_->SendMessageToEmbedder(
      new BrowserPluginMsg_BuffersSwapped(
          guest_->instance_id(),
          params.size,
          params.mailbox_name,
          params.route_id,
          gpu_host_id));
}

void RenderWidgetHostViewGuest::AcceleratedSurfacePostSubBuffer(
    const GpuHostMsg_AcceleratedSurfacePostSubBuffer_Params& params,
    int gpu_host_id) {
  NOTREACHED();
}

void RenderWidgetHostViewGuest::SetBounds(const gfx::Rect& rect) {
  platform_view_->SetBounds(rect);
}

bool RenderWidgetHostViewGuest::OnMessageReceived(const IPC::Message& msg) {
  return platform_view_->OnMessageReceived(msg);
}

void RenderWidgetHostViewGuest::InitAsChild(
    gfx::NativeView parent_view) {
  platform_view_->InitAsChild(parent_view);
}

void RenderWidgetHostViewGuest::InitAsPopup(
    RenderWidgetHostView* parent_host_view, const gfx::Rect& pos) {
  // This should never get called.
  NOTREACHED();
}

void RenderWidgetHostViewGuest::InitAsFullscreen(
    RenderWidgetHostView* reference_host_view) {
  // This should never get called.
  NOTREACHED();
}

gfx::NativeView RenderWidgetHostViewGuest::GetNativeView() const {
  return guest_->GetEmbedderRenderWidgetHostView()->GetNativeView();
}

gfx::NativeViewId RenderWidgetHostViewGuest::GetNativeViewId() const {
  return guest_->GetEmbedderRenderWidgetHostView()->GetNativeViewId();
}

gfx::NativeViewAccessible RenderWidgetHostViewGuest::GetNativeViewAccessible() {
  return guest_->GetEmbedderRenderWidgetHostView()->GetNativeViewAccessible();
}

void RenderWidgetHostViewGuest::MovePluginWindows(
    const gfx::Vector2d& scroll_offset,
    const std::vector<webkit::npapi::WebPluginGeometry>& moves) {
  platform_view_->MovePluginWindows(scroll_offset, moves);
}

void RenderWidgetHostViewGuest::Focus() {
}

void RenderWidgetHostViewGuest::Blur() {
}

bool RenderWidgetHostViewGuest::HasFocus() const {
  return false;
}

bool RenderWidgetHostViewGuest::IsSurfaceAvailableForCopy() const {
  NOTIMPLEMENTED();
  return false;
}

void RenderWidgetHostViewGuest::UpdateCursor(const WebCursor& cursor) {
  platform_view_->UpdateCursor(cursor);
}

void RenderWidgetHostViewGuest::SetIsLoading(bool is_loading) {
  platform_view_->SetIsLoading(is_loading);
}

void RenderWidgetHostViewGuest::TextInputStateChanged(
    const ViewHostMsg_TextInputState_Params& params) {
  platform_view_->TextInputStateChanged(params);
}

void RenderWidgetHostViewGuest::ImeCancelComposition() {
  platform_view_->ImeCancelComposition();
}

void RenderWidgetHostViewGuest::ImeCompositionRangeChanged(
    const ui::Range& range,
    const std::vector<gfx::Rect>& character_bounds) {
}

void RenderWidgetHostViewGuest::DidUpdateBackingStore(
    const gfx::Rect& scroll_rect,
    const gfx::Vector2d& scroll_delta,
    const std::vector<gfx::Rect>& copy_rects) {
  NOTREACHED();
}

void RenderWidgetHostViewGuest::SelectionBoundsChanged(
    const ViewHostMsg_SelectionBounds_Params& params) {
  platform_view_->SelectionBoundsChanged(params);
}

void RenderWidgetHostViewGuest::ScrollOffsetChanged() {
}

BackingStore* RenderWidgetHostViewGuest::AllocBackingStore(
    const gfx::Size& size) {
  NOTREACHED();
  return NULL;
}

void RenderWidgetHostViewGuest::CopyFromCompositingSurface(
    const gfx::Rect& src_subrect,
    const gfx::Size& /* dst_size */,
    const base::Callback<void(bool, const SkBitmap&)>& callback) {
  callback.Run(false, SkBitmap());
}

void RenderWidgetHostViewGuest::CopyFromCompositingSurfaceToVideoFrame(
      const gfx::Rect& src_subrect,
      const scoped_refptr<media::VideoFrame>& target,
      const base::Callback<void(bool)>& callback) {
  NOTIMPLEMENTED();
  callback.Run(false);
}

bool RenderWidgetHostViewGuest::CanCopyToVideoFrame() const {
  return false;
}

void RenderWidgetHostViewGuest::AcceleratedSurfaceSuspend() {
  NOTREACHED();
}

void RenderWidgetHostViewGuest::AcceleratedSurfaceRelease() {
}

bool RenderWidgetHostViewGuest::HasAcceleratedSurface(
      const gfx::Size& desired_size) {
  return false;
}

void RenderWidgetHostViewGuest::SetBackground(const SkBitmap& background) {
  platform_view_->SetBackground(background);
}

#if defined(OS_WIN) && !defined(USE_AURA)
void RenderWidgetHostViewGuest::SetClickthroughRegion(SkRegion* region) {
}
#endif

#if defined(OS_WIN) && defined(USE_AURA)
void RenderWidgetHostViewGuest::SetParentNativeViewAccessible(
    gfx::NativeViewAccessible accessible_parent) {
}
#endif

void RenderWidgetHostViewGuest::SetHasHorizontalScrollbar(
    bool has_horizontal_scrollbar) {
  platform_view_->SetHasHorizontalScrollbar(has_horizontal_scrollbar);
}

void RenderWidgetHostViewGuest::SetScrollOffsetPinning(
    bool is_pinned_to_left, bool is_pinned_to_right) {
  platform_view_->SetScrollOffsetPinning(
      is_pinned_to_left, is_pinned_to_right);
}

void RenderWidgetHostViewGuest::OnAcceleratedCompositingStateChange() {
}

bool RenderWidgetHostViewGuest::LockMouse() {
  return platform_view_->LockMouse();
}

void RenderWidgetHostViewGuest::UnlockMouse() {
  return platform_view_->UnlockMouse();
}

void RenderWidgetHostViewGuest::GetScreenInfo(WebKit::WebScreenInfo* results) {
  platform_view_->GetScreenInfo(results);
}

void RenderWidgetHostViewGuest::OnAccessibilityNotifications(
    const std::vector<AccessibilityHostMsg_NotificationParams>& params) {
}

#if defined(OS_MACOSX)
void RenderWidgetHostViewGuest::SetActive(bool active) {
  platform_view_->SetActive(active);
}

void RenderWidgetHostViewGuest::SetTakesFocusOnlyOnMouseDown(bool flag) {
  platform_view_->SetTakesFocusOnlyOnMouseDown(flag);
}

void RenderWidgetHostViewGuest::SetWindowVisibility(bool visible) {
  platform_view_->SetWindowVisibility(visible);
}

void RenderWidgetHostViewGuest::WindowFrameChanged() {
  platform_view_->WindowFrameChanged();
}

void RenderWidgetHostViewGuest::ShowDefinitionForSelection() {
  platform_view_->ShowDefinitionForSelection();
}

bool RenderWidgetHostViewGuest::SupportsSpeech() const {
  return platform_view_->SupportsSpeech();
}

void RenderWidgetHostViewGuest::SpeakSelection() {
  platform_view_->SpeakSelection();
}

bool RenderWidgetHostViewGuest::IsSpeaking() const {
  return platform_view_->IsSpeaking();
}

void RenderWidgetHostViewGuest::StopSpeaking() {
  platform_view_->StopSpeaking();
}

void RenderWidgetHostViewGuest::AboutToWaitForBackingStoreMsg() {
  NOTREACHED();
}

bool RenderWidgetHostViewGuest::PostProcessEventForPluginIme(
    const NativeWebKeyboardEvent& event) {
  return false;
}

#endif  // defined(OS_MACOSX)

#if defined(OS_ANDROID)
void RenderWidgetHostViewGuest::ShowDisambiguationPopup(
    const gfx::Rect& target_rect,
    const SkBitmap& zoomed_bitmap) {
}

void RenderWidgetHostViewGuest::UpdateFrameInfo(
    const gfx::Vector2dF& scroll_offset,
    float page_scale_factor,
    const gfx::Vector2dF& page_scale_factor_limits,
    const gfx::SizeF& content_size,
    const gfx::SizeF& viewport_size,
    const gfx::Vector2dF& controls_offset,
    const gfx::Vector2dF& content_offset) {
}

void RenderWidgetHostViewGuest::HasTouchEventHandlers(bool need_touch_events) {
}
#endif  // defined(OS_ANDROID)

#if defined(TOOLKIT_GTK)
GdkEventButton* RenderWidgetHostViewGuest::GetLastMouseDown() {
  return NULL;
}

gfx::NativeView RenderWidgetHostViewGuest::BuildInputMethodsGtkMenu() {
  return gfx::NativeView();
}
#endif  // defined(TOOLKIT_GTK)

#if defined(OS_WIN) && !defined(USE_AURA)
void RenderWidgetHostViewGuest::WillWmDestroy() {
}
#endif

void RenderWidgetHostViewGuest::DestroyGuestView() {
  host_ = NULL;
  MessageLoop::current()->DeleteSoon(FROM_HERE, this);
}

bool RenderWidgetHostViewGuest::DispatchLongPressGestureEvent(
    ui::GestureEvent* event) {
  return ForwardGestureEventToRenderer(event);
}

bool RenderWidgetHostViewGuest::DispatchCancelTouchEvent(
    ui::TouchEvent* event) {
  if (!host_)
    return false;

  WebKit::WebTouchEvent cancel_event;
  cancel_event.type = WebKit::WebInputEvent::TouchCancel;
  cancel_event.timeStampSeconds = event->time_stamp().InSecondsF();
  host_->ForwardTouchEvent(cancel_event);
  return true;
}

bool RenderWidgetHostViewGuest::ForwardGestureEventToRenderer(
    ui::GestureEvent* gesture) {
#if defined(OS_WIN) || defined(USE_AURA)
  if (!host_)
    return false;

  // Pinch gestures are disabled by default on windows desktop. See
  // crbug.com/128477 and crbug.com/148816
  if ((gesture->type() == ui::ET_GESTURE_PINCH_BEGIN ||
      gesture->type() == ui::ET_GESTURE_PINCH_UPDATE ||
      gesture->type() == ui::ET_GESTURE_PINCH_END) &&
      !ShouldSendPinchGesture()) {
    return true;
  }

  WebKit::WebGestureEvent web_gesture =
      MakeWebGestureEventFromUIEvent(*gesture);
  const gfx::Point& client_point = gesture->location();
  const gfx::Point& screen_point = gesture->location();

  web_gesture.x = client_point.x();
  web_gesture.y = client_point.y();
  web_gesture.globalX = screen_point.x();
  web_gesture.globalY = screen_point.y();

  if (web_gesture.type == WebKit::WebGestureEvent::Undefined)
    return false;
  if (web_gesture.type == WebKit::WebGestureEvent::GestureTapDown) {
    host_->ForwardGestureEvent(
        CreateFlingCancelEvent(gesture->time_stamp().InSecondsF()));
  }
  host_->ForwardGestureEvent(web_gesture);
  return true;
#else
  return false;
#endif
}

void RenderWidgetHostViewGuest::ProcessGestures(
    ui::GestureRecognizer::Gestures* gestures) {
  if ((gestures == NULL) || gestures->empty())
    return;
  for (ui::GestureRecognizer::Gestures::iterator g_it = gestures->begin();
      g_it != gestures->end();
      ++g_it) {
    ForwardGestureEventToRenderer(*g_it);
  }
}


}  // namespace content
