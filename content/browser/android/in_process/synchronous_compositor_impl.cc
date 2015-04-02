// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/in_process/synchronous_compositor_impl.h"

#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/lazy_instance.h"
#include "base/message_loop/message_loop.h"
#include "cc/input/input_handler.h"
#include "content/browser/android/in_process/synchronous_compositor_external_begin_frame_source.h"
#include "content/browser/android/in_process/synchronous_compositor_factory_impl.h"
#include "content/browser/android/in_process/synchronous_compositor_registry.h"
#include "content/browser/android/in_process/synchronous_input_event_filter.h"
#include "content/browser/renderer_host/render_widget_host_view_android.h"
#include "content/common/input/did_overscroll_params.h"
#include "content/public/browser/android/synchronous_compositor_client.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
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

}  // namespace

DEFINE_WEB_CONTENTS_USER_DATA_KEY(SynchronousCompositorImpl);

// static
SynchronousCompositorImpl* SynchronousCompositorImpl::FromID(int process_id,
                                                             int routing_id) {
  if (g_factory == NULL)
    return NULL;
  RenderViewHost* rvh = RenderViewHost::FromID(process_id, routing_id);
  if (!rvh)
    return NULL;
  WebContents* contents = WebContents::FromRenderViewHost(rvh);
  if (!contents)
    return NULL;
  return FromWebContents(contents);
}

SynchronousCompositorImpl* SynchronousCompositorImpl::FromRoutingID(
    int routing_id) {
  return FromID(GetInProcessRendererId(), routing_id);
}

SynchronousCompositorImpl::SynchronousCompositorImpl(WebContents* contents)
    : compositor_client_(NULL),
      output_surface_(NULL),
      begin_frame_source_(nullptr),
      contents_(contents),
      routing_id_(contents->GetRoutingID()),
      input_handler_(NULL),
      invoking_composite_(false),
      weak_ptr_factory_(this) {
  DCHECK(contents);
  DCHECK_NE(routing_id_, MSG_ROUTING_NONE);
  SynchronousCompositorRegistry::GetInstance()->RegisterCompositor(routing_id_,
                                                                   this);
}

SynchronousCompositorImpl::~SynchronousCompositorImpl() {
  SynchronousCompositorRegistry::GetInstance()->UnregisterCompositor(
      routing_id_, this);
  SetInputHandler(NULL);

  DCHECK(!output_surface_);
  DCHECK(!begin_frame_source_);
}

void SynchronousCompositorImpl::SetClient(
    SynchronousCompositorClient* compositor_client) {
  DCHECK(CalledOnValidThread());
  compositor_client_ = compositor_client;
}

// static
void SynchronousCompositor::SetGpuService(
    scoped_refptr<gpu::InProcessCommandBuffer::Service> service) {
  g_factory.Get().SetDeferredGpuService(service);
}

// static
void SynchronousCompositor::SetRecordFullDocument(bool record_full_document) {
  g_factory.Get().SetRecordFullDocument(record_full_document);
}

void SynchronousCompositorImpl::DidInitializeRendererObjects(
    SynchronousCompositorOutputSurface* output_surface,
    SynchronousCompositorExternalBeginFrameSource* begin_frame_source) {
  DCHECK(!output_surface_);
  DCHECK(!begin_frame_source_);
  DCHECK(output_surface);
  DCHECK(begin_frame_source);
  DCHECK(compositor_client_);

  output_surface_ = output_surface;
  begin_frame_source_ = begin_frame_source;

  begin_frame_source_->SetCompositor(this);
  output_surface_->SetBeginFrameSource(begin_frame_source_);
  output_surface_->SetTreeActivationCallback(
      base::Bind(&SynchronousCompositorImpl::DidActivatePendingTree,
                 weak_ptr_factory_.GetWeakPtr()));
  NeedsBeginFramesChanged();
  compositor_client_->DidInitializeCompositor(this);
}

void SynchronousCompositorImpl::DidDestroyRendererObjects() {
  DCHECK(output_surface_);
  DCHECK(begin_frame_source_);

  begin_frame_source_->SetCompositor(nullptr);
  output_surface_->SetBeginFrameSource(nullptr);
  if (compositor_client_)
    compositor_client_->DidDestroyCompositor(this);
  compositor_client_ = nullptr;
  output_surface_ = nullptr;
  begin_frame_source_ = nullptr;
}

void SynchronousCompositorImpl::NotifyDidDestroyCompositorToClient() {
  if (compositor_client_)
    compositor_client_->DidDestroyCompositor(this);
  compositor_client_ = nullptr;
}

bool SynchronousCompositorImpl::InitializeHwDraw() {
  DCHECK(CalledOnValidThread());
  DCHECK(output_surface_);

  scoped_refptr<cc::ContextProvider> onscreen_context =
      g_factory.Get().CreateContextProviderForCompositor();

  scoped_refptr<cc::ContextProvider> worker_context =
      g_factory.Get().CreateContextProviderForCompositor();

  bool success =
      output_surface_->InitializeHwDraw(onscreen_context, worker_context);

  if (success)
    g_factory.Get().CompositorInitializedHardwareDraw();
  return success;
}

