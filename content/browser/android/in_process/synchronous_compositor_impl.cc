// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/in_process/synchronous_compositor_impl.h"

#include <utility>

#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/lazy_instance.h"
#include "base/message_loop/message_loop.h"
#include "content/browser/android/in_process/synchronous_compositor_factory_impl.h"
#include "content/browser/android/in_process/synchronous_compositor_registry_in_proc.h"
#include "content/browser/android/in_process/synchronous_input_event_filter.h"
#include "content/browser/gpu/gpu_process_host.h"
#include "content/browser/renderer_host/render_widget_host_view_android.h"
#include "content/common/input/did_overscroll_params.h"
#include "content/common/input_messages.h"
#include "content/public/browser/android/synchronous_compositor_client.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/common/child_process_host.h"
#include "ui/gfx/geometry/scroll_offset.h"
#include "ui/gl/gl_surface.h"

namespace content {

namespace {

int g_process_id = ChildProcessHost::kInvalidUniqueID;

base::LazyInstance<SynchronousCompositorFactoryImpl>::Leaky g_factory =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

SynchronousCompositorImpl* SynchronousCompositorImpl::FromRoutingID(
    int routing_id) {
  if (g_factory == nullptr)
    return nullptr;
  if (g_process_id == ChildProcessHost::kInvalidUniqueID)
    return nullptr;
  RenderViewHost* rvh = RenderViewHost::FromID(g_process_id, routing_id);
  if (!rvh)
    return nullptr;
  RenderWidgetHostViewAndroid* rwhva =
      static_cast<RenderWidgetHostViewAndroid*>(rvh->GetWidget()->GetView());
  if (!rwhva)
    return nullptr;
  return static_cast<SynchronousCompositorImpl*>(
      rwhva->GetSynchronousCompositor());
}

SynchronousCompositorImpl::SynchronousCompositorImpl(
    RenderWidgetHostViewAndroid* rwhva,
    SynchronousCompositorClient* client)
    : rwhva_(rwhva),
      routing_id_(rwhva_->GetRenderWidgetHost()->GetRoutingID()),
      compositor_client_(client),
      output_surface_(nullptr),
      begin_frame_source_(nullptr),
      synchronous_input_handler_proxy_(nullptr),
      registered_with_client_(false),
      is_active_(true),
      renderer_needs_begin_frames_(false),
      need_animate_input_(false),
      weak_ptr_factory_(this) {
  DCHECK_NE(routing_id_, MSG_ROUTING_NONE);
  g_factory.Get();  // Ensure it's initialized.

  int process_id = rwhva_->GetRenderWidgetHost()->GetProcess()->GetID();
  if (g_process_id == ChildProcessHost::kInvalidUniqueID) {
    g_process_id = process_id;
  } else {
    DCHECK_EQ(g_process_id, process_id);  // Not multiprocess compatible.
  }

  SynchronousCompositorRegistryInProc::GetInstance()->RegisterCompositor(
      routing_id_, this);
}

SynchronousCompositorImpl::~SynchronousCompositorImpl() {
  SynchronousCompositorRegistryInProc::GetInstance()->UnregisterCompositor(
      routing_id_, this);
}

void SynchronousCompositorImpl::RegisterWithClient() {
  DCHECK(CalledOnValidThread());
  DCHECK(output_surface_);
  DCHECK(synchronous_input_handler_proxy_);
  DCHECK(!registered_with_client_);
  registered_with_client_ = true;

  compositor_client_->DidInitializeCompositor(this);
  compositor_client_->DidBecomeCurrent(this);

  output_surface_->SetTreeActivationCallback(
    base::Bind(&SynchronousCompositorImpl::DidActivatePendingTree,
               weak_ptr_factory_.GetWeakPtr()));

  // This disables the input system from animating inputs autonomously, instead
  // routing all input animations through the SynchronousInputHandler, which is
  // |this| class. Calling this causes an UpdateRootLayerState() immediately so,
  // do it after setting the client.
  synchronous_input_handler_proxy_->SetOnlySynchronouslyAnimateRootFlings(this);
}

void SynchronousCompositorImpl::DidInitializeRendererObjects(
    SynchronousCompositorOutputSurface* output_surface,
    SynchronousCompositorExternalBeginFrameSource* begin_frame_source,
    ui::SynchronousInputHandlerProxy* synchronous_input_handler_proxy) {
  DCHECK(!output_surface_);
  DCHECK(!begin_frame_source_);
  DCHECK(output_surface);
  DCHECK(begin_frame_source);
  DCHECK(synchronous_input_handler_proxy);

  output_surface_ = output_surface;
  begin_frame_source_ = begin_frame_source;
  synchronous_input_handler_proxy_ = synchronous_input_handler_proxy;

  output_surface_->SetSyncClient(this);
  begin_frame_source_->SetClient(this);
  begin_frame_source_->SetBeginFrameSourcePaused(!is_active_);
}

void SynchronousCompositorImpl::DidDestroyRendererObjects() {
  DCHECK(output_surface_);
  DCHECK(begin_frame_source_);

  if (registered_with_client_) {
    output_surface_->SetTreeActivationCallback(base::Closure());
    compositor_client_->DidDestroyCompositor(this);
    registered_with_client_ = false;
  }

  // This object is being destroyed, so remove pointers to it.
  begin_frame_source_->SetClient(nullptr);
  output_surface_->SetSyncClient(nullptr);
  synchronous_input_handler_proxy_->SetOnlySynchronouslyAnimateRootFlings(
      nullptr);

  synchronous_input_handler_proxy_ = nullptr;
  begin_frame_source_ = nullptr;
  output_surface_ = nullptr;
  // Don't propogate this signal from one renderer to the next.
  need_animate_input_ = false;
}

SynchronousCompositor::Frame SynchronousCompositorImpl::DemandDrawHw(
    const gfx::Size& surface_size,
    const gfx::Transform& transform,
    const gfx::Rect& viewport,
    const gfx::Rect& clip,
    const gfx::Rect& viewport_rect_for_tile_priority,
    const gfx::Transform& transform_for_tile_priority) {
  DCHECK(CalledOnValidThread());
  DCHECK(output_surface_);
  DCHECK(begin_frame_source_);
  DCHECK(!frame_holder_.frame);

  output_surface_->DemandDrawHw(surface_size, transform, viewport, clip,
                                viewport_rect_for_tile_priority,
                                transform_for_tile_priority);

  if (frame_holder_.frame)
    UpdateFrameMetaData(frame_holder_.frame->metadata);

  return std::move(frame_holder_);
}

void SynchronousCompositorImpl::ReturnResources(
    uint32_t output_surface_id,
    const cc::CompositorFrameAck& frame_ack) {
  DCHECK(CalledOnValidThread());
  output_surface_->ReturnResources(output_surface_id, frame_ack);
}

bool SynchronousCompositorImpl::DemandDrawSw(SkCanvas* canvas) {
  DCHECK(CalledOnValidThread());
  DCHECK(output_surface_);
  DCHECK(begin_frame_source_);
  DCHECK(!frame_holder_.frame);

  output_surface_->DemandDrawSw(canvas);

  bool success = !!frame_holder_.frame;
  if (frame_holder_.frame) {
    UpdateFrameMetaData(frame_holder_.frame->metadata);
    frame_holder_.frame.reset();
  }

  return success;
}

void SynchronousCompositorImpl::SwapBuffers(uint32_t output_surface_id,
                                            cc::CompositorFrame* frame) {
  DCHECK(!frame_holder_.frame);
  frame_holder_.output_surface_id = output_surface_id;
  frame_holder_.frame.reset(new cc::CompositorFrame);
  frame->AssignTo(frame_holder_.frame.get());
}

void SynchronousCompositorImpl::UpdateFrameMetaData(
    const cc::CompositorFrameMetadata& frame_metadata) {
  rwhva_->SynchronousFrameMetadata(frame_metadata);
  DeliverMessages();
}

void SynchronousCompositorImpl::SetMemoryPolicy(size_t bytes_limit) {
  DCHECK(CalledOnValidThread());
  DCHECK(output_surface_);
  output_surface_->SetMemoryPolicy(bytes_limit);
}

void SynchronousCompositorImpl::Invalidate() {
  DCHECK(CalledOnValidThread());
  if (registered_with_client_)
    compositor_client_->PostInvalidate();
}

void SynchronousCompositorImpl::DidChangeRootLayerScrollOffset(
    const gfx::ScrollOffset& root_offset) {
  DCHECK(CalledOnValidThread());
  if (!synchronous_input_handler_proxy_)
    return;
  synchronous_input_handler_proxy_->SynchronouslySetRootScrollOffset(
      root_offset);
}

void SynchronousCompositorImpl::SynchronouslyZoomBy(float zoom_delta,
                                                    const gfx::Point& anchor) {
  DCHECK(CalledOnValidThread());
  if (!synchronous_input_handler_proxy_)
    return;
  synchronous_input_handler_proxy_->SynchronouslyZoomBy(zoom_delta, anchor);
}

void SynchronousCompositorImpl::SetIsActive(bool is_active) {
  TRACE_EVENT1("cc", "SynchronousCompositorImpl::SetIsActive", "is_active",
               is_active);
  if (is_active_ == is_active)
    return;

  is_active_ = is_active;
  UpdateNeedsBeginFrames();
  if (begin_frame_source_)
    begin_frame_source_->SetBeginFrameSourcePaused(!is_active_);
}

void SynchronousCompositorImpl::OnComputeScroll(
    base::TimeTicks animation_time) {
  if (need_animate_input_) {
    need_animate_input_ = false;
    synchronous_input_handler_proxy_->SynchronouslyAnimate(animation_time);
  }
}

void SynchronousCompositorImpl::OnNeedsBeginFramesChange(
    bool needs_begin_frames) {
  renderer_needs_begin_frames_ = needs_begin_frames;
  UpdateNeedsBeginFrames();
}

void SynchronousCompositorImpl::BeginFrame(const cc::BeginFrameArgs& args) {
  if (!registered_with_client_ && is_active_ && renderer_needs_begin_frames_) {
    // Make sure this is a BeginFrame that renderer side explicitly requested.
    // Otherwise it is possible renderer objects not initialized.
    RegisterWithClient();
    DCHECK(registered_with_client_);
  }
  if (begin_frame_source_)
    begin_frame_source_->BeginFrame(args);
}

void SynchronousCompositorImpl::UpdateNeedsBeginFrames() {
  rwhva_->OnSetNeedsBeginFrames(is_active_ && renderer_needs_begin_frames_);
}

void SynchronousCompositorImpl::DidOverscrollInProcess(
    const DidOverscrollParams& params) {
  if (registered_with_client_) {
    compositor_client_->DidOverscroll(params.accumulated_overscroll,
                                      params.latest_overscroll_delta,
                                      params.current_fling_velocity);
  }
}

void SynchronousCompositorImpl::DidStopFlinging() {
  // It's important that the fling-end notification follow the same path as it
  // takes on other platforms (using an IPC). This ensures consistent
  // bookkeeping at all stages of the input pipeline.
  rwhva_->GetRenderWidgetHost()->GetProcess()->OnMessageReceived(
      InputHostMsg_DidStopFlinging(routing_id_));
}

InputEventAckState SynchronousCompositorImpl::HandleInputEvent(
    const blink::WebInputEvent& input_event) {
  DCHECK(CalledOnValidThread());
  return g_factory.Get().synchronous_input_event_filter()->HandleInputEvent(
      routing_id_, input_event);
}

void SynchronousCompositorImpl::DidOverscroll(
    const DidOverscrollParams& params) {
  // SynchronousCompositorImpl uses synchronous DidOverscrollInProcess for
  // overscroll instead of this async path.
  NOTREACHED();
}

bool SynchronousCompositorImpl::OnMessageReceived(const IPC::Message& message) {
  NOTREACHED();
  return false;
}

void SynchronousCompositorImpl::DidBecomeCurrent() {
  // This is single process synchronous compositor. There is only one
  // RenderViewHost.  DidBecomeCurrent could be called before the renderer
  // objects are initialized. So hold off calling DidBecomeCurrent until
  // RegisterWithClient. Intentional no-op here.
}

void SynchronousCompositorImpl::DeliverMessages() {
  std::vector<std::unique_ptr<IPC::Message>> messages;
  output_surface_->GetMessagesToDeliver(&messages);
  RenderProcessHost* rph = rwhva_->GetRenderWidgetHost()->GetProcess();
  for (const auto& msg : messages) {
    rph->OnMessageReceived(*msg);
  }
}

void SynchronousCompositorImpl::DidActivatePendingTree() {
  if (registered_with_client_)
    compositor_client_->DidUpdateContent();
  DeliverMessages();
}

void SynchronousCompositorImpl::SetNeedsSynchronousAnimateInput() {
  DCHECK(CalledOnValidThread());
  if (!registered_with_client_)
    return;
  need_animate_input_ = true;
  compositor_client_->PostInvalidate();
}

void SynchronousCompositorImpl::UpdateRootLayerState(
    const gfx::ScrollOffset& total_scroll_offset,
    const gfx::ScrollOffset& max_scroll_offset,
    const gfx::SizeF& scrollable_size,
    float page_scale_factor,
    float min_page_scale_factor,
    float max_page_scale_factor) {
  DCHECK(CalledOnValidThread());

  if (registered_with_client_) {
    // TODO(miletus): Pass in ScrollOffset. crbug.com/414283.
    compositor_client_->UpdateRootLayerState(
        gfx::ScrollOffsetToVector2dF(total_scroll_offset),
        gfx::ScrollOffsetToVector2dF(max_scroll_offset),
        scrollable_size,
        page_scale_factor,
        min_page_scale_factor,
        max_page_scale_factor);
  }
}

// Not using base::NonThreadSafe as we want to enforce a more exacting threading
// requirement: SynchronousCompositorImpl() must only be used on the UI thread.
bool SynchronousCompositorImpl::CalledOnValidThread() const {
  return BrowserThread::CurrentlyOn(BrowserThread::UI);
}

}  // namespace content
