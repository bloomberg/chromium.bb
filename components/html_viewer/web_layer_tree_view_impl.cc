// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/html_viewer/web_layer_tree_view_impl.h"

#include "base/thread_task_runner_handle.h"
#include "cc/blink/web_layer_impl.h"
#include "cc/layers/layer.h"
#include "cc/output/begin_frame_args.h"
#include "cc/scheduler/begin_frame_source.h"
#include "cc/trees/layer_tree_host.h"
#include "components/view_manager/public/cpp/view.h"
#include "mojo/cc/context_provider_mojo.h"
#include "mojo/cc/output_surface_mojo.h"
#include "mojo/converters/surfaces/surfaces_type_converters.h"
#include "third_party/WebKit/public/web/WebWidget.h"
#include "ui/gfx/buffer_types.h"

namespace html_viewer {

WebLayerTreeViewImpl::WebLayerTreeViewImpl(
    scoped_refptr<base::SingleThreadTaskRunner> compositor_task_runner,
    gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager,
    cc::TaskGraphRunner* task_graph_runner,
    mojo::SurfacePtr surface,
    mojo::GpuPtr gpu_service)
    : widget_(NULL),
      view_(NULL),
      main_thread_compositor_task_runner_(base::ThreadTaskRunnerHandle::Get()),
      weak_factory_(this) {
  main_thread_bound_weak_ptr_ = weak_factory_.GetWeakPtr();

  cc::LayerTreeSettings settings;

  // Must match the value of
  // blink::RuntimeEnabledFeature::slimmingPaintEnabled()
  settings.use_display_lists = true;

  settings.use_image_texture_targets = std::vector<unsigned>(
      static_cast<size_t>(gfx::BufferFormat::LAST) + 1, GL_TEXTURE_2D);
  settings.use_one_copy = true;
  // TODO(jam): use multiple compositor raster threads and set gather_pixel_refs
  // accordingly (see content).

  // For web contents, layer transforms should scale up the contents of layers
  // to keep content always crisp when possible.
  settings.layer_transforms_should_scale_layer_contents = true;

  cc::SharedBitmapManager* shared_bitmap_manager = nullptr;

  cc::LayerTreeHost::InitParams params;
  params.client = this;
  params.shared_bitmap_manager = shared_bitmap_manager;
  params.gpu_memory_buffer_manager = gpu_memory_buffer_manager;
  params.settings = &settings;
  params.task_graph_runner = task_graph_runner;
  params.main_task_runner = main_thread_compositor_task_runner_;

  layer_tree_host_ =
      cc::LayerTreeHost::CreateThreaded(compositor_task_runner, &params);
  DCHECK(layer_tree_host_);

  if (surface && gpu_service) {
    mojo::CommandBufferPtr cb;
    gpu_service->CreateOffscreenGLES2Context(GetProxy(&cb));
    scoped_refptr<cc::ContextProvider> context_provider(
        new mojo::ContextProviderMojo(cb.PassInterface().PassHandle()));
    output_surface_.reset(
        new mojo::OutputSurfaceMojo(this, context_provider,
                                    surface.PassInterface().PassHandle()));
  }
  layer_tree_host_->SetLayerTreeHostClientReady();
}

WebLayerTreeViewImpl::~WebLayerTreeViewImpl() {
  // Destroy the LayerTreeHost before anything else as doing so ensures we're
  // not accessed on the compositor thread (we are the LayerTreeHostClient).
  layer_tree_host_.reset();
}

void WebLayerTreeViewImpl::WillBeginMainFrame() {
}

void WebLayerTreeViewImpl::DidBeginMainFrame() {
}

void WebLayerTreeViewImpl::BeginMainFrameNotExpectedSoon() {
}

void WebLayerTreeViewImpl::BeginMainFrame(const cc::BeginFrameArgs& args) {
  VLOG(2) << "WebLayerTreeViewImpl::BeginMainFrame";
  double frame_time_sec = (args.frame_time - base::TimeTicks()).InSecondsF();
  double deadline_sec = (args.deadline - base::TimeTicks()).InSecondsF();
  double interval_sec = args.interval.InSecondsF();
  blink::WebBeginFrameArgs web_begin_frame_args(
      frame_time_sec, deadline_sec, interval_sec);
  widget_->beginFrame(web_begin_frame_args);
}

void WebLayerTreeViewImpl::Layout() {
  widget_->layout();
}

void WebLayerTreeViewImpl::ApplyViewportDeltas(
    const gfx::Vector2dF& inner_delta,
    const gfx::Vector2dF& outer_delta,
    const gfx::Vector2dF& elastic_overscroll_delta,
    float page_scale,
    float top_controls_delta) {
  widget_->applyViewportDeltas(
      inner_delta,
      outer_delta,
      elastic_overscroll_delta,
      page_scale,
      top_controls_delta);
}

void WebLayerTreeViewImpl::RequestNewOutputSurface() {
  if (output_surface_.get())
    layer_tree_host_->SetOutputSurface(output_surface_.Pass());
}

void WebLayerTreeViewImpl::DidFailToInitializeOutputSurface() {
  RequestNewOutputSurface();
}

void WebLayerTreeViewImpl::DidInitializeOutputSurface() {
}

void WebLayerTreeViewImpl::WillCommit() {
}

void WebLayerTreeViewImpl::DidCommit() {
}

void WebLayerTreeViewImpl::DidCommitAndDrawFrame() {
}

void WebLayerTreeViewImpl::DidCompleteSwapBuffers() {
}

void WebLayerTreeViewImpl::setRootLayer(const blink::WebLayer& layer) {
  layer_tree_host_->SetRootLayer(
      static_cast<const cc_blink::WebLayerImpl*>(&layer)->layer());
}

void WebLayerTreeViewImpl::clearRootLayer() {
  layer_tree_host_->SetRootLayer(scoped_refptr<cc::Layer>());
}

void WebLayerTreeViewImpl::setViewportSize(
    const blink::WebSize& device_viewport_size) {
  layer_tree_host_->SetViewportSize(device_viewport_size);
}

blink::WebSize WebLayerTreeViewImpl::deviceViewportSize() const {
  return layer_tree_host_->device_viewport_size();
}

void WebLayerTreeViewImpl::setDeviceScaleFactor(float device_scale_factor) {
  layer_tree_host_->SetDeviceScaleFactor(device_scale_factor);
}

float WebLayerTreeViewImpl::deviceScaleFactor() const {
  return layer_tree_host_->device_scale_factor();
}

void WebLayerTreeViewImpl::setBackgroundColor(blink::WebColor color) {
  layer_tree_host_->set_background_color(color);
}

void WebLayerTreeViewImpl::setHasTransparentBackground(
    bool has_transparent_background) {
  layer_tree_host_->set_has_transparent_background(has_transparent_background);
}

void WebLayerTreeViewImpl::setVisible(bool visible) {
  layer_tree_host_->SetVisible(visible);
}

void WebLayerTreeViewImpl::setPageScaleFactorAndLimits(float page_scale_factor,
                                                       float minimum,
                                                       float maximum) {
  layer_tree_host_->SetPageScaleFactorAndLimits(
      page_scale_factor, minimum, maximum);
}

void WebLayerTreeViewImpl::registerForAnimations(blink::WebLayer* layer) {
  cc::Layer* cc_layer = static_cast<cc_blink::WebLayerImpl*>(layer)->layer();
  cc_layer->RegisterForAnimations(layer_tree_host_->animation_registrar());
}

void WebLayerTreeViewImpl::registerViewportLayers(
    const blink::WebLayer* overscrollElasticityLayer,
    const blink::WebLayer* pageScaleLayer,
    const blink::WebLayer* innerViewportScrollLayer,
    const blink::WebLayer* outerViewportScrollLayer) {
  layer_tree_host_->RegisterViewportLayers(
      // The scroll elasticity layer will only exist when using pinch virtual
      // viewports.
      overscrollElasticityLayer
          ? static_cast<const cc_blink::WebLayerImpl*>(
                overscrollElasticityLayer)->layer()
          : NULL,
      static_cast<const cc_blink::WebLayerImpl*>(pageScaleLayer)->layer(),
      static_cast<const cc_blink::WebLayerImpl*>(innerViewportScrollLayer)
          ->layer(),
      // The outer viewport layer will only exist when using pinch virtual
      // viewports.
      outerViewportScrollLayer
          ? static_cast<const cc_blink::WebLayerImpl*>(outerViewportScrollLayer)
                ->layer()
          : NULL);
}

void WebLayerTreeViewImpl::clearViewportLayers() {
  layer_tree_host_->RegisterViewportLayers(scoped_refptr<cc::Layer>(),
                                           scoped_refptr<cc::Layer>(),
                                           scoped_refptr<cc::Layer>(),
                                           scoped_refptr<cc::Layer>());
}

void WebLayerTreeViewImpl::startPageScaleAnimation(
    const blink::WebPoint& destination,
    bool use_anchor,
    float new_page_scale,
    double duration_sec) {
  base::TimeDelta duration = base::TimeDelta::FromMicroseconds(
      duration_sec * base::Time::kMicrosecondsPerSecond);
  layer_tree_host_->StartPageScaleAnimation(
      gfx::Vector2d(destination.x, destination.y),
      use_anchor,
      new_page_scale,
      duration);
}

void WebLayerTreeViewImpl::setNeedsAnimate() {
  layer_tree_host_->SetNeedsAnimate();
}

void WebLayerTreeViewImpl::finishAllRendering() {
  layer_tree_host_->FinishAllRendering();
}

void WebLayerTreeViewImpl::DidCreateSurface(cc::SurfaceId id) {
  main_thread_compositor_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&WebLayerTreeViewImpl::DidCreateSurfaceOnMainThread,
                 main_thread_bound_weak_ptr_,
                 id));
}

void WebLayerTreeViewImpl::DidCreateSurfaceOnMainThread(cc::SurfaceId id) {
  view_->SetSurfaceId(mojo::SurfaceId::From(id));
}

}  // namespace html_viewer
