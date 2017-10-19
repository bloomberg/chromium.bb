// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/render_widget_host_view_base.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/unguessable_token.h"
#include "build/build_config.h"
#include "content/browser/accessibility/browser_accessibility_manager.h"
#include "content/browser/gpu/gpu_data_manager_impl.h"
#include "content/browser/renderer_host/input/synthetic_gesture_target_base.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_delegate.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_base_observer.h"
#include "content/browser/renderer_host/render_widget_host_view_frame_subscriber.h"
#include "content/browser/renderer_host/text_input_manager.h"
#include "content/common/content_switches_internal.h"
#include "content/public/common/content_features.h"
#include "media/base/video_frame.h"
#include "ui/base/layout.h"
#include "ui/base/ui_base_types.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gfx/geometry/size_f.h"

#if defined(USE_AURA)
#include "base/unguessable_token.h"
#include "content/common/render_widget_window_tree_client_factory.mojom.h"
#endif

namespace content {

RenderWidgetHostViewBase::RenderWidgetHostViewBase()
    : is_fullscreen_(false),
      popup_type_(blink::kWebPopupTypeNone),
      mouse_locked_(false),
      current_device_scale_factor_(0),
      current_display_rotation_(display::Display::ROTATE_0),
      text_input_manager_(nullptr),
      wheel_scroll_latching_enabled_(base::FeatureList::IsEnabled(
          features::kTouchpadAndWheelScrollLatching)),
      web_contents_accessibility_(nullptr),
      renderer_frame_number_(0),
      weak_factory_(this) {}

RenderWidgetHostViewBase::~RenderWidgetHostViewBase() {
  DCHECK(!mouse_locked_);
  // We call this here to guarantee that observers are notified before we go
  // away. However, some subclasses may wish to call this earlier in their
  // shutdown process, e.g. to force removal from
  // RenderWidgetHostInputEventRouter's surface map before relinquishing a
  // host pointer, as in RenderWidgetHostViewGuest. There is no harm in calling
  // NotifyObserversAboutShutdown() twice, as the observers are required to
  // de-register on the first call, and so the second call does nothing.
  NotifyObserversAboutShutdown();
  // If we have a live reference to |text_input_manager_|, we should unregister
  // so that the |text_input_manager_| will free its state.
  if (text_input_manager_)
    text_input_manager_->Unregister(this);
}

RenderWidgetHostImpl* RenderWidgetHostViewBase::GetFocusedWidget() const {
  RenderWidgetHostImpl* host =
      RenderWidgetHostImpl::From(GetRenderWidgetHost());

  return host && host->delegate()
             ? host->delegate()->GetFocusedRenderWidgetHost(host)
             : nullptr;
}

RenderWidgetHost* RenderWidgetHostViewBase::GetRenderWidgetHost() const {
  return nullptr;
}

void RenderWidgetHostViewBase::NotifyObserversAboutShutdown() {
  // Note: RenderWidgetHostInputEventRouter is an observer, and uses the
  // following notification to remove this view from its surface owners map.
  for (auto& observer : observers_)
    observer.OnRenderWidgetHostViewBaseDestroyed(this);
  // All observers are required to disconnect after they are notified.
  DCHECK(!observers_.might_have_observers());
}

bool RenderWidgetHostViewBase::OnMessageReceived(const IPC::Message& msg){
  return false;
}

void RenderWidgetHostViewBase::SetBackgroundColorToDefault() {
  SetBackgroundColor(SK_ColorWHITE);
}

gfx::Size RenderWidgetHostViewBase::GetPhysicalBackingSize() const {
  return gfx::ScaleToCeiledSize(
      GetRequestedRendererSize(),
      ui::GetScaleFactorForNativeView(GetNativeView()));
}

bool RenderWidgetHostViewBase::DoBrowserControlsShrinkBlinkSize() const {
  return false;
}

float RenderWidgetHostViewBase::GetTopControlsHeight() const {
  return 0.f;
}

void RenderWidgetHostViewBase::SelectionBoundsChanged(
    const ViewHostMsg_SelectionBounds_Params& params) {
#if !defined(OS_ANDROID)
  if (GetTextInputManager())
    GetTextInputManager()->SelectionBoundsChanged(this, params);
#else
  NOTREACHED() << "Selection bounds should be routed through the compositor.";
#endif
}

float RenderWidgetHostViewBase::GetBottomControlsHeight() const {
  return 0.f;
}

void RenderWidgetHostViewBase::SelectionChanged(const base::string16& text,
                                                size_t offset,
                                                const gfx::Range& range) {
  if (GetTextInputManager())
    GetTextInputManager()->SelectionChanged(this, text, offset, range);
}

gfx::Size RenderWidgetHostViewBase::GetRequestedRendererSize() const {
  return GetViewBounds().size();
}

ui::TextInputClient* RenderWidgetHostViewBase::GetTextInputClient() {
  NOTREACHED();
  return NULL;
}

void RenderWidgetHostViewBase::SetIsInVR(bool is_in_vr) {
  NOTIMPLEMENTED();
}

bool RenderWidgetHostViewBase::IsInVR() const {
  return false;
}

bool RenderWidgetHostViewBase::IsSurfaceAvailableForCopy() const {
  return false;
}

void RenderWidgetHostViewBase::CopyFromSurface(
    const gfx::Rect& src_rect,
    const gfx::Size& output_size,
    const ReadbackRequestCallback& callback,
    const SkColorType color_type) {
  NOTIMPLEMENTED();
  callback.Run(SkBitmap(), READBACK_SURFACE_UNAVAILABLE);
}

void RenderWidgetHostViewBase::CopyFromSurfaceToVideoFrame(
    const gfx::Rect& src_rect,
    scoped_refptr<media::VideoFrame> target,
    const base::Callback<void(const gfx::Rect&, bool)>& callback) {
  NOTIMPLEMENTED();
  callback.Run(gfx::Rect(), false);
}

base::string16 RenderWidgetHostViewBase::GetSelectedText() {
  if (!GetTextInputManager())
    return base::string16();
  return GetTextInputManager()->GetTextSelection(this)->selected_text();
}

bool RenderWidgetHostViewBase::IsMouseLocked() {
  return mouse_locked_;
}

InputEventAckState RenderWidgetHostViewBase::FilterInputEvent(
    const blink::WebInputEvent& input_event) {
  // By default, input events are simply forwarded to the renderer.
  return INPUT_EVENT_ACK_STATE_NOT_CONSUMED;
}

InputEventAckState RenderWidgetHostViewBase::FilterChildGestureEvent(
    const blink::WebGestureEvent& gesture_event) {
  // By default, do nothing with the child's gesture events.
  return INPUT_EVENT_ACK_STATE_NOT_CONSUMED;
}

void RenderWidgetHostViewBase::WheelEventAck(
    const blink::WebMouseWheelEvent& event,
    InputEventAckState ack_result) {
}

void RenderWidgetHostViewBase::GestureEventAck(
    const blink::WebGestureEvent& event,
    InputEventAckState ack_result) {
}

void RenderWidgetHostViewBase::SetPopupType(blink::WebPopupType popup_type) {
  popup_type_ = popup_type;
}

blink::WebPopupType RenderWidgetHostViewBase::GetPopupType() {
  return popup_type_;
}

BrowserAccessibilityManager*
RenderWidgetHostViewBase::CreateBrowserAccessibilityManager(
    BrowserAccessibilityDelegate* delegate, bool for_root_frame) {
  NOTREACHED();
  return NULL;
}

void RenderWidgetHostViewBase::AccessibilityShowMenu(const gfx::Point& point) {
  RenderWidgetHostImpl* impl = NULL;
  if (GetRenderWidgetHost())
    impl = RenderWidgetHostImpl::From(GetRenderWidgetHost());

  if (impl)
    impl->ShowContextMenuAtPoint(point, ui::MENU_SOURCE_NONE);
}

gfx::Point RenderWidgetHostViewBase::AccessibilityOriginInScreen(
    const gfx::Rect& bounds) {
  return bounds.origin();
}

gfx::AcceleratedWidget
    RenderWidgetHostViewBase::AccessibilityGetAcceleratedWidget() {
  return gfx::kNullAcceleratedWidget;
}

gfx::NativeViewAccessible
    RenderWidgetHostViewBase::AccessibilityGetNativeViewAccessible() {
  return NULL;
}

void RenderWidgetHostViewBase::UpdateScreenInfo(gfx::NativeView view) {
  RenderWidgetHostImpl* impl = NULL;
  if (GetRenderWidgetHost())
    impl = RenderWidgetHostImpl::From(GetRenderWidgetHost());

  if (impl && impl->delegate())
    impl->delegate()->SendScreenRects();

  if (HasDisplayPropertyChanged(view) && impl)
    impl->NotifyScreenInfoChanged();
}

bool RenderWidgetHostViewBase::HasDisplayPropertyChanged(gfx::NativeView view) {
  display::Display display =
      display::Screen::GetScreen()->GetDisplayNearestView(view);
  if (current_display_area_ == display.work_area() &&
      current_device_scale_factor_ == display.device_scale_factor() &&
      current_display_rotation_ == display.rotation() &&
      current_display_color_space_ == display.color_space()) {
    return false;
  }

  current_display_area_ = display.work_area();
  current_device_scale_factor_ = display.device_scale_factor();
  current_display_rotation_ = display.rotation();
  current_display_color_space_ = display.color_space();
  return true;
}

void RenderWidgetHostViewBase::DidUnregisterFromTextInputManager(
    TextInputManager* text_input_manager) {
  DCHECK(text_input_manager && text_input_manager_ == text_input_manager);

  text_input_manager_ = nullptr;
}

base::WeakPtr<RenderWidgetHostViewBase> RenderWidgetHostViewBase::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

std::unique_ptr<SyntheticGestureTarget>
RenderWidgetHostViewBase::CreateSyntheticGestureTarget() {
  RenderWidgetHostImpl* host =
      RenderWidgetHostImpl::From(GetRenderWidgetHost());
  return std::unique_ptr<SyntheticGestureTarget>(
      new SyntheticGestureTargetBase(host));
}

// Base implementation is unimplemented.
void RenderWidgetHostViewBase::BeginFrameSubscription(
    std::unique_ptr<RenderWidgetHostViewFrameSubscriber> subscriber) {
  NOTREACHED();
}

void RenderWidgetHostViewBase::EndFrameSubscription() {
  NOTREACHED();
}

void RenderWidgetHostViewBase::FocusedNodeTouched(
    const gfx::Point& location_dips_screen,
    bool editable) {
  DVLOG(1) << "FocusedNodeTouched: " << editable;
}

uint32_t RenderWidgetHostViewBase::RendererFrameNumber() {
  return renderer_frame_number_;
}

void RenderWidgetHostViewBase::DidReceiveRendererFrame() {
  ++renderer_frame_number_;
}

void RenderWidgetHostViewBase::ShowDisambiguationPopup(
    const gfx::Rect& rect_pixels,
    const SkBitmap& zoomed_bitmap) {
  NOTIMPLEMENTED();
}

gfx::Size RenderWidgetHostViewBase::GetVisibleViewportSize() const {
  return GetViewBounds().size();
}

void RenderWidgetHostViewBase::SetInsets(const gfx::Insets& insets) {
  NOTIMPLEMENTED();
}

void RenderWidgetHostViewBase::DisplayCursor(const WebCursor& cursor) {
  return;
}

CursorManager* RenderWidgetHostViewBase::GetCursorManager() {
  return nullptr;
}

// static
ScreenOrientationValues RenderWidgetHostViewBase::GetOrientationTypeForMobile(
    const display::Display& display) {
  int angle = display.RotationAsDegree();
  const gfx::Rect& bounds = display.bounds();

  // Whether the device's natural orientation is portrait.
  bool natural_portrait = false;
  if (angle == 0 || angle == 180) // The device is in its natural orientation.
    natural_portrait = bounds.height() >= bounds.width();
  else
    natural_portrait = bounds.height() <= bounds.width();

  switch (angle) {
  case 0:
    return natural_portrait ? SCREEN_ORIENTATION_VALUES_PORTRAIT_PRIMARY
                            : SCREEN_ORIENTATION_VALUES_LANDSCAPE_PRIMARY;
  case 90:
    return natural_portrait ? SCREEN_ORIENTATION_VALUES_LANDSCAPE_PRIMARY
                            : SCREEN_ORIENTATION_VALUES_PORTRAIT_SECONDARY;
  case 180:
    return natural_portrait ? SCREEN_ORIENTATION_VALUES_PORTRAIT_SECONDARY
                            : SCREEN_ORIENTATION_VALUES_LANDSCAPE_SECONDARY;
  case 270:
    return natural_portrait ? SCREEN_ORIENTATION_VALUES_LANDSCAPE_SECONDARY
                            : SCREEN_ORIENTATION_VALUES_PORTRAIT_PRIMARY;
  default:
    NOTREACHED();
    return SCREEN_ORIENTATION_VALUES_PORTRAIT_PRIMARY;
  }
}

// static
ScreenOrientationValues RenderWidgetHostViewBase::GetOrientationTypeForDesktop(
    const display::Display& display) {
  static int primary_landscape_angle = -1;
  static int primary_portrait_angle = -1;

  int angle = display.RotationAsDegree();
  const gfx::Rect& bounds = display.bounds();
  bool is_portrait = bounds.height() >= bounds.width();

  if (is_portrait && primary_portrait_angle == -1)
    primary_portrait_angle = angle;

  if (!is_portrait && primary_landscape_angle == -1)
    primary_landscape_angle = angle;

  if (is_portrait) {
    return primary_portrait_angle == angle
        ? SCREEN_ORIENTATION_VALUES_PORTRAIT_PRIMARY
        : SCREEN_ORIENTATION_VALUES_PORTRAIT_SECONDARY;
  }

  return primary_landscape_angle == angle
      ? SCREEN_ORIENTATION_VALUES_LANDSCAPE_PRIMARY
      : SCREEN_ORIENTATION_VALUES_LANDSCAPE_SECONDARY;
}

void RenderWidgetHostViewBase::OnDidNavigateMainFrameToNewPage() {
}

viz::FrameSinkId RenderWidgetHostViewBase::GetFrameSinkId() {
  return viz::FrameSinkId();
}

viz::LocalSurfaceId RenderWidgetHostViewBase::GetLocalSurfaceId() const {
  return viz::LocalSurfaceId();
}

viz::FrameSinkId RenderWidgetHostViewBase::FrameSinkIdAtPoint(
    viz::SurfaceHittestDelegate* delegate,
    const gfx::PointF& point,
    gfx::PointF* transformed_point) {
  NOTREACHED();
  return viz::FrameSinkId();
}

gfx::PointF RenderWidgetHostViewBase::TransformPointToRootCoordSpaceF(
    const gfx::PointF& point) {
  return point;
}

bool RenderWidgetHostViewBase::TransformPointToLocalCoordSpace(
    const gfx::PointF& point,
    const viz::SurfaceId& original_surface,
    gfx::PointF* transformed_point) {
  *transformed_point = point;
  return true;
}

bool RenderWidgetHostViewBase::TransformPointToCoordSpaceForView(
    const gfx::PointF& point,
    RenderWidgetHostViewBase* target_view,
    gfx::PointF* transformed_point) {
  NOTREACHED();
  return true;
}

bool RenderWidgetHostViewBase::IsRenderWidgetHostViewGuest() {
  return false;
}

bool RenderWidgetHostViewBase::IsRenderWidgetHostViewChildFrame() {
  return false;
}

void RenderWidgetHostViewBase::TextInputStateChanged(
    const TextInputState& text_input_state) {
  if (GetTextInputManager())
    GetTextInputManager()->UpdateTextInputState(this, text_input_state);
}

void RenderWidgetHostViewBase::ImeCancelComposition() {
  if (GetTextInputManager())
    GetTextInputManager()->ImeCancelComposition(this);
}

void RenderWidgetHostViewBase::ImeCompositionRangeChanged(
    const gfx::Range& range,
    const std::vector<gfx::Rect>& character_bounds) {
  if (GetTextInputManager()) {
    GetTextInputManager()->ImeCompositionRangeChanged(this, range,
                                                      character_bounds);
  }
}

TextInputManager* RenderWidgetHostViewBase::GetTextInputManager() {
  if (text_input_manager_)
    return text_input_manager_;

  RenderWidgetHostImpl* host =
      RenderWidgetHostImpl::From(GetRenderWidgetHost());
  if (!host || !host->delegate())
    return nullptr;

  // This RWHV needs to be registered with the TextInputManager so that the
  // TextInputManager starts tracking its state, and observing its lifetime.
  text_input_manager_ = host->delegate()->GetTextInputManager();
  if (text_input_manager_)
    text_input_manager_->Register(this);

  return text_input_manager_;
}

void RenderWidgetHostViewBase::AddObserver(
    RenderWidgetHostViewBaseObserver* observer) {
  observers_.AddObserver(observer);
}

void RenderWidgetHostViewBase::RemoveObserver(
    RenderWidgetHostViewBaseObserver* observer) {
  observers_.RemoveObserver(observer);
}

TouchSelectionControllerClientManager*
RenderWidgetHostViewBase::GetTouchSelectionControllerClientManager() {
  return nullptr;
}

#if defined(USE_AURA)
void RenderWidgetHostViewBase::EmbedChildFrameRendererWindowTreeClient(
    RenderWidgetHostViewBase* root_view,
    int routing_id,
    ui::mojom::WindowTreeClientPtr renderer_window_tree_client) {
  RenderWidgetHost* render_widget_host = GetRenderWidgetHost();
  if (!render_widget_host)
    return;
  const int embed_id = ++next_embed_id_;
  pending_embeds_[routing_id] = embed_id;
  root_view->ScheduleEmbed(
      std::move(renderer_window_tree_client),
      base::BindOnce(&RenderWidgetHostViewBase::OnDidScheduleEmbed,
                     GetWeakPtr(), routing_id, embed_id));
}

void RenderWidgetHostViewBase::OnChildFrameDestroyed(int routing_id) {
  DCHECK(render_widget_window_tree_client_);
  pending_embeds_.erase(routing_id);
  render_widget_window_tree_client_->DestroyFrame(routing_id);
}
#endif

bool RenderWidgetHostViewBase::IsChildFrameForTesting() const {
  return false;
}

viz::SurfaceId RenderWidgetHostViewBase::SurfaceIdForTesting() const {
  return viz::SurfaceId();
}

#if defined(USE_AURA)
void RenderWidgetHostViewBase::OnDidScheduleEmbed(
    int routing_id,
    int embed_id,
    const base::UnguessableToken& token) {
  auto iter = pending_embeds_.find(routing_id);
  if (iter == pending_embeds_.end() || iter->second != embed_id)
    return;
  pending_embeds_.erase(iter);
  DCHECK(render_widget_window_tree_client_);
  render_widget_window_tree_client_->Embed(routing_id, token);
}

void RenderWidgetHostViewBase::ScheduleEmbed(
    ui::mojom::WindowTreeClientPtr client,
    base::OnceCallback<void(const base::UnguessableToken&)> callback) {
  NOTREACHED();
}

ui::mojom::WindowTreeClientPtr
RenderWidgetHostViewBase::GetWindowTreeClientFromRenderer() {
  // NOTE: this function may be called multiple times.
  RenderWidgetHost* render_widget_host = GetRenderWidgetHost();
  mojom::RenderWidgetWindowTreeClientFactoryPtr factory;
  BindInterface(render_widget_host->GetProcess(), &factory);

  ui::mojom::WindowTreeClientPtr window_tree_client;
  factory->CreateWindowTreeClientForRenderWidget(
      render_widget_host->GetRoutingID(),
      mojo::MakeRequest(&window_tree_client),
      mojo::MakeRequest(&render_widget_window_tree_client_));
  return window_tree_client;
}

#endif

}  // namespace content
