// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/synchronous_compositor_host.h"

#include <utility>

#include "base/containers/hash_tables.h"
#include "base/memory/shared_memory.h"
#include "base/trace_event/trace_event_argument.h"
#include "cc/output/compositor_frame_ack.h"
#include "content/browser/android/in_process/synchronous_compositor_renderer_statics.h"
#include "content/browser/renderer_host/render_widget_host_view_android.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/android/sync_compositor_messages.h"
#include "content/public/browser/android/synchronous_compositor_client.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_view_host.h"
#include "ipc/ipc_sender.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "third_party/skia/include/core/SkRect.h"
#include "ui/gfx/skia_util.h"

namespace content {

SynchronousCompositorHost::SynchronousCompositorHost(
    RenderWidgetHostViewAndroid* rwhva,
    SynchronousCompositorClient* client,
    bool async_input,
    bool use_in_proc_software_draw)
    : rwhva_(rwhva),
      client_(client),
      ui_task_runner_(
          BrowserThread::GetMessageLoopProxyForThread(BrowserThread::UI)),
      routing_id_(rwhva_->GetRenderWidgetHost()->GetRoutingID()),
      sender_(rwhva_->GetRenderWidgetHost()),
      async_input_(async_input),
      use_in_process_zero_copy_software_draw_(use_in_proc_software_draw),
      is_active_(false),
      bytes_limit_(0u),
      output_surface_id_from_last_draw_(0u),
      root_scroll_offset_updated_by_browser_(false),
      renderer_param_version_(0u),
      need_animate_scroll_(false),
      need_invalidate_count_(0u),
      need_begin_frame_(false),
      did_activate_pending_tree_count_(0u),
      weak_ptr_factory_(this) {
  client_->DidInitializeCompositor(this);
}

SynchronousCompositorHost::~SynchronousCompositorHost() {
  client_->DidDestroyCompositor(this);
  if (weak_ptr_factory_.HasWeakPtrs())
    UpdateStateTask();
}

bool SynchronousCompositorHost::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(SynchronousCompositorHost, message)
    IPC_MESSAGE_HANDLER(SyncCompositorHostMsg_UpdateState, ProcessCommonParams)
    IPC_MESSAGE_HANDLER(SyncCompositorHostMsg_OverScroll, OnOverScroll)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void SynchronousCompositorHost::DidBecomeCurrent() {
  client_->DidBecomeCurrent(this);
}

SynchronousCompositor::Frame SynchronousCompositorHost::DemandDrawHw(
    const gfx::Size& surface_size,
    const gfx::Transform& transform,
    const gfx::Rect& viewport,
    const gfx::Rect& clip,
    const gfx::Rect& viewport_rect_for_tile_priority,
    const gfx::Transform& transform_for_tile_priority) {
  SyncCompositorDemandDrawHwParams params(surface_size, transform, viewport,
                                          clip, viewport_rect_for_tile_priority,
                                          transform_for_tile_priority);
  SynchronousCompositor::Frame frame;
  frame.frame.reset(new cc::CompositorFrame);
  SyncCompositorCommonBrowserParams common_browser_params;
  PopulateCommonParams(&common_browser_params);
  SyncCompositorCommonRendererParams common_renderer_params;
  if (!sender_->Send(new SyncCompositorMsg_DemandDrawHw(
          routing_id_, common_browser_params, params, &common_renderer_params,
          &frame.output_surface_id, frame.frame.get()))) {
    return SynchronousCompositor::Frame();
  }
  ProcessCommonParams(common_renderer_params);
  if (!frame.frame->delegated_frame_data) {
    // This can happen if compositor did not swap in this draw.
    frame.frame.reset();
  }
  if (frame.frame) {
    UpdateFrameMetaData(frame.frame->metadata);
    if (output_surface_id_from_last_draw_ != frame.output_surface_id)
      returned_resources_.clear();
    output_surface_id_from_last_draw_ = frame.output_surface_id;
  }
  return frame;
}

void SynchronousCompositorHost::UpdateFrameMetaData(
    const cc::CompositorFrameMetadata& frame_metadata) {
  rwhva_->SynchronousFrameMetadata(frame_metadata);
}

namespace {

class ScopedSetSkCanvas {
 public:
  explicit ScopedSetSkCanvas(SkCanvas* canvas) {
    SynchronousCompositorSetSkCanvas(canvas);
  }

