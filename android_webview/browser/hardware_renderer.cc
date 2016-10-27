// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/hardware_renderer.h"

#include <utility>

#include "android_webview/browser/aw_gl_surface.h"
#include "android_webview/browser/aw_render_thread_context_provider.h"
#include "android_webview/browser/child_frame.h"
#include "android_webview/browser/parent_compositor_draw_constraints.h"
#include "android_webview/browser/render_thread_manager.h"
#include "android_webview/browser/surfaces_instance.h"
#include "android_webview/public/browser/draw_gl.h"
#include "base/memory/ptr_util.h"
#include "base/trace_event/trace_event.h"
#include "cc/output/compositor_frame.h"
#include "cc/surfaces/surface_factory.h"
#include "cc/surfaces/surface_id_allocator.h"
#include "cc/surfaces/surface_manager.h"
#include "ui/gfx/transform.h"
#include "ui/gl/gl_bindings.h"

namespace android_webview {

HardwareRenderer::HardwareRenderer(RenderThreadManager* state)
    : render_thread_manager_(state),
      last_egl_context_(eglGetCurrentContext()),
      surfaces_(SurfacesInstance::GetOrCreateInstance()),
      frame_sink_id_(surfaces_->AllocateFrameSinkId()),
      surface_id_allocator_(base::MakeUnique<cc::SurfaceIdAllocator>()),
      last_committed_compositor_frame_sink_id_(0u),
      last_submitted_compositor_frame_sink_id_(0u) {
  DCHECK(last_egl_context_);
  surfaces_->GetSurfaceManager()->RegisterFrameSinkId(frame_sink_id_);
  surfaces_->GetSurfaceManager()->RegisterSurfaceFactoryClient(frame_sink_id_,
                                                               this);
}

HardwareRenderer::~HardwareRenderer() {
  // Must reset everything before |surface_factory_| to ensure all
  // resources are returned before resetting.
  if (!child_id_.is_null())
    DestroySurface();
  surface_factory_.reset();
  surfaces_->GetSurfaceManager()->UnregisterSurfaceFactoryClient(
      frame_sink_id_);
  surfaces_->GetSurfaceManager()->InvalidateFrameSinkId(frame_sink_id_);

  // Reset draw constraints.
  render_thread_manager_->PostExternalDrawConstraintsToChildCompositorOnRT(
      ParentCompositorDrawConstraints());
  ReturnResourcesInChildFrame();
}

void HardwareRenderer::CommitFrame() {
  TRACE_EVENT0("android_webview", "CommitFrame");
  scroll_offset_ = render_thread_manager_->GetScrollOffsetOnRT();
  std::unique_ptr<ChildFrame> child_frame =
      render_thread_manager_->PassFrameOnRT();
  if (!child_frame.get())
    return;
  ReturnResourcesInChildFrame();
  frame_future_ = render_thread_manager_->PassFrameFutureOnRT();
  child_frame_ = std::move(child_frame);
}

void HardwareRenderer::DrawGL(AwDrawGLInfo* draw_info) {
  TRACE_EVENT0("android_webview", "HardwareRenderer::DrawGL");

  if (frame_future_) {
    TRACE_EVENT0("android_webview", "GetFrame");
    DCHECK(child_frame_);
    DCHECK(!child_frame_->frame);

    std::unique_ptr<content::SynchronousCompositor::Frame> frame =
        frame_future_->GetFrame();
    if (frame) {
      child_frame_->compositor_frame_sink_id = frame->compositor_frame_sink_id;
      child_frame_->frame = std::move(frame->frame);
    } else {
      child_frame_.reset();
    }
    frame_future_ = nullptr;
  }

  if (child_frame_) {
    last_committed_compositor_frame_sink_id_ =
        child_frame_->compositor_frame_sink_id;
  }

  // We need to watch if the current Android context has changed and enforce
  // a clean-up in the compositor.
  EGLContext current_context = eglGetCurrentContext();
  DCHECK(current_context) << "DrawGL called without EGLContext";

  // TODO(boliu): Handle context loss.
  if (last_egl_context_ != current_context)
    DLOG(WARNING) << "EGLContextChanged";

  // SurfaceFactory::SubmitCompositorFrame might call glFlush. So calling it
  // during "kModeSync" stage (which does not allow GL) might result in extra
  // kModeProcess. Instead, submit the frame in "kModeDraw" stage to avoid
  // unnecessary kModeProcess.
  if (child_frame_.get() && child_frame_->frame.get()) {
    if (!compositor_id_.Equals(child_frame_->compositor_id) ||
        last_submitted_compositor_frame_sink_id_ !=
            child_frame_->compositor_frame_sink_id) {
      if (!child_id_.is_null())
        DestroySurface();

      // This will return all the resources to the previous compositor.
      surface_factory_.reset();
      compositor_id_ = child_frame_->compositor_id;
      last_submitted_compositor_frame_sink_id_ =
          child_frame_->compositor_frame_sink_id;
      surface_factory_.reset(new cc::SurfaceFactory(
          frame_sink_id_, surfaces_->GetSurfaceManager(), this));
    }

    std::unique_ptr<cc::CompositorFrame> child_compositor_frame =
        std::move(child_frame_->frame);

    gfx::Size frame_size =
        child_compositor_frame->delegated_frame_data->render_pass_list.back()
            ->output_rect.size();
    bool size_changed = frame_size != frame_size_;
    frame_size_ = frame_size;
    if (child_id_.is_null() || size_changed) {
      if (!child_id_.is_null())
        DestroySurface();
      AllocateSurface();
    }

    surface_factory_->SubmitCompositorFrame(child_id_,
                                            std::move(*child_compositor_frame),
                                            cc::SurfaceFactory::DrawCallback());
  }

  gfx::Transform transform(gfx::Transform::kSkipInitialization);
  transform.matrix().setColMajorf(draw_info->transform);
  transform.Translate(scroll_offset_.x(), scroll_offset_.y());

  gfx::Size viewport(draw_info->width, draw_info->height);
  // Need to post the new transform matrix back to child compositor
  // because there is no onDraw during a Render Thread animation, and child
  // compositor might not have the tiles rasterized as the animation goes on.
  ParentCompositorDrawConstraints draw_constraints(
      draw_info->is_layer, transform, viewport.IsEmpty());
  if (!child_frame_.get() || draw_constraints.NeedUpdate(*child_frame_)) {
    render_thread_manager_->PostExternalDrawConstraintsToChildCompositorOnRT(
        draw_constraints);
  }

  if (child_id_.is_null())
    return;

  gfx::Rect clip(draw_info->clip_left, draw_info->clip_top,
                 draw_info->clip_right - draw_info->clip_left,
                 draw_info->clip_bottom - draw_info->clip_top);
  surfaces_->DrawAndSwap(viewport, clip, transform, frame_size_,
                         cc::SurfaceId(frame_sink_id_, child_id_));
}

void HardwareRenderer::AllocateSurface() {
  DCHECK(child_id_.is_null());
  DCHECK(surface_factory_);
  child_id_ = surface_id_allocator_->GenerateId();
  surface_factory_->Create(child_id_);
  surfaces_->AddChildId(cc::SurfaceId(frame_sink_id_, child_id_));
}

void HardwareRenderer::DestroySurface() {
  DCHECK(!child_id_.is_null());
  DCHECK(surface_factory_);

  // Submit an empty frame to force any existing resources to be returned.
  cc::CompositorFrame empty_frame;
  empty_frame.delegated_frame_data =
      base::WrapUnique(new cc::DelegatedFrameData);
  surface_factory_->SubmitCompositorFrame(child_id_, std::move(empty_frame),
                                          cc::SurfaceFactory::DrawCallback());

  surfaces_->RemoveChildId(cc::SurfaceId(frame_sink_id_, child_id_));
  surface_factory_->Destroy(child_id_);
  child_id_ = cc::LocalFrameId();
}

void HardwareRenderer::ReturnResources(
    const cc::ReturnedResourceArray& resources) {
  ReturnResourcesToCompositor(resources, compositor_id_,
                              last_submitted_compositor_frame_sink_id_);
}

void HardwareRenderer::SetBeginFrameSource(
    cc::BeginFrameSource* begin_frame_source) {
  // TODO(tansell): Hook this up.
}

void HardwareRenderer::ReturnResourcesInChildFrame() {
  if (child_frame_.get() && child_frame_->frame.get()) {
    cc::ReturnedResourceArray resources_to_return;
    cc::TransferableResource::ReturnResources(
        child_frame_->frame->delegated_frame_data->resource_list,
        &resources_to_return);

    // The child frame's compositor id is not necessarily same as
    // compositor_id_.
    ReturnResourcesToCompositor(resources_to_return,
                                child_frame_->compositor_id,
                                child_frame_->compositor_frame_sink_id);
  }
  child_frame_.reset();
}

void HardwareRenderer::ReturnResourcesToCompositor(
    const cc::ReturnedResourceArray& resources,
    const CompositorID& compositor_id,
    uint32_t compositor_frame_sink_id) {
  if (compositor_frame_sink_id != last_committed_compositor_frame_sink_id_)
    return;
  render_thread_manager_->InsertReturnedResourcesOnRT(resources, compositor_id,
                                                      compositor_frame_sink_id);
}

}  // namespace android_webview
