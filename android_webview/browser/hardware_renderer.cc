// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/hardware_renderer.h"

#include "android_webview/browser/aw_gl_surface.h"
#include "android_webview/browser/child_frame.h"
#include "android_webview/browser/deferred_gpu_command_service.h"
#include "android_webview/browser/parent_compositor_draw_constraints.h"
#include "android_webview/browser/parent_output_surface.h"
#include "android_webview/browser/shared_renderer_state.h"
#include "android_webview/public/browser/draw_gl.h"
#include "base/auto_reset.h"
#include "base/strings/string_number_conversions.h"
#include "base/trace_event/trace_event.h"
#include "cc/layers/delegated_frame_provider.h"
#include "cc/layers/delegated_renderer_layer.h"
#include "cc/layers/layer.h"
#include "cc/output/compositor_frame.h"
#include "cc/output/output_surface.h"
#include "cc/scheduler/begin_frame_source.h"
#include "cc/trees/layer_tree_host.h"
#include "cc/trees/layer_tree_settings.h"
#include "gpu/blink/webgraphicscontext3d_in_process_command_buffer_impl.h"
#include "gpu/command_buffer/client/gl_in_process_context.h"
#include "gpu/command_buffer/common/gles2_cmd_utils.h"
#include "ui/gfx/frame_time.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/transform.h"
#include "ui/gl/gl_bindings.h"
#include "webkit/common/gpu/context_provider_in_process.h"