  ~ScopedSetSkCanvas() {
    SynchronousCompositorSetSkCanvas(nullptr);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ScopedSetSkCanvas);
};

}

bool SynchronousCompositorHost::DemandDrawSwInProc(SkCanvas* canvas) {
  SyncCompositorCommonBrowserParams common_browser_params;
  PopulateCommonParams(&common_browser_params);
  SyncCompositorCommonRendererParams common_renderer_params;
  bool success = false;
  std::unique_ptr<cc::CompositorFrame> frame(new cc::CompositorFrame);
  ScopedSetSkCanvas set_sk_canvas(canvas);
  SyncCompositorDemandDrawSwParams params;  // Unused.
  if (!sender_->Send(new SyncCompositorMsg_DemandDrawSw(
          routing_id_, common_browser_params, params, &success,
          &common_renderer_params, frame.get()))) {
    return false;
  }
  if (!success)
    return false;
  ProcessCommonParams(common_renderer_params);
  UpdateFrameMetaData(frame->metadata);
  return true;
}

class SynchronousCompositorHost::ScopedSendZeroMemory {
 public:
  ScopedSendZeroMemory(SynchronousCompositorHost* host) : host_(host) {}
  ~ScopedSendZeroMemory() { host_->SendZeroMemory(); }

 private:
  SynchronousCompositorHost* const host_;

  DISALLOW_COPY_AND_ASSIGN(ScopedSendZeroMemory);
};

struct SynchronousCompositorHost::SharedMemoryWithSize {
  base::SharedMemory shm;
  const size_t stride;
  const size_t buffer_size;

  SharedMemoryWithSize(size_t stride, size_t buffer_size)
      : stride(stride), buffer_size(buffer_size) {}

