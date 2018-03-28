// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/render_widget_host_view_base.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/unguessable_token.h"
#include "build/build_config.h"
#include "components/viz/host/host_frame_sink_manager.h"
#include "components/viz/service/frame_sinks/frame_sink_manager_impl.h"
#include "components/viz/service/surfaces/surface_hittest.h"
#include "content/browser/accessibility/browser_accessibility_manager.h"
#include "content/browser/compositor/surface_utils.h"
#include "content/browser/gpu/gpu_data_manager_impl.h"
#include "content/browser/renderer_host/display_util.h"
#include "content/browser/renderer_host/input/synthetic_gesture_target_base.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_delegate.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_base_observer.h"
#include "content/browser/renderer_host/text_input_manager.h"
#include "content/common/content_switches_internal.h"
#include "content/public/common/content_features.h"
#include "ui/base/layout.h"
#include "ui/base/ui_base_types.h"
#include "ui/display/screen.h"
#include "ui/events/event.h"
#include "ui/gfx/geometry/dip_util.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gfx/geometry/size_f.h"

#if defined(USE_AURA)
#include "base/unguessable_token.h"
#include "content/common/render_widget_window_tree_client_factory.mojom.h"
#endif

namespace content {

RenderWidgetHostViewBase::RenderWidgetHostViewBase(RenderWidgetHost* host)
    : host_(RenderWidgetHostImpl::From(host)),
      is_fullscreen_(false),
      popup_type_(blink::kWebPopupTypeNone),
      current_device_scale_factor_(0),
      current_display_rotation_(display::Display::ROTATE_0),
      text_input_manager_(nullptr),
      wheel_scroll_latching_enabled_(base::FeatureList::IsEnabled(
          features::kTouchpadAndWheelScrollLatching)),
      web_contents_accessibility_(nullptr),
      is_currently_scrolling_viewport_(false),
      renderer_frame_number_(0),
      weak_factory_(this) {
  host_->render_frame_metadata_provider()->AddObserver(this);
}

RenderWidgetHostViewBase::~RenderWidgetHostViewBase() {
  DCHECK(!keyboard_locked_);
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
  if (host_)
    host_->render_frame_metadata_provider()->RemoveObserver(this);
}

RenderWidgetHostImpl* RenderWidgetHostViewBase::GetFocusedWidget() const {
  return host() && host()->delegate()
             ? host()->delegate()->GetFocusedRenderWidgetHost(host())
             : nullptr;
}

RenderWidgetHost* RenderWidgetHostViewBase::GetRenderWidgetHost() const {
  return host();
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

void RenderWidgetHostViewBase::OnRenderFrameMetadataChanged() {
  is_scroll_offset_at_top_ = host_->render_frame_metadata_provider()
                                 ->LastRenderFrameMetadata()
                                 .is_scroll_offset_at_top;
}

void RenderWidgetHostViewBase::OnRenderFrameSubmission() {}

void RenderWidgetHostViewBase::SetBackgroundColorToDefault() {
  SetBackgroundColor(SK_ColorWHITE);
}

gfx::Size RenderWidgetHostViewBase::GetCompositorViewportPixelSize() const {
  return gfx::ScaleToCeiledSize(GetRequestedRendererSize(),
                                GetDeviceScaleFactor());
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

int RenderWidgetHostViewBase::GetMouseWheelMinimumGranularity() const {
  // Most platforms can specify the floating-point delta in the wheel event so
  // they don't have a minimum granularity. Android is currently the only
  // platform that overrides this.
  return 0;
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
  return nullptr;
}

void RenderWidgetHostViewBase::SetIsInVR(bool is_in_vr) {
  NOTIMPLEMENTED();
}

bool RenderWidgetHostViewBase::IsInVR() const {
  return false;
}

viz::FrameSinkId RenderWidgetHostViewBase::GetRootFrameSinkId() {
  return viz::FrameSinkId();
}

bool RenderWidgetHostViewBase::IsSurfaceAvailableForCopy() const {
  return false;
}

void RenderWidgetHostViewBase::CopyFromSurface(
    const gfx::Rect& src_rect,
    const gfx::Size& output_size,
    base::OnceCallback<void(const SkBitmap&)> callback) {
  NOTIMPLEMENTED();
  std::move(callback).Run(SkBitmap());
}

viz::mojom::FrameSinkVideoCapturerPtr
RenderWidgetHostViewBase::CreateVideoCapturer() {
  viz::mojom::FrameSinkVideoCapturerPtr video_capturer;
  GetHostFrameSinkManager()->CreateVideoCapturer(
      mojo::MakeRequest(&video_capturer));
  video_capturer->ChangeTarget(GetFrameSinkId());
  return video_capturer;
}

base::string16 RenderWidgetHostViewBase::GetSelectedText() {
  if (!GetTextInputManager())
    return base::string16();
  return GetTextInputManager()->GetTextSelection(this)->selected_text();
}

bool RenderWidgetHostViewBase::IsMouseLocked() {
  return mouse_locked_;
}

bool RenderWidgetHostViewBase::LockKeyboard(
    base::Optional<base::flat_set<int>> keys) {
  NOTIMPLEMENTED();
  return false;
}

void RenderWidgetHostViewBase::UnlockKeyboard() {
  NOTIMPLEMENTED();
}

bool RenderWidgetHostViewBase::IsKeyboardLocked() {
  return keyboard_locked_;
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
  return nullptr;
}

void RenderWidgetHostViewBase::AccessibilityShowMenu(const gfx::Point& point) {
  if (host())
    host()->ShowContextMenuAtPoint(point, ui::MENU_SOURCE_NONE);
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
  return nullptr;
}

void RenderWidgetHostViewBase::UpdateScreenInfo(gfx::NativeView view) {
  if (host() && host()->delegate())
    host()->delegate()->SendScreenRects();

  if (HasDisplayPropertyChanged(view) && host()) {
    OnSynchronizedDisplayPropertiesChanged();
    host()->NotifyScreenInfoChanged();
  }
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

bool RenderWidgetHostViewBase::IsScrollOffsetAtTop() const {
  return is_scroll_offset_at_top_;
}

viz::ScopedSurfaceIdAllocator RenderWidgetHostViewBase::ResizeDueToAutoResize(
    const gfx::Size& new_size,
    uint64_t sequence_number) {
  // TODO(cblume): This doesn't currently suppress allocation.
  // It maintains existing behavior while using the suppression style.
  // This will be addressed in a follow-up patch.
  // See https://crbug.com/805073
  base::OnceCallback<void()> allocation_task =
      base::BindOnce(&RenderWidgetHostViewBase::OnResizeDueToAutoResizeComplete,
                     weak_factory_.GetWeakPtr(), sequence_number);
  return viz::ScopedSurfaceIdAllocator(std::move(allocation_task));
}

bool RenderWidgetHostViewBase::IsLocalSurfaceIdAllocationSuppressed() const {
  return false;
}

base::WeakPtr<RenderWidgetHostViewBase> RenderWidgetHostViewBase::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

std::unique_ptr<SyntheticGestureTarget>
RenderWidgetHostViewBase::CreateSyntheticGestureTarget() {
  return std::unique_ptr<SyntheticGestureTarget>(
      new SyntheticGestureTargetBase(host()));
}

void RenderWidgetHostViewBase::FocusedNodeTouched(
    bool editable) {
  DVLOG(1) << "FocusedNodeTouched: " << editable;
}

void RenderWidgetHostViewBase::GetScreenInfo(ScreenInfo* screen_info) const {
  DisplayUtil::GetNativeViewScreenInfo(screen_info, GetNativeView());
}

float RenderWidgetHostViewBase::GetDeviceScaleFactor() const {
  ScreenInfo screen_info;
  GetScreenInfo(&screen_info);
  return screen_info.device_scale_factor;
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

void RenderWidgetHostViewBase::OnDidNavigateMainFrameToNewPage() {
}

void RenderWidgetHostViewBase::OnFrameTokenChangedForView(
    uint32_t frame_token) {
  if (host())
    host()->DidProcessFrame(frame_token);
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
    gfx::PointF* transformed_point,
    bool* out_query_renderer) {
  float device_scale_factor = ui::GetScaleFactorForNativeView(GetNativeView());
  DCHECK(device_scale_factor != 0.0f);

  // The surface hittest happens in device pixels, so we need to convert the
  // |point| from DIPs to pixels before hittesting.
  gfx::PointF point_in_pixels =
      gfx::ConvertPointToPixel(device_scale_factor, point);
  viz::SurfaceId surface_id = GetCurrentSurfaceId();
  if (!surface_id.is_valid()) {
    return GetFrameSinkId();
  }
  viz::SurfaceHittest hittest(delegate,
                              GetFrameSinkManager()->surface_manager());
  gfx::Transform target_transform;
  viz::SurfaceId target_local_surface_id = hittest.GetTargetSurfaceAtPoint(
      surface_id, gfx::ToFlooredPoint(point_in_pixels), &target_transform,
      out_query_renderer);
  *transformed_point = point_in_pixels;
  if (target_local_surface_id.is_valid()) {
    target_transform.TransformPoint(transformed_point);
  }
  *transformed_point =
      gfx::ConvertPointToDIP(device_scale_factor, *transformed_point);
  // It is possible that the renderer has not yet produced a surface, in which
  // case we return our current FrameSinkId.
  auto frame_sink_id = target_local_surface_id.frame_sink_id();
  return frame_sink_id.is_valid() ? frame_sink_id : GetFrameSinkId();
}

void RenderWidgetHostViewBase::ProcessMouseEvent(
    const blink::WebMouseEvent& event,
    const ui::LatencyInfo& latency) {
  PreProcessMouseEvent(event);
  host()->ForwardMouseEventWithLatencyInfo(event, latency);
}

void RenderWidgetHostViewBase::ProcessMouseWheelEvent(
    const blink::WebMouseWheelEvent& event,
    const ui::LatencyInfo& latency) {
  host()->ForwardWheelEventWithLatencyInfo(event, latency);
}

void RenderWidgetHostViewBase::ProcessTouchEvent(
    const blink::WebTouchEvent& event,
    const ui::LatencyInfo& latency) {
  PreProcessTouchEvent(event);
  host()->ForwardTouchEventWithLatencyInfo(event, latency);
}

void RenderWidgetHostViewBase::ProcessGestureEvent(
    const blink::WebGestureEvent& event,
    const ui::LatencyInfo& latency) {
  host()->ForwardGestureEventWithLatencyInfo(event, latency);
}

gfx::PointF RenderWidgetHostViewBase::TransformPointToRootCoordSpaceF(
    const gfx::PointF& point) {
  return point;
}

gfx::PointF RenderWidgetHostViewBase::TransformRootPointToViewCoordSpace(
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

bool RenderWidgetHostViewBase::HasSize() const {
  return true;
}

void RenderWidgetHostViewBase::Destroy() {
  if (host_) {
    host_->render_frame_metadata_provider()->RemoveObserver(this);
    host_ = nullptr;
  }
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

  if (!host() || !host()->delegate())
    return nullptr;

  // This RWHV needs to be registered with the TextInputManager so that the
  // TextInputManager starts tracking its state, and observing its lifetime.
  text_input_manager_ = host()->delegate()->GetTextInputManager();
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
  pending_embeds_.erase(routing_id);
  // Tests may not create |render_widget_window_tree_client_| (tests don't
  // necessarily create RenderWidgetHostViewAura).
  if (render_widget_window_tree_client_)
    render_widget_window_tree_client_->DestroyFrame(routing_id);
}
#endif

#if defined(USE_AURA)
void RenderWidgetHostViewBase::OnDidScheduleEmbed(
    int routing_id,
    int embed_id,
    const base::UnguessableToken& token) {
  auto iter = pending_embeds_.find(routing_id);
  if (iter == pending_embeds_.end() || iter->second != embed_id)
    return;
  pending_embeds_.erase(iter);
  // Tests may not create |render_widget_window_tree_client_| (tests don't
  // necessarily create RenderWidgetHostViewAura).
  if (render_widget_window_tree_client_)
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

void RenderWidgetHostViewBase::OnResizeDueToAutoResizeComplete(
    uint64_t sequence_number) {
  if (host())
    host()->DidAllocateLocalSurfaceIdForAutoResize(sequence_number);
}

#if defined(OS_MACOSX)
bool RenderWidgetHostViewBase::ShouldContinueToPauseForFrame() {
  return false;
}
#endif

}  // namespace content
