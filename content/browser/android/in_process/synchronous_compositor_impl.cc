// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/in_process/synchronous_compositor_impl.h"

#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/lazy_instance.h"
#include "base/message_loop/message_loop.h"
#include "content/browser/android/in_process/synchronous_compositor_external_begin_frame_source.h"
#include "content/browser/android/in_process/synchronous_compositor_factory_impl.h"
#include "content/browser/android/in_process/synchronous_compositor_registry.h"
#include "content/browser/android/in_process/synchronous_input_event_filter.h"
#include "content/browser/gpu/gpu_process_host.h"
#include "content/browser/renderer_host/render_widget_host_view_android.h"
#include "content/common/input/did_overscroll_params.h"
#include "content/common/input_messages.h"
#include "content/public/browser/android/synchronous_compositor_client.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "ui/gfx/geometry/scroll_offset.h"
#include "ui/gl/gl_surface.h"

namespace content {

namespace {

int GetInProcessRendererId() {
  content::RenderProcessHost::iterator it =
      content::RenderProcessHost::AllHostsIterator();
  if (it.IsAtEnd()) {
    // There should always be one RPH in single process mode.
    NOTREACHED();
    return 0;
  }

  int id = it.GetCurrentValue()->GetID();
  it.Advance();
  DCHECK(it.IsAtEnd());  // Not multiprocess compatible.
  return id;
}

base::LazyInstance<SynchronousCompositorFactoryImpl>::Leaky g_factory =
    LAZY_INSTANCE_INITIALIZER;

base::Thread* CreateInProcessGpuThreadForSynchronousCompositor(
    const InProcessChildThreadParams& params) {
  return g_factory.Get().CreateInProcessGpuThread(params);
}

}  // namespace

DEFINE_WEB_CONTENTS_USER_DATA_KEY(SynchronousCompositorImpl);

// static
SynchronousCompositorImpl* SynchronousCompositorImpl::FromID(int process_id,
                                                             int routing_id) {
  if (g_factory == nullptr)
    return nullptr;
  RenderViewHost* rvh = RenderViewHost::FromID(process_id, routing_id);
  if (!rvh)
    return nullptr;
  WebContents* contents = WebContents::FromRenderViewHost(rvh);
  if (!contents)
    return nullptr;
  return FromWebContents(contents);
}

SynchronousCompositorImpl* SynchronousCompositorImpl::FromRoutingID(
    int routing_id) {
  return FromID(GetInProcessRendererId(), routing_id);
}

SynchronousCompositorImpl::SynchronousCompositorImpl(WebContents* contents)
    : compositor_client_(nullptr),
      output_surface_(nullptr),
      begin_frame_source_(nullptr),
      contents_(contents),
      routing_id_(contents->GetRoutingID()),
      synchronous_input_handler_proxy_(nullptr),
      registered_with_client_(false),
      is_active_(true),
      renderer_needs_begin_frames_(false),
      need_animate_input_(false),
      weak_ptr_factory_(this) {
  DCHECK(contents);
  DCHECK_NE(routing_id_, MSG_ROUTING_NONE);
}

SynchronousCompositorImpl::~SynchronousCompositorImpl() {
  DCHECK(!output_surface_);
  DCHECK(!begin_frame_source_);
  DCHECK(!synchronous_input_handler_proxy_);
}

void SynchronousCompositorImpl::SetClient(
    SynchronousCompositorClient* compositor_client) {
  DCHECK(CalledOnValidThread());
  DCHECK_IMPLIES(compositor_client, !compositor_client_);
  DCHECK_IMPLIES(!compositor_client, compositor_client_);

  if (!compositor_client) {
    SynchronousCompositorRegistry::GetInstance()->UnregisterCompositor(
        routing_id_, this);
  }

  compositor_client_ = compositor_client;

  // SetClient is essentially the constructor and destructor of
  // SynchronousCompositorImpl.
  if (compositor_client_) {
    SynchronousCompositorRegistry::GetInstance()->RegisterCompositor(
        routing_id_, this);
  }
}

void SynchronousCompositorImpl::RegisterWithClient() {
  DCHECK(CalledOnValidThread());
  DCHECK(compositor_client_);
  DCHECK(output_surface_);
  DCHECK(synchronous_input_handler_proxy_);
  DCHECK(!registered_with_client_);
  registered_with_client_ = true;

  compositor_client_->DidInitializeCompositor(this);

  output_surface_->SetTreeActivationCallback(
    base::Bind(&SynchronousCompositorImpl::DidActivatePendingTree,
               weak_ptr_factory_.GetWeakPtr()));

  // This disables the input system from animating inputs autonomously, instead
  // routing all input animations through the SynchronousInputHandler, which is
  // |this| class. Calling this causes an UpdateRootLayerState() immediately so,
  // do it after setting the client.
  synchronous_input_handler_proxy_->SetOnlySynchronouslyAnimateRootFlings(this);
}

// static
void SynchronousCompositor::SetGpuService(
    scoped_refptr<gpu::InProcessCommandBuffer::Service> service) {
  g_factory.Get().SetDeferredGpuService(service);
  GpuProcessHost::RegisterGpuMainThreadFactory(
      CreateInProcessGpuThreadForSynchronousCompositor);
}

// static
void SynchronousCompositor::SetUseIpcCommandBuffer() {
  g_factory.Get().SetUseIpcCommandBuffer();
}

void SynchronousCompositorImpl::DidInitializeRendererObjects(
    SynchronousCompositorOutputSurface* output_surface,
    SynchronousCompositorExternalBeginFrameSource* begin_frame_source,
    SynchronousInputHandlerProxy* synchronous_input_handler_proxy) {
  DCHECK(!output_surface_);
  DCHECK(!begin_frame_source_);
  DCHECK(output_surface);
  DCHECK(begin_frame_source);
  DCHECK(compositor_client_);
  DCHECK(synchronous_input_handler_proxy);

  output_surface_ = output_surface;
  begin_frame_source_ = begin_frame_source;
  synchronous_input_handler_proxy_ = synchronous_input_handler_proxy;

  output_surface_->SetCompositor(this);
  begin_frame_source_->SetCompositor(this);
}

void SynchronousCompositorImpl::DidDestroyRendererObjects() {
  DCHECK(output_surface_);
  DCHECK(begin_frame_source_);
  DCHECK(compositor_client_);

  if (registered_with_client_) {
    output_surface_->SetTreeActivationCallback(base::Closure());
    compositor_client_->DidDestroyCompositor(this);
    registered_with_client_ = false;
  }

  // This object is being destroyed, so remove pointers to it.
  begin_frame_source_->SetCompositor(nullptr);
  output_surface_->SetCompositor(nullptr);
  synchronous_input_handler_proxy_->SetOnlySynchronouslyAnimateRootFlings(
      nullptr);

  synchronous_input_handler_proxy_ = nullptr;
  begin_frame_source_ = nullptr;
  output_surface_ = nullptr;
  // Don't propogate this signal from one renderer to the next.
  need_animate_input_ = false;
}

scoped_ptr<cc::CompositorFrame> SynchronousCompositorImpl::DemandDrawHw(
    gfx::Size surface_size,
    const gfx::Transform& transform,
    gfx::Rect viewport,
    gfx::Rect clip,
    gfx::Rect viewport_rect_for_tile_priority,
    const gfx::Transform& transform_for_tile_priority) {
  DCHECK(CalledOnValidThread());
  DCHECK(output_surface_);
  DCHECK(compositor_client_);
  DCHECK(begin_frame_source_);

  scoped_ptr<cc::CompositorFrame> frame =
      output_surface_->DemandDrawHw(surface_size,
                                    transform,
                                    viewport,
                                    clip,
                                    viewport_rect_for_tile_priority,
                                    transform_for_tile_priority);

  if (frame.get())
    UpdateFrameMetaData(frame->metadata);

  return frame.Pass();
}

void SynchronousCompositorImpl::ReturnResources(
    const cc::CompositorFrameAck& frame_ack) {
  DCHECK(CalledOnValidThread());
  output_surface_->ReturnResources(frame_ack);
}

bool SynchronousCompositorImpl::DemandDrawSw(SkCanvas* canvas) {
  DCHECK(CalledOnValidThread());
  DCHECK(output_surface_);
  DCHECK(compositor_client_);
  DCHECK(begin_frame_source_);

  scoped_ptr<cc::CompositorFrame> frame =
      output_surface_->DemandDrawSw(canvas);

  if (frame.get())
    UpdateFrameMetaData(frame->metadata);

  return !!frame.get();
}

void SynchronousCompositorImpl::UpdateFrameMetaData(
    const cc::CompositorFrameMetadata& frame_metadata) {
  RenderWidgetHostViewAndroid* rwhv = static_cast<RenderWidgetHostViewAndroid*>(
      contents_->GetRenderWidgetHostView());
  if (rwhv)
    rwhv->SynchronousFrameMetadata(frame_metadata);
  DeliverMessages();
}

void SynchronousCompositorImpl::SetMemoryPolicy(size_t bytes_limit) {
  DCHECK(CalledOnValidThread());
  DCHECK(output_surface_);

  size_t current_bytes_limit = output_surface_->GetMemoryPolicy();
  output_surface_->SetMemoryPolicy(bytes_limit);

  if (bytes_limit && !current_bytes_limit) {
    g_factory.Get().CompositorInitializedHardwareDraw();
  } else if (!bytes_limit && current_bytes_limit) {
    g_factory.Get().CompositorReleasedHardwareDraw();
  }
}

void SynchronousCompositorImpl::PostInvalidate() {
  DCHECK(CalledOnValidThread());
  DCHECK(compositor_client_);
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

void SynchronousCompositorImpl::SetIsActive(bool is_active) {
  TRACE_EVENT1("cc", "SynchronousCompositorImpl::SetIsActive", "is_active",
               is_active);
  is_active_ = is_active;
  UpdateNeedsBeginFrames();
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
  RenderWidgetHostViewAndroid* rwhv = static_cast<RenderWidgetHostViewAndroid*>(
      contents_->GetRenderWidgetHostView());
  if (rwhv)
    rwhv->OnSetNeedsBeginFrames(is_active_ && renderer_needs_begin_frames_);
}

void SynchronousCompositorImpl::DidOverscroll(
    const DidOverscrollParams& params) {
  DCHECK(compositor_client_);
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
  contents_->GetRenderProcessHost()->OnMessageReceived(
      InputHostMsg_DidStopFlinging(routing_id_));
}

InputEventAckState SynchronousCompositorImpl::HandleInputEvent(
    const blink::WebInputEvent& input_event) {
  DCHECK(CalledOnValidThread());
  return g_factory.Get().synchronous_input_event_filter()->HandleInputEvent(
      contents_->GetRoutingID(), input_event);
}

void SynchronousCompositorImpl::DeliverMessages() {
  ScopedVector<IPC::Message> messages;
  output_surface_->GetMessagesToDeliver(&messages);
  RenderProcessHost* rph = contents_->GetRenderProcessHost();
  for (ScopedVector<IPC::Message>::const_iterator i = messages.begin();
       i != messages.end();
       ++i) {
    rph->OnMessageReceived(**i);
  }
}

void SynchronousCompositorImpl::DidActivatePendingTree() {
  DCHECK(compositor_client_);
  if (registered_with_client_)
    compositor_client_->DidUpdateContent();
  DeliverMessages();
}

void SynchronousCompositorImpl::SetNeedsSynchronousAnimateInput() {
  DCHECK(CalledOnValidThread());
  DCHECK(compositor_client_);
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
  DCHECK(compositor_client_);

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

// static
void SynchronousCompositor::SetClientForWebContents(
    WebContents* contents,
    SynchronousCompositorClient* client) {
  DCHECK(contents);
  if (client) {
    g_factory.Get();  // Ensure it's initialized.
    SynchronousCompositorImpl::CreateForWebContents(contents);
  }
  SynchronousCompositorImpl* instance =
      SynchronousCompositorImpl::FromWebContents(contents);
  DCHECK(instance);
  instance->SetClient(client);
}

}  // namespace content