 private:
  DISALLOW_COPY_AND_ASSIGN(SharedMemoryWithSize);
};

bool SynchronousCompositorHost::DemandDrawSw(SkCanvas* canvas) {
  if (use_in_process_zero_copy_software_draw_)
    return DemandDrawSwInProc(canvas);

  SyncCompositorDemandDrawSwParams params;
  params.size = gfx::Size(canvas->getBaseLayerSize().width(),
                          canvas->getBaseLayerSize().height());
  SkIRect canvas_clip;
  canvas->getClipDeviceBounds(&canvas_clip);
  params.clip = gfx::SkIRectToRect(canvas_clip);
  params.transform.matrix() = canvas->getTotalMatrix();
  if (params.size.IsEmpty())
    return true;

  SkImageInfo info =
      SkImageInfo::MakeN32Premul(params.size.width(), params.size.height());
  DCHECK_EQ(kRGBA_8888_SkColorType, info.colorType());
  size_t stride = info.minRowBytes();
  size_t buffer_size = info.getSafeSize(stride);
  if (!buffer_size)
    return false;  // Overflow.

  SetSoftwareDrawSharedMemoryIfNeeded(stride, buffer_size);
  if (!software_draw_shm_)
    return false;

  std::unique_ptr<cc::CompositorFrame> frame(new cc::CompositorFrame);
  SyncCompositorCommonBrowserParams common_browser_params;
  PopulateCommonParams(&common_browser_params);
  SyncCompositorCommonRendererParams common_renderer_params;
  bool success = false;
  if (!sender_->Send(new SyncCompositorMsg_DemandDrawSw(
          routing_id_, common_browser_params, params, &success,
          &common_renderer_params, frame.get()))) {
    return false;
  }
  ScopedSendZeroMemory send_zero_memory(this);
  if (!success)
    return false;

  ProcessCommonParams(common_renderer_params);
  UpdateFrameMetaData(frame->metadata);

  SkBitmap bitmap;
  if (!bitmap.installPixels(info, software_draw_shm_->shm.memory(), stride))
    return false;

  {
    TRACE_EVENT0("browser", "DrawBitmap");
    canvas->save();
    canvas->resetMatrix();
    canvas->drawBitmap(bitmap, 0, 0);
    canvas->restore();
  }

  return true;
}

void SynchronousCompositorHost::SetSoftwareDrawSharedMemoryIfNeeded(
    size_t stride,
    size_t buffer_size) {
  if (software_draw_shm_ && software_draw_shm_->stride == stride &&
      software_draw_shm_->buffer_size == buffer_size)
    return;
  software_draw_shm_.reset();
  std::unique_ptr<SharedMemoryWithSize> software_draw_shm(
      new SharedMemoryWithSize(stride, buffer_size));
  {
    TRACE_EVENT1("browser", "AllocateSharedMemory", "buffer_size", buffer_size);
    if (!software_draw_shm->shm.CreateAndMapAnonymous(buffer_size))
      return;
  }

  SyncCompositorSetSharedMemoryParams set_shm_params;
  set_shm_params.buffer_size = buffer_size;
  base::ProcessHandle renderer_process_handle =
      rwhva_->GetRenderWidgetHost()->GetProcess()->GetHandle();
  if (!software_draw_shm->shm.ShareToProcess(renderer_process_handle,
                                             &set_shm_params.shm_handle)) {
    return;
  }

  SyncCompositorCommonBrowserParams common_browser_params;
  PopulateCommonParams(&common_browser_params);
  bool success = false;
  SyncCompositorCommonRendererParams common_renderer_params;
  if (!sender_->Send(new SyncCompositorMsg_SetSharedMemory(
          routing_id_, common_browser_params, set_shm_params, &success,
          &common_renderer_params)) ||
      !success) {
    return;
  }
  software_draw_shm_ = std::move(software_draw_shm);
  ProcessCommonParams(common_renderer_params);
}

void SynchronousCompositorHost::SendZeroMemory() {
  // No need to check return value.
  sender_->Send(new SyncCompositorMsg_ZeroSharedMemory(routing_id_));
}

void SynchronousCompositorHost::ReturnResources(
    uint32_t output_surface_id,
    const cc::CompositorFrameAck& frame_ack) {
  // If output_surface_id does not match, then renderer side has switched
  // to a new OutputSurface, so dropping resources for old OutputSurface
  // is allowed.
  if (output_surface_id_from_last_draw_ != output_surface_id)
    return;
  returned_resources_.insert(returned_resources_.end(),
                             frame_ack.resources.begin(),
                             frame_ack.resources.end());
}

void SynchronousCompositorHost::SetMemoryPolicy(size_t bytes_limit) {
  if (bytes_limit_ == bytes_limit)
    return;
  bytes_limit_ = bytes_limit;
  SendAsyncCompositorStateIfNeeded();
}

void SynchronousCompositorHost::DidChangeRootLayerScrollOffset(
    const gfx::ScrollOffset& root_offset) {
  if (root_scroll_offset_ == root_offset)
    return;
  root_scroll_offset_updated_by_browser_ = true;
  root_scroll_offset_ = root_offset;
  SendAsyncCompositorStateIfNeeded();
}

void SynchronousCompositorHost::SendAsyncCompositorStateIfNeeded() {
  if (weak_ptr_factory_.HasWeakPtrs())
    return;

  ui_task_runner_->PostTask(
      FROM_HERE, base::Bind(&SynchronousCompositorHost::UpdateStateTask,
                            weak_ptr_factory_.GetWeakPtr()));
}

void SynchronousCompositorHost::UpdateStateTask() {
  SyncCompositorCommonBrowserParams common_browser_params;
  PopulateCommonParams(&common_browser_params);
  sender_->Send(
      new SyncCompositorMsg_UpdateState(routing_id_, common_browser_params));
  DCHECK(!weak_ptr_factory_.HasWeakPtrs());
}

void SynchronousCompositorHost::SynchronouslyZoomBy(float zoom_delta,
                                                    const gfx::Point& anchor) {
  SyncCompositorCommonBrowserParams common_browser_params;
  PopulateCommonParams(&common_browser_params);
  SyncCompositorCommonRendererParams common_renderer_params;
  if (!sender_->Send(new SyncCompositorMsg_ZoomBy(
          routing_id_, common_browser_params, zoom_delta, anchor,
          &common_renderer_params))) {
    return;
  }
  ProcessCommonParams(common_renderer_params);
}

void SynchronousCompositorHost::SetIsActive(bool is_active) {
  if (is_active_ == is_active)
    return;
  is_active_ = is_active;
  UpdateNeedsBeginFrames();
  SendAsyncCompositorStateIfNeeded();
}

void SynchronousCompositorHost::OnComputeScroll(
    base::TimeTicks animation_time) {
  if (!need_animate_scroll_)
    return;
  need_animate_scroll_ = false;

  SyncCompositorCommonBrowserParams common_browser_params;
  PopulateCommonParams(&common_browser_params);
  SyncCompositorCommonRendererParams common_renderer_params;
  sender_->Send(new SyncCompositorMsg_ComputeScroll(
      routing_id_, common_browser_params, animation_time));
}

InputEventAckState SynchronousCompositorHost::HandleInputEvent(
    const blink::WebInputEvent& input_event) {
  if (async_input_)
    return INPUT_EVENT_ACK_STATE_NOT_CONSUMED;
  SyncCompositorCommonBrowserParams common_browser_params;
  PopulateCommonParams(&common_browser_params);
  SyncCompositorCommonRendererParams common_renderer_params;
  InputEventAckState ack = INPUT_EVENT_ACK_STATE_NOT_CONSUMED;
  if (!sender_->Send(new SyncCompositorMsg_HandleInputEvent(
          routing_id_, common_browser_params, &input_event,
          &common_renderer_params, &ack))) {
    return INPUT_EVENT_ACK_STATE_NOT_CONSUMED;
  }
  ProcessCommonParams(common_renderer_params);
  return ack;
}

void SynchronousCompositorHost::DidOverscroll(
    const DidOverscrollParams& over_scroll_params) {
  client_->DidOverscroll(over_scroll_params.accumulated_overscroll,
                         over_scroll_params.latest_overscroll_delta,
                         over_scroll_params.current_fling_velocity);
}

void SynchronousCompositorHost::BeginFrame(const cc::BeginFrameArgs& args) {
  if (!is_active_)
    return;

  SyncCompositorCommonBrowserParams common_browser_params;
  PopulateCommonParams(&common_browser_params);
  SyncCompositorCommonRendererParams common_renderer_params;
  if (!sender_->Send(
          new SyncCompositorMsg_BeginFrame(routing_id_, common_browser_params,
                                           args, &common_renderer_params))) {
    return;
  }
  ProcessCommonParams(common_renderer_params);
}

void SynchronousCompositorHost::OnOverScroll(
    const SyncCompositorCommonRendererParams& params,
    const DidOverscrollParams& over_scroll_params) {
  ProcessCommonParams(params);
  DidOverscroll(over_scroll_params);
}

void SynchronousCompositorHost::PopulateCommonParams(
    SyncCompositorCommonBrowserParams* params) {
  DCHECK(params);
  DCHECK(params->ack.resources.empty());
  params->bytes_limit = bytes_limit_;
  params->output_surface_id_for_returned_resources =
      output_surface_id_from_last_draw_;
  params->ack.resources.swap(returned_resources_);
  if (root_scroll_offset_updated_by_browser_) {
    params->root_scroll_offset = root_scroll_offset_;
    params->update_root_scroll_offset = root_scroll_offset_updated_by_browser_;
    root_scroll_offset_updated_by_browser_ = false;
  }
  params->begin_frame_source_paused = !is_active_;

  weak_ptr_factory_.InvalidateWeakPtrs();
}

void SynchronousCompositorHost::ProcessCommonParams(
    const SyncCompositorCommonRendererParams& params) {
  // Ignore if |renderer_param_version_| is newer than |params.version|. This
  // comparison takes into account when the unsigned int wraps.
  if ((renderer_param_version_ - params.version) < 0x80000000) {
    return;
  }
  renderer_param_version_ = params.version;
  need_animate_scroll_ = params.need_animate_scroll;
  if (need_begin_frame_ != params.need_begin_frame) {
    need_begin_frame_ = params.need_begin_frame;
    UpdateNeedsBeginFrames();
  }
  root_scroll_offset_ = params.total_scroll_offset;

  if (need_invalidate_count_ != params.need_invalidate_count) {
    need_invalidate_count_ = params.need_invalidate_count;
    client_->PostInvalidate();
  }

  if (did_activate_pending_tree_count_ !=
      params.did_activate_pending_tree_count) {
    did_activate_pending_tree_count_ = params.did_activate_pending_tree_count;
    client_->DidUpdateContent();
  }

  // Ensure only valid values from compositor are sent to client.
  // Compositor has page_scale_factor set to 0 before initialization, so check
  // for that case here.
  if (params.page_scale_factor) {
    client_->UpdateRootLayerState(
        gfx::ScrollOffsetToVector2dF(params.total_scroll_offset),
        gfx::ScrollOffsetToVector2dF(params.max_scroll_offset),
        params.scrollable_size, params.page_scale_factor,
        params.min_page_scale_factor, params.max_page_scale_factor);
  }
}

void SynchronousCompositorHost::UpdateNeedsBeginFrames() {
  rwhva_->OnSetNeedsBeginFrames(is_active_ && need_begin_frame_);
}

}  // namespace content
