// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/web_layer_tree_view_impl_for_testing.h"

#include "base/command_line.h"
#include "base/strings/string_number_conversions.h"
#include "base/synchronization/lock.h"
#include "base/thread_task_runner_handle.h"
#include "cc/animation/animation_host.h"
#include "cc/animation/animation_timeline.h"
#include "cc/base/switches.h"
#include "cc/blink/web_compositor_animation_timeline_impl.h"
#include "cc/blink/web_layer_impl.h"
#include "cc/input/input_handler.h"
#include "cc/layers/layer.h"
#include "cc/scheduler/begin_frame_source.h"
#include "cc/trees/layer_tree_host.h"
#include "content/test/test_blink_web_unit_test_support.h"
#include "third_party/WebKit/public/platform/Platform.h"
#include "third_party/WebKit/public/platform/WebLayer.h"
#include "third_party/WebKit/public/platform/WebLayerTreeView.h"
#include "third_party/WebKit/public/platform/WebSize.h"
#include "third_party/WebKit/public/web/WebRuntimeFeatures.h"

using blink::WebColor;
using blink::WebRect;
using blink::WebSize;

namespace content {

WebLayerTreeViewImplForTesting::WebLayerTreeViewImplForTesting() {}

WebLayerTreeViewImplForTesting::~WebLayerTreeViewImplForTesting() {}

void WebLayerTreeViewImplForTesting::Initialize() {
  cc::LayerTreeSettings settings;

  // For web contents, layer transforms should scale up the contents of layers
  // to keep content always crisp when possible.
  settings.layer_transforms_should_scale_layer_contents = true;

  // Accelerated animations are enabled for unit tests.
  settings.accelerated_animation_enabled = true;
  // External cc::AnimationHost is enabled for unit tests.
  settings.use_compositor_animation_timelines = true;

  cc::LayerTreeHost::InitParams params;
  params.client = this;
  params.settings = &settings;
  params.main_task_runner = base::ThreadTaskRunnerHandle::Get();
  params.task_graph_runner = &task_graph_runner_;
  layer_tree_host_ = cc::LayerTreeHost::CreateSingleThreaded(this, &params);
  DCHECK(layer_tree_host_);
}

void WebLayerTreeViewImplForTesting::setRootLayer(
    const blink::WebLayer& root) {
  layer_tree_host_->SetRootLayer(
      static_cast<const cc_blink::WebLayerImpl*>(&root)->layer());
}

void WebLayerTreeViewImplForTesting::clearRootLayer() {
  layer_tree_host_->SetRootLayer(scoped_refptr<cc::Layer>());
}

void WebLayerTreeViewImplForTesting::attachCompositorAnimationTimeline(
    blink::WebCompositorAnimationTimeline* compositor_timeline) {
  DCHECK(compositor_timeline);
  DCHECK(layer_tree_host_->animation_host());
  layer_tree_host_->animation_host()->AddAnimationTimeline(
      static_cast<const cc_blink::WebCompositorAnimationTimelineImpl*>(
          compositor_timeline)
          ->animation_timeline());
}

void WebLayerTreeViewImplForTesting::detachCompositorAnimationTimeline(
    blink::WebCompositorAnimationTimeline* compositor_timeline) {
  DCHECK(compositor_timeline);
  DCHECK(layer_tree_host_->animation_host());
  layer_tree_host_->animation_host()->RemoveAnimationTimeline(
      static_cast<const cc_blink::WebCompositorAnimationTimelineImpl*>(
          compositor_timeline)
          ->animation_timeline());
}

void WebLayerTreeViewImplForTesting::setViewportSize(
    const WebSize& unused_deprecated,
    const WebSize& device_viewport_size) {
  layer_tree_host_->SetViewportSize(device_viewport_size);
}

void WebLayerTreeViewImplForTesting::setViewportSize(
    const WebSize& device_viewport_size) {
  layer_tree_host_->SetViewportSize(device_viewport_size);
}

void WebLayerTreeViewImplForTesting::setDeviceScaleFactor(
    float device_scale_factor) {
  layer_tree_host_->SetDeviceScaleFactor(device_scale_factor);
}

void WebLayerTreeViewImplForTesting::setBackgroundColor(WebColor color) {
  layer_tree_host_->set_background_color(color);
}

void WebLayerTreeViewImplForTesting::setHasTransparentBackground(
    bool transparent) {
  layer_tree_host_->set_has_transparent_background(transparent);
}

void WebLayerTreeViewImplForTesting::setVisible(bool visible) {
  layer_tree_host_->SetVisible(visible);
}

void WebLayerTreeViewImplForTesting::setPageScaleFactorAndLimits(
    float page_scale_factor,
    float minimum,
    float maximum) {
  layer_tree_host_->SetPageScaleFactorAndLimits(
      page_scale_factor, minimum, maximum);
}

void WebLayerTreeViewImplForTesting::startPageScaleAnimation(
    const blink::WebPoint& scroll,
    bool use_anchor,
    float new_page_scale,
    double duration_sec) {}

void WebLayerTreeViewImplForTesting::setNeedsAnimate() {
  layer_tree_host_->SetNeedsAnimate();
}

void WebLayerTreeViewImplForTesting::didStopFlinging() {}

void WebLayerTreeViewImplForTesting::setDeferCommits(bool defer_commits) {
  layer_tree_host_->SetDeferCommits(defer_commits);
}

void WebLayerTreeViewImplForTesting::UpdateLayerTreeHost() {
}

void WebLayerTreeViewImplForTesting::ApplyViewportDeltas(
    const gfx::Vector2dF& inner_delta,
    const gfx::Vector2dF& outer_delta,
    const gfx::Vector2dF& elastic_overscroll_delta,
    float page_scale,
    float top_controls_delta) {
}

void WebLayerTreeViewImplForTesting::RequestNewOutputSurface() {
  // Intentionally do not create and set an OutputSurface.
}

void WebLayerTreeViewImplForTesting::DidFailToInitializeOutputSurface() {
  NOTREACHED();
}

void WebLayerTreeViewImplForTesting::registerForAnimations(
    blink::WebLayer* layer) {
  cc::Layer* cc_layer = static_cast<cc_blink::WebLayerImpl*>(layer)->layer();
  cc_layer->RegisterForAnimations(layer_tree_host_->animation_registrar());
}

void WebLayerTreeViewImplForTesting::registerViewportLayers(
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

void WebLayerTreeViewImplForTesting::clearViewportLayers() {
  layer_tree_host_->RegisterViewportLayers(scoped_refptr<cc::Layer>(),
                                           scoped_refptr<cc::Layer>(),
                                           scoped_refptr<cc::Layer>(),
                                           scoped_refptr<cc::Layer>());
}

void WebLayerTreeViewImplForTesting::registerSelection(
    const blink::WebSelection& selection) {
}

void WebLayerTreeViewImplForTesting::clearSelection() {
}

void WebLayerTreeViewImplForTesting::setEventListenerProperties(
    blink::WebEventListenerClass eventClass,
    blink::WebEventListenerProperties properties) {
  // Equality of static_cast is checked in render_widget_compositor.cc.
  layer_tree_host_->SetEventListenerProperties(
      static_cast<cc::EventListenerClass>(eventClass),
      static_cast<cc::EventListenerProperties>(properties));
}

blink::WebEventListenerProperties
WebLayerTreeViewImplForTesting::eventListenerProperties(
    blink::WebEventListenerClass event_class) const {
  // Equality of static_cast is checked in render_widget_compositor.cc.
  return static_cast<blink::WebEventListenerProperties>(
      layer_tree_host_->event_listener_properties(
          static_cast<cc::EventListenerClass>(event_class)));
}

void WebLayerTreeViewImplForTesting::setHaveScrollEventHandlers(
    bool have_event_handlers) {
  layer_tree_host_->SetHaveScrollEventHandlers(have_event_handlers);
}

bool WebLayerTreeViewImplForTesting::haveScrollEventHandlers() const {
  return layer_tree_host_->have_scroll_event_handlers();
}

}  // namespace content
