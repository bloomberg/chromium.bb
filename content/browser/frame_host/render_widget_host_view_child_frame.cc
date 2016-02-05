// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/frame_host/render_widget_host_view_child_frame.h"

#include <algorithm>
#include <utility>
#include <vector>

#include "build/build_config.h"
#include "cc/output/copy_output_request.h"
#include "cc/output/copy_output_result.h"
#include "cc/surfaces/surface.h"
#include "cc/surfaces/surface_factory.h"
#include "cc/surfaces/surface_manager.h"
#include "cc/surfaces/surface_sequence.h"
#include "content/browser/accessibility/browser_accessibility_manager.h"
#include "content/browser/browser_plugin/browser_plugin_guest.h"
#include "content/browser/compositor/surface_utils.h"
#include "content/browser/frame_host/cross_process_frame_connector.h"
#include "content/browser/gpu/compositor_util.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_delegate.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_input_event_router.h"
#include "content/common/gpu/gpu_messages.h"
#include "content/common/view_messages.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/browser_plugin_guest_mode.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gfx/geometry/size_f.h"

namespace content {

RenderWidgetHostViewChildFrame::RenderWidgetHostViewChildFrame(
    RenderWidgetHost* widget_host)
    : host_(RenderWidgetHostImpl::From(widget_host)),
      next_surface_sequence_(1u),
      last_output_surface_id_(0),
      current_surface_scale_factor_(1.f),
      ack_pending_count_(0),
      frame_connector_(nullptr),
      weak_factory_(this) {
  id_allocator_ = CreateSurfaceIdAllocator();
  if (host_->delegate() && host_->delegate()->GetInputEventRouter()) {
    host_->delegate()->GetInputEventRouter()->AddSurfaceIdNamespaceOwner(
        GetSurfaceIdNamespace(), this);
  }

  host_->SetView(this);
}

RenderWidgetHostViewChildFrame::~RenderWidgetHostViewChildFrame() {
  if (!surface_id_.is_null())
    surface_factory_->Destroy(surface_id_);
}

void RenderWidgetHostViewChildFrame::InitAsChild(
    gfx::NativeView parent_view) {
  NOTREACHED();
}

RenderWidgetHost* RenderWidgetHostViewChildFrame::GetRenderWidgetHost() const {
  return host_;
}

void RenderWidgetHostViewChildFrame::SetSize(const gfx::Size& size) {
  host_->WasResized();
}

void RenderWidgetHostViewChildFrame::SetBounds(const gfx::Rect& rect) {
  SetSize(rect.size());
}

void RenderWidgetHostViewChildFrame::Focus() {
}

bool RenderWidgetHostViewChildFrame::HasFocus() const {
  if (frame_connector_)
    return frame_connector_->HasFocus();
  return false;
}

bool RenderWidgetHostViewChildFrame::IsSurfaceAvailableForCopy() const {
  return surface_factory_ && !surface_id_.is_null();
}

void RenderWidgetHostViewChildFrame::Show() {
  if (!host_->is_hidden())
    return;
  host_->WasShown(ui::LatencyInfo());
}

void RenderWidgetHostViewChildFrame::Hide() {
  if (host_->is_hidden())
    return;
  host_->WasHidden();
}

bool RenderWidgetHostViewChildFrame::IsShowing() {
  return !host_->is_hidden();
}

gfx::Rect RenderWidgetHostViewChildFrame::GetViewBounds() const {
  gfx::Rect rect;
  if (frame_connector_)
    rect = frame_connector_->ChildFrameRect();
  return rect;
}

gfx::Vector2dF RenderWidgetHostViewChildFrame::GetLastScrollOffset() const {
  return last_scroll_offset_;
}

gfx::NativeView RenderWidgetHostViewChildFrame::GetNativeView() const {
  NOTREACHED();
  return NULL;
}

gfx::NativeViewId RenderWidgetHostViewChildFrame::GetNativeViewId() const {
  NOTREACHED();
  return 0;
}

gfx::NativeViewAccessible
RenderWidgetHostViewChildFrame::GetNativeViewAccessible() {
  NOTREACHED();
  return NULL;
}

void RenderWidgetHostViewChildFrame::SetBackgroundColor(SkColor color) {
  RenderWidgetHostViewBase::SetBackgroundColor(color);
  bool opaque = GetBackgroundOpaque();
  host_->SetBackgroundOpaque(opaque);
}

gfx::Size RenderWidgetHostViewChildFrame::GetPhysicalBackingSize() const {
  gfx::Size size;
  if (frame_connector_) {
    size = gfx::ScaleToCeiledSize(frame_connector_->ChildFrameRect().size(),
                                  frame_connector_->device_scale_factor());
  }
  return size;
}

void RenderWidgetHostViewChildFrame::InitAsPopup(
    RenderWidgetHostView* parent_host_view,
    const gfx::Rect& bounds) {
  NOTREACHED();
}

void RenderWidgetHostViewChildFrame::InitAsFullscreen(
    RenderWidgetHostView* reference_host_view) {
  NOTREACHED();
}

void RenderWidgetHostViewChildFrame::ImeCancelComposition() {
  // TODO(kenrb): Fix OOPIF Ime.
}

void RenderWidgetHostViewChildFrame::ImeCompositionRangeChanged(
    const gfx::Range& range,
    const std::vector<gfx::Rect>& character_bounds) {
  // TODO(kenrb): Fix OOPIF Ime.
}

void RenderWidgetHostViewChildFrame::MovePluginWindows(
    const std::vector<WebPluginGeometry>& moves) {
}

void RenderWidgetHostViewChildFrame::UpdateCursor(const WebCursor& cursor) {
  if (frame_connector_)
    frame_connector_->UpdateCursor(cursor);
}

void RenderWidgetHostViewChildFrame::SetIsLoading(bool is_loading) {
  // It is valid for an inner WebContents's SetIsLoading() to end up here.
  // This is because an inner WebContents's main frame's RenderWidgetHostView
  // is a RenderWidgetHostViewChildFrame. In contrast, when there is no
  // inner/outer WebContents, only subframe's RenderWidgetHostView can be a
  // RenderWidgetHostViewChildFrame which do not get a SetIsLoading() call.
  if (BrowserPluginGuestMode::UseCrossProcessFramesForGuests() &&
      BrowserPluginGuest::IsGuest(
          static_cast<RenderViewHostImpl*>(RenderViewHost::From(host_)))) {
    return;
  }

  NOTREACHED();
}

void RenderWidgetHostViewChildFrame::TextInputStateChanged(
    const ViewHostMsg_TextInputState_Params& params) {
  // TODO(kenrb): Implement.
}

void RenderWidgetHostViewChildFrame::RenderProcessGone(
    base::TerminationStatus status,
    int error_code) {
  if (frame_connector_)
    frame_connector_->RenderProcessGone();
  Destroy();
}

void RenderWidgetHostViewChildFrame::Destroy() {
  if (frame_connector_) {
    frame_connector_->set_view(NULL);
    frame_connector_ = NULL;
  }

  if (host_->delegate() && host_->delegate()->GetInputEventRouter()) {
    host_->delegate()->GetInputEventRouter()->RemoveSurfaceIdNamespaceOwner(
        GetSurfaceIdNamespace());
  }

  host_->SetView(NULL);
  host_ = NULL;
  base::MessageLoop::current()->DeleteSoon(FROM_HERE, this);
}

void RenderWidgetHostViewChildFrame::SetTooltipText(
    const base::string16& tooltip_text) {
}

void RenderWidgetHostViewChildFrame::SelectionChanged(
    const base::string16& text,
    size_t offset,
    const gfx::Range& range) {
}

void RenderWidgetHostViewChildFrame::SelectionBoundsChanged(
    const ViewHostMsg_SelectionBounds_Params& params) {
}

void RenderWidgetHostViewChildFrame::LockCompositingSurface() {
  NOTIMPLEMENTED();
}

void RenderWidgetHostViewChildFrame::UnlockCompositingSurface() {
  NOTIMPLEMENTED();
}

void RenderWidgetHostViewChildFrame::SurfaceDrawn(uint32_t output_surface_id,
                                                  cc::SurfaceDrawStatus drawn) {
  cc::CompositorFrameAck ack;
  DCHECK_GT(ack_pending_count_, 0U);
  if (!surface_returned_resources_.empty())
    ack.resources.swap(surface_returned_resources_);
  if (host_) {
    host_->Send(new ViewMsg_SwapCompositorFrameAck(host_->GetRoutingID(),
                                                   output_surface_id, ack));
  }
  ack_pending_count_--;
}

void RenderWidgetHostViewChildFrame::OnSwapCompositorFrame(
    uint32_t output_surface_id,
    scoped_ptr<cc::CompositorFrame> frame) {
  last_scroll_offset_ = frame->metadata.root_scroll_offset;

  if (!frame_connector_)
    return;

  cc::RenderPass* root_pass =
      frame->delegated_frame_data->render_pass_list.back().get();

  gfx::Size frame_size = root_pass->output_rect.size();
  float scale_factor = frame->metadata.device_scale_factor;

  // Check whether we need to recreate the cc::Surface, which means the child
  // frame renderer has changed its output surface, or size, or scale factor.
  if (output_surface_id != last_output_surface_id_ && surface_factory_) {
    surface_factory_->Destroy(surface_id_);
    surface_factory_.reset();
  }
  if (output_surface_id != last_output_surface_id_ ||
      frame_size != current_surface_size_ ||
      scale_factor != current_surface_scale_factor_) {
    ClearCompositorSurfaceIfNecessary();
    last_output_surface_id_ = output_surface_id;
    current_surface_size_ = frame_size;
    current_surface_scale_factor_ = scale_factor;
  }

  if (!surface_factory_) {
    cc::SurfaceManager* manager = GetSurfaceManager();
    surface_factory_ = make_scoped_ptr(new cc::SurfaceFactory(manager, this));
  }

  if (surface_id_.is_null()) {
    surface_id_ = id_allocator_->GenerateId();
    surface_factory_->Create(surface_id_);

    cc::SurfaceSequence sequence = cc::SurfaceSequence(
        id_allocator_->id_namespace(), next_surface_sequence_++);
    // The renderer process will satisfy this dependency when it creates a
    // SurfaceLayer.
    cc::SurfaceManager* manager = GetSurfaceManager();
    manager->GetSurfaceForId(surface_id_)->AddDestructionDependency(sequence);
    frame_connector_->SetChildFrameSurface(surface_id_, frame_size,
                                           scale_factor, sequence);
  }

  cc::SurfaceFactory::DrawCallback ack_callback =
      base::Bind(&RenderWidgetHostViewChildFrame::SurfaceDrawn, AsWeakPtr(),
                 output_surface_id);
  ack_pending_count_++;
  // If this value grows very large, something is going wrong.
  DCHECK_LT(ack_pending_count_, 1000U);
  surface_factory_->SubmitCompositorFrame(surface_id_, std::move(frame),
                                          ack_callback);

  ProcessFrameSwappedCallbacks();
}

void RenderWidgetHostViewChildFrame::ProcessFrameSwappedCallbacks() {
  // We only use callbacks once, therefore we make a new list for registration
  // before we start, and discard the old list entries when we are done.
  FrameSwappedCallbackList process_callbacks;
  process_callbacks.swap(frame_swapped_callbacks_);
  for (scoped_ptr<base::Closure>& callback : process_callbacks)
    callback->Run();
}

void RenderWidgetHostViewChildFrame::GetScreenInfo(
    blink::WebScreenInfo* results) {
  if (!frame_connector_)
    return;
  frame_connector_->GetScreenInfo(results);
}

bool RenderWidgetHostViewChildFrame::GetScreenColorProfile(
    std::vector<char>* color_profile) {
  if (!frame_connector_)
    return false;
  DCHECK(color_profile->empty());
  NOTIMPLEMENTED();
  return false;
}

gfx::Rect RenderWidgetHostViewChildFrame::GetBoundsInRootWindow() {
  // We do not have any root window specific parts in this view.
  return GetViewBounds();
}

#if defined(USE_AURA)
void RenderWidgetHostViewChildFrame::ProcessAckedTouchEvent(
    const TouchEventWithLatencyInfo& touch,
    InputEventAckState ack_result) {
}
#endif  // defined(USE_AURA)

bool RenderWidgetHostViewChildFrame::LockMouse() {
  return false;
}

void RenderWidgetHostViewChildFrame::UnlockMouse() {
}

uint32_t RenderWidgetHostViewChildFrame::GetSurfaceIdNamespace() {
  return id_allocator_->id_namespace();
}

void RenderWidgetHostViewChildFrame::ProcessKeyboardEvent(
    const NativeWebKeyboardEvent& event) {
  if (!host_)
    return;

  host_->ForwardKeyboardEvent(event);
}

void RenderWidgetHostViewChildFrame::ProcessMouseEvent(
    const blink::WebMouseEvent& event) {
  if (!host_)
    return;

  host_->ForwardMouseEvent(event);
}

void RenderWidgetHostViewChildFrame::ProcessMouseWheelEvent(
    const blink::WebMouseWheelEvent& event) {
  if (!host_)
    return;

  if (event.deltaX != 0 || event.deltaY != 0)
    host_->ForwardWheelEvent(event);
}

gfx::Point RenderWidgetHostViewChildFrame::TransformPointToRootCoordSpace(
    const gfx::Point& point) {
  if (!frame_connector_)
    return point;

  return frame_connector_->TransformPointToRootCoordSpace(point, surface_id_);
}

#if defined(OS_MACOSX)
void RenderWidgetHostViewChildFrame::SetActive(bool active) {
}

void RenderWidgetHostViewChildFrame::SetWindowVisibility(bool visible) {
}

void RenderWidgetHostViewChildFrame::WindowFrameChanged() {
}

void RenderWidgetHostViewChildFrame::ShowDefinitionForSelection() {
}

bool RenderWidgetHostViewChildFrame::SupportsSpeech() const {
  return false;
}

void RenderWidgetHostViewChildFrame::SpeakSelection() {
}

bool RenderWidgetHostViewChildFrame::IsSpeaking() const {
  return false;
}

void RenderWidgetHostViewChildFrame::StopSpeaking() {
}

bool RenderWidgetHostViewChildFrame::PostProcessEventForPluginIme(
      const NativeWebKeyboardEvent& event) {
  return false;
}
#endif  // defined(OS_MACOSX)

void RenderWidgetHostViewChildFrame::RegisterFrameSwappedCallback(
    scoped_ptr<base::Closure> callback) {
  frame_swapped_callbacks_.push_back(std::move(callback));
}

void RenderWidgetHostViewChildFrame::CopyFromCompositingSurface(
    const gfx::Rect& src_subrect,
    const gfx::Size& output_size,
    const ReadbackRequestCallback& callback,
    const SkColorType preferred_color_type) {
  if (!IsSurfaceAvailableForCopy()) {
    // Defer submitting the copy request until after a frame is drawn, at which
    // point we should be guaranteed that the surface is available.
    RegisterFrameSwappedCallback(make_scoped_ptr(new base::Closure(base::Bind(
        &RenderWidgetHostViewChildFrame::SubmitSurfaceCopyRequest, AsWeakPtr(),
        src_subrect, output_size, callback, preferred_color_type))));
    return;
  }

  SubmitSurfaceCopyRequest(src_subrect, output_size, callback,
                           preferred_color_type);
}

void RenderWidgetHostViewChildFrame::SubmitSurfaceCopyRequest(
    const gfx::Rect& src_subrect,
    const gfx::Size& output_size,
    const ReadbackRequestCallback& callback,
    const SkColorType preferred_color_type) {
  DCHECK(IsSurfaceAvailableForCopy());

  scoped_ptr<cc::CopyOutputRequest> request =
      cc::CopyOutputRequest::CreateRequest(
          base::Bind(&CopyFromCompositingSurfaceHasResult, output_size,
                     preferred_color_type, callback));
  if (!src_subrect.IsEmpty())
    request->set_area(src_subrect);

  surface_factory_->RequestCopyOfSurface(surface_id_, std::move(request));
}

void RenderWidgetHostViewChildFrame::CopyFromCompositingSurfaceToVideoFrame(
    const gfx::Rect& src_subrect,
    const scoped_refptr<media::VideoFrame>& target,
    const base::Callback<void(const gfx::Rect&, bool)>& callback) {
  NOTIMPLEMENTED();
  callback.Run(gfx::Rect(), false);
}

bool RenderWidgetHostViewChildFrame::CanCopyToVideoFrame() const {
  return false;
}

bool RenderWidgetHostViewChildFrame::HasAcceleratedSurface(
      const gfx::Size& desired_size) {
  return false;
}

#if defined(OS_WIN)
void RenderWidgetHostViewChildFrame::SetParentNativeViewAccessible(
    gfx::NativeViewAccessible accessible_parent) {
}

gfx::NativeViewId RenderWidgetHostViewChildFrame::GetParentForWindowlessPlugin()
    const {
  return NULL;
}
#endif  // defined(OS_WIN)

// cc::SurfaceFactoryClient implementation.
void RenderWidgetHostViewChildFrame::ReturnResources(
    const cc::ReturnedResourceArray& resources) {
  if (resources.empty())
    return;

  if (!ack_pending_count_ && host_) {
    cc::CompositorFrameAck ack;
    std::copy(resources.begin(), resources.end(),
              std::back_inserter(ack.resources));
    host_->Send(new ViewMsg_ReclaimCompositorResources(
        host_->GetRoutingID(), last_output_surface_id_, ack));
    return;
  }

  std::copy(resources.begin(), resources.end(),
            std::back_inserter(surface_returned_resources_));
}

void RenderWidgetHostViewChildFrame::SetBeginFrameSource(
    cc::SurfaceId surface_id,
    cc::BeginFrameSource* begin_frame_source) {
  // TODO(tansell): Hook this up.
}

BrowserAccessibilityManager*
RenderWidgetHostViewChildFrame::CreateBrowserAccessibilityManager(
    BrowserAccessibilityDelegate* delegate) {
  return BrowserAccessibilityManager::Create(
      BrowserAccessibilityManager::GetEmptyDocument(), delegate);
}

void RenderWidgetHostViewChildFrame::ClearCompositorSurfaceIfNecessary() {
  if (surface_factory_ && !surface_id_.is_null())
    surface_factory_->Destroy(surface_id_);
  surface_id_ = cc::SurfaceId();
}

}  // namespace content