void SynchronousCompositorImpl::ReleaseHwDraw() {
  DCHECK(CalledOnValidThread());
  DCHECK(output_surface_);
  output_surface_->ReleaseHwDraw();
  g_factory.Get().CompositorReleasedHardwareDraw();
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
  DCHECK(!invoking_composite_);
  DCHECK(compositor_client_);
  DCHECK(begin_frame_source_);

  base::AutoReset<bool> invoking_composite_resetter(&invoking_composite_,
                                                    true);
  scoped_ptr<cc::CompositorFrame> frame =
      output_surface_->DemandDrawHw(surface_size,
                                    transform,
                                    viewport,
                                    clip,
                                    viewport_rect_for_tile_priority,
                                    transform_for_tile_priority);
  if (frame.get())
    UpdateFrameMetaData(frame->metadata);

  compositor_client_->SetContinuousInvalidate(
      begin_frame_source_->NeedsBeginFrames());

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
  DCHECK(!invoking_composite_);
  DCHECK(compositor_client_);
  DCHECK(begin_frame_source_);

  base::AutoReset<bool> invoking_composite_resetter(&invoking_composite_,
                                                    true);
  scoped_ptr<cc::CompositorFrame> frame =
      output_surface_->DemandDrawSw(canvas);
  if (frame.get())
    UpdateFrameMetaData(frame->metadata);

  compositor_client_->SetContinuousInvalidate(
      begin_frame_source_->NeedsBeginFrames());

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

  output_surface_->SetMemoryPolicy(bytes_limit);
}

void SynchronousCompositorImpl::DidChangeRootLayerScrollOffset() {
  if (input_handler_)
    input_handler_->OnRootLayerDelegatedScrollOffsetChanged();
}

void SynchronousCompositorImpl::SetInputHandler(
    cc::InputHandler* input_handler) {
  DCHECK(CalledOnValidThread());

  if (input_handler_)
    input_handler_->SetRootLayerScrollOffsetDelegate(NULL);

  input_handler_ = input_handler;

  if (input_handler_)
    input_handler_->SetRootLayerScrollOffsetDelegate(this);
}

void SynchronousCompositorImpl::DidOverscroll(
    const DidOverscrollParams& params) {
  if (compositor_client_) {
    compositor_client_->DidOverscroll(params.accumulated_overscroll,
                                      params.latest_overscroll_delta,
                                      params.current_fling_velocity);
  }
}

void SynchronousCompositorImpl::DidStopFlinging() {
  RenderWidgetHostViewAndroid* rwhv = static_cast<RenderWidgetHostViewAndroid*>(
      contents_->GetRenderWidgetHostView());
  if (rwhv)
    rwhv->DidStopFlinging();
}

void SynchronousCompositorImpl::NeedsBeginFramesChanged() const {
  DCHECK(CalledOnValidThread());
  DCHECK(begin_frame_source_);
  if (invoking_composite_)
    return;

  if (compositor_client_) {
    compositor_client_->SetContinuousInvalidate(
        begin_frame_source_->NeedsBeginFrames());
  }
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
  if (compositor_client_)
    compositor_client_->DidUpdateContent();
}

gfx::ScrollOffset SynchronousCompositorImpl::GetTotalScrollOffset() {
  DCHECK(CalledOnValidThread());
  if (compositor_client_) {
    // TODO(miletus): Make GetTotalRootLayerScrollOffset return
    // ScrollOffset. crbug.com/414283.
    return gfx::ScrollOffset(
        compositor_client_->GetTotalRootLayerScrollOffset());
  }
  return gfx::ScrollOffset();
}

bool SynchronousCompositorImpl::IsExternalFlingActive() const {
  DCHECK(CalledOnValidThread());
  if (compositor_client_)
    return compositor_client_->IsExternalFlingActive();
  return false;
}

void SynchronousCompositorImpl::UpdateRootLayerState(
    const gfx::ScrollOffset& total_scroll_offset,
    const gfx::ScrollOffset& max_scroll_offset,
    const gfx::SizeF& scrollable_size,
    float page_scale_factor,
    float min_page_scale_factor,
    float max_page_scale_factor) {
  DCHECK(CalledOnValidThread());
  if (!compositor_client_)
    return;

  // TODO(miletus): Pass in ScrollOffset. crbug.com/414283.
  compositor_client_->UpdateRootLayerState(
      gfx::ScrollOffsetToVector2dF(total_scroll_offset),
      gfx::ScrollOffsetToVector2dF(max_scroll_offset),
      scrollable_size,
      page_scale_factor,
      min_page_scale_factor,
      max_page_scale_factor);
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
