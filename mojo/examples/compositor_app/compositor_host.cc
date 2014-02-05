// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/examples/compositor_app/compositor_host.h"

#include "cc/layers/layer.h"
#include "cc/layers/solid_color_layer.h"
#include "cc/output/context_provider.h"
#include "cc/output/output_surface.h"
#include "cc/trees/layer_tree_host.h"
#include "mojo/examples/compositor_app/mojo_context_provider.h"

namespace mojo {
namespace examples {

CompositorHost::CompositorHost(ScopedMessagePipeHandle gl_pipe)
    : gl_pipe_(gl_pipe.Pass()), compositor_thread_("compositor") {
  DCHECK(gl_pipe_.is_valid());
  bool started = compositor_thread_.Start();
  DCHECK(started);

  cc::LayerTreeSettings settings;
  tree_ = cc::LayerTreeHost::CreateThreaded(
      this, NULL, settings, compositor_thread_.message_loop_proxy());
  SetupScene();
}

CompositorHost::~CompositorHost() {}

void CompositorHost::SetSize(gfx::Size viewport_size) {
  tree_->SetViewportSize(viewport_size);
  tree_->SetLayerTreeHostClientReady();
  tree_->InitializeOutputSurfaceIfNeeded();
}

void CompositorHost::SetupScene() {
  scoped_refptr<cc::Layer> root_layer = cc::SolidColorLayer::Create();
  root_layer->SetBounds(gfx::Size(500, 500));
  root_layer->SetBackgroundColor(SK_ColorBLUE);
  root_layer->SetIsDrawable(true);
  tree_->SetRootLayer(root_layer);

  child_layer_ = cc::SolidColorLayer::Create();
  child_layer_->SetBounds(gfx::Size(100, 100));
  child_layer_->SetBackgroundColor(SK_ColorGREEN);
  child_layer_->SetIsDrawable(true);
  gfx::Transform child_transform;
  child_transform.Translate(200, 200);
  child_layer_->SetTransform(child_transform);
  root_layer->AddChild(child_layer_);
}

void CompositorHost::WillBeginMainFrame(int frame_id) {}
void CompositorHost::DidBeginMainFrame() {}

void CompositorHost::Animate(double frame_begin_time) {
  // TODO(jamesr): Should use cc's animation system.
  static const double kDegreesPerSecond = 70.0;
  double child_rotation_degrees = kDegreesPerSecond * frame_begin_time;
  gfx::Transform child_transform;
  child_transform.Translate(200, 200);
  child_transform.Rotate(child_rotation_degrees);
  child_layer_->SetTransform(child_transform);
  tree_->SetNeedsAnimate();
}

void CompositorHost::Layout() {}
void CompositorHost::ApplyScrollAndScale(const gfx::Vector2d& scroll_delta,
                                         float page_scale) {}

scoped_ptr<cc::OutputSurface> CompositorHost::CreateOutputSurface(
    bool fallback) {
  return make_scoped_ptr(
      new cc::OutputSurface(new MojoContextProvider(gl_pipe_.Pass())));
}

void CompositorHost::DidInitializeOutputSurface(bool success) {}

void CompositorHost::WillCommit() {}
void CompositorHost::DidCommit() {}
void CompositorHost::DidCommitAndDrawFrame() {}
void CompositorHost::DidCompleteSwapBuffers() {}

scoped_refptr<cc::ContextProvider> CompositorHost::OffscreenContextProvider() {
  return NULL;
}

}  // namespace examples
}  // namespace mojo