namespace android_webview {

namespace {

using gpu_blink::WebGraphicsContext3DImpl;
using gpu_blink::WebGraphicsContext3DInProcessCommandBufferImpl;

scoped_refptr<cc::ContextProvider> CreateContext(
    scoped_refptr<gfx::GLSurface> surface,
    scoped_refptr<gpu::InProcessCommandBuffer::Service> service) {
  const gfx::GpuPreference gpu_preference = gfx::PreferDiscreteGpu;

  blink::WebGraphicsContext3D::Attributes attributes;
  attributes.antialias = false;
  attributes.depth = false;
  attributes.stencil = false;
  attributes.shareResources = true;
  attributes.noAutomaticFlushes = true;
  gpu::gles2::ContextCreationAttribHelper attribs_for_gles2;
  WebGraphicsContext3DImpl::ConvertAttributes(
      attributes, &attribs_for_gles2);
  attribs_for_gles2.lose_context_when_out_of_memory = true;

  scoped_ptr<gpu::GLInProcessContext> context(gpu::GLInProcessContext::Create(
      service,
      surface,
      surface->IsOffscreen(),
      gfx::kNullAcceleratedWidget,
      surface->GetSize(),
      NULL /* share_context */,
      false /* share_resources */,
      attribs_for_gles2,
      gpu_preference,
      gpu::GLInProcessContextSharedMemoryLimits(),
      nullptr,
      nullptr));
  DCHECK(context.get());

  return webkit::gpu::ContextProviderInProcess::Create(
      WebGraphicsContext3DInProcessCommandBufferImpl::WrapContext(
          context.Pass(), attributes),
      "Parent-Compositor");
}

}  // namespace

HardwareRenderer::HardwareRenderer(SharedRendererState* state)
    : shared_renderer_state_(state),
      last_egl_context_(eglGetCurrentContext()),
      stencil_enabled_(false),
      viewport_clip_valid_for_dcheck_(false),
      gl_surface_(new AwGLSurface),
      root_layer_(cc::Layer::Create()),
      resource_collection_(new cc::DelegatedFrameResourceCollection),
      output_surface_(NULL) {
  DCHECK(last_egl_context_);

  resource_collection_->SetClient(this);

  cc::LayerTreeSettings settings;

  // Should be kept in sync with compositor_impl_android.cc.
  settings.renderer_settings.allow_antialiasing = false;
  settings.renderer_settings.highp_threshold_min = 2048;

  // Webview does not own the surface so should not clear it.
  settings.renderer_settings.should_clear_root_render_pass = false;

  // TODO(enne): Update this this compositor to use a synchronous scheduler.
  settings.single_thread_proxy_scheduler = false;

  layer_tree_host_ = cc::LayerTreeHost::CreateSingleThreaded(
      this, this, nullptr, nullptr, settings, nullptr, nullptr);
  layer_tree_host_->SetRootLayer(root_layer_);
  layer_tree_host_->SetLayerTreeHostClientReady();
  layer_tree_host_->set_has_transparent_background(true);
}

HardwareRenderer::~HardwareRenderer() {
  // Must reset everything before |resource_collection_| to ensure all
  // resources are returned before resetting |resource_collection_| client.
  layer_tree_host_.reset();
  root_layer_ = NULL;
  delegated_layer_ = NULL;
  frame_provider_ = NULL;
#if DCHECK_IS_ON()
  // Check collection is empty.
  cc::ReturnedResourceArray returned_resources;
  resource_collection_->TakeUnusedResourcesForChildCompositor(
      &returned_resources);
  DCHECK_EQ(0u, returned_resources.size());
#endif  // DCHECK_IS_ON()

  resource_collection_->SetClient(NULL);

  // Reset draw constraints.
  shared_renderer_state_->PostExternalDrawConstraintsToChildCompositorOnRT(
      ParentCompositorDrawConstraints());
}

void HardwareRenderer::DidBeginMainFrame() {
  // This is called after OutputSurface is created, but before the impl frame
  // starts. We set the draw constraints here.
  DCHECK(output_surface_);
  DCHECK(viewport_clip_valid_for_dcheck_);
  output_surface_->SetExternalStencilTest(stencil_enabled_);
  output_surface_->SetDrawConstraints(viewport_, clip_);
}

void HardwareRenderer::CommitFrame() {
  TRACE_EVENT0("android_webview", "CommitFrame");
  scroll_offset_ = shared_renderer_state_->GetScrollOffsetOnRT();
  {
    scoped_ptr<ChildFrame> child_frame =
        shared_renderer_state_->PassCompositorFrameOnRT();
    if (!child_frame.get())
      return;
    child_frame_ = child_frame.Pass();
  }

  scoped_ptr<cc::CompositorFrame> frame = child_frame_->frame.Pass();
  DCHECK(frame.get());
  DCHECK(!frame->gl_frame_data);
  DCHECK(!frame->software_frame_data);

  // DelegatedRendererLayerImpl applies the inverse device_scale_factor of the
  // renderer frame, assuming that the browser compositor will scale
  // it back up to device scale.  But on Android we put our browser layers in
  // physical pixels and set our browser CC device_scale_factor to 1, so this
  // suppresses the transform.
  frame->delegated_frame_data->device_scale_factor = 1.0f;

  gfx::Size frame_size =
      frame->delegated_frame_data->render_pass_list.back()->output_rect.size();
  bool size_changed = frame_size != frame_size_;
  frame_size_ = frame_size;

  if (!frame_provider_.get() || size_changed) {
    if (delegated_layer_.get()) {
      delegated_layer_->RemoveFromParent();
    }

    frame_provider_ = new cc::DelegatedFrameProvider(
        resource_collection_.get(), frame->delegated_frame_data.Pass());

    delegated_layer_ = cc::DelegatedRendererLayer::Create(frame_provider_);
    delegated_layer_->SetBounds(frame_size_);
    delegated_layer_->SetIsDrawable(true);

    root_layer_->AddChild(delegated_layer_);
  } else {
    frame_provider_->SetFrameData(frame->delegated_frame_data.Pass());
  }
}

void HardwareRenderer::DrawGL(bool stencil_enabled,
                              int framebuffer_binding_ext,
                              AwDrawGLInfo* draw_info) {
  TRACE_EVENT0("android_webview", "HardwareRenderer::DrawGL");

  // We need to watch if the current Android context has changed and enforce
  // a clean-up in the compositor.
  EGLContext current_context = eglGetCurrentContext();
  DCHECK(current_context) << "DrawGL called without EGLContext";

  // TODO(boliu): Handle context loss.
  if (last_egl_context_ != current_context)
    DLOG(WARNING) << "EGLContextChanged";

  gfx::Transform transform(gfx::Transform::kSkipInitialization);
  transform.matrix().setColMajorf(draw_info->transform);
  transform.Translate(scroll_offset_.x(), scroll_offset_.y());

  viewport_.SetSize(draw_info->width, draw_info->height);
  // Need to post the new transform matrix back to child compositor
  // because there is no onDraw during a Render Thread animation, and child
  // compositor might not have the tiles rasterized as the animation goes on.
  ParentCompositorDrawConstraints draw_constraints(
      draw_info->is_layer, transform, gfx::Rect(viewport_));
  if (!child_frame_.get() || draw_constraints.NeedUpdate(*child_frame_)) {
    shared_renderer_state_->PostExternalDrawConstraintsToChildCompositorOnRT(
        draw_constraints);
  }

  if (!delegated_layer_.get())
    return;

  layer_tree_host_->SetViewportSize(viewport_);
  clip_.SetRect(draw_info->clip_left,
                draw_info->clip_top,
                draw_info->clip_right - draw_info->clip_left,
                draw_info->clip_bottom - draw_info->clip_top);
  stencil_enabled_ = stencil_enabled;

  delegated_layer_->SetTransform(transform);

  gl_surface_->SetBackingFrameBufferObject(framebuffer_binding_ext);
  {
    base::AutoReset<bool> frame_resetter(&viewport_clip_valid_for_dcheck_,
                                         true);
    layer_tree_host_->SetNeedsRedrawRect(clip_);
    layer_tree_host_->Composite(gfx::FrameTime::Now());
  }
  gl_surface_->ResetBackingFrameBufferObject();
}

void HardwareRenderer::RequestNewOutputSurface() {
  scoped_refptr<cc::ContextProvider> context_provider =
      CreateContext(gl_surface_,
                    DeferredGpuCommandService::GetInstance());
  scoped_ptr<ParentOutputSurface> output_surface_holder(
      new ParentOutputSurface(context_provider));
  output_surface_ = output_surface_holder.get();
  layer_tree_host_->SetOutputSurface(output_surface_holder.Pass());
}

void HardwareRenderer::DidFailToInitializeOutputSurface() {
  RequestNewOutputSurface();
}

void HardwareRenderer::UnusedResourcesAreAvailable() {
  cc::ReturnedResourceArray returned_resources;
  resource_collection_->TakeUnusedResourcesForChildCompositor(
      &returned_resources);
  shared_renderer_state_->InsertReturnedResourcesOnRT(returned_resources);
}

}  // namespace android_webview
