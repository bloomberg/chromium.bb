// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/client/compositor/blimp_compositor.h"

#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/lazy_instance.h"
#include "base/single_thread_task_runner.h"
#include "base/thread_task_runner_handle.h"
#include "base/threading/thread.h"
#include "base/threading/thread_local.h"
#include "base/threading/thread_restrictions.h"
#include "blimp/client/compositor/blimp_context_provider.h"
#include "blimp/client/compositor/blimp_layer_tree_settings.h"
#include "blimp/client/compositor/blimp_output_surface.h"
#include "blimp/client/compositor/test/dummy_layer_driver.h"
#include "blimp/common/compositor/blimp_task_graph_runner.h"
#include "cc/layers/layer.h"
#include "cc/output/output_surface.h"
#include "cc/trees/layer_tree_host.h"
#include "ui/gl/gl_surface.h"

namespace {

base::LazyInstance<blimp::BlimpTaskGraphRunner> g_task_graph_runner =
    LAZY_INSTANCE_INITIALIZER;

// TODO(dtrainor): Replace this when Layer content comes from the server (see
// crbug.com/527200 for details).
base::LazyInstance<blimp::DummyLayerDriver> g_dummy_layer_driver =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

namespace blimp {

BlimpCompositor::BlimpCompositor(float dp_to_px)
    : device_scale_factor_(dp_to_px) {}

BlimpCompositor::~BlimpCompositor() {
  // Destroy |host_| first, as it has a reference to the |settings_| and runs
  // tasks on |compositor_thread_|.
  host_.reset();
  settings_.reset();
  if (compositor_thread_)
    compositor_thread_->Stop();
}

void BlimpCompositor::SetVisible(bool visible) {
  if (visible && !host_) {
    if (!settings_) {
      settings_.reset(new cc::LayerTreeSettings);
      GenerateLayerTreeSettings(settings_.get());
    }

    // Create the LayerTreeHost
    cc::LayerTreeHost::InitParams params;
    params.client = this;
    params.task_graph_runner = g_task_graph_runner.Pointer();
    params.main_task_runner = base::ThreadTaskRunnerHandle::Get();
    params.settings = settings_.get();

    // TODO(dtrainor): Swap this out with the remote client proxy when
    // implemented.
    host_ =
        cc::LayerTreeHost::CreateThreaded(GetCompositorTaskRunner(), &params);

    host_->SetVisible(true);
    host_->SetViewportSize(viewport_size_);
    host_->SetDeviceScaleFactor(device_scale_factor_);

    // Build the root Layer.
    scoped_refptr<cc::Layer> root(cc::Layer::Create(cc::LayerSettings()));
    host_->SetRootLayer(root);

    // For testing, set the dummy Layer.
    g_dummy_layer_driver.Pointer()->SetParentLayer(root);

  } else if (!visible && host_) {
    // Release the LayerTreeHost to free all resources when the compositor is no
    // longer visible.  This will destroy the underlying compositor components.
    host_.reset();
  }
}

void BlimpCompositor::SetSize(const gfx::Size& size) {
  viewport_size_ = size;
  if (host_)
    host_->SetViewportSize(viewport_size_);
}

void BlimpCompositor::WillBeginMainFrame() {}

void BlimpCompositor::DidBeginMainFrame() {}

void BlimpCompositor::BeginMainFrame(const cc::BeginFrameArgs& args) {}

void BlimpCompositor::BeginMainFrameNotExpectedSoon() {}

void BlimpCompositor::UpdateLayerTreeHost() {}

void BlimpCompositor::ApplyViewportDeltas(
    const gfx::Vector2dF& inner_delta,
    const gfx::Vector2dF& outer_delta,
    const gfx::Vector2dF& elastic_overscroll_delta,
    float page_scale,
    float top_controls_delta) {}

void BlimpCompositor::RequestNewOutputSurface() {
  gfx::AcceleratedWidget widget = GetWindow();
  DCHECK(widget);

  scoped_refptr<BlimpContextProvider> context_provider =
      BlimpContextProvider::Create(widget);

  host_->SetOutputSurface(
      make_scoped_ptr(new BlimpOutputSurface(context_provider)));
}

void BlimpCompositor::DidInitializeOutputSurface() {}

void BlimpCompositor::DidFailToInitializeOutputSurface() {}

void BlimpCompositor::WillCommit() {}

void BlimpCompositor::DidCommit() {}

void BlimpCompositor::DidCommitAndDrawFrame() {}

void BlimpCompositor::DidCompleteSwapBuffers() {}

void BlimpCompositor::DidCompletePageScaleAnimation() {}

void BlimpCompositor::RecordFrameTimingEvents(
    scoped_ptr<cc::FrameTimingTracker::CompositeTimingSet> composite_events,
    scoped_ptr<cc::FrameTimingTracker::MainFrameTimingSet> main_frame_events) {}

void BlimpCompositor::GenerateLayerTreeSettings(
    cc::LayerTreeSettings* settings) {
  PopulateCommonLayerTreeSettings(settings);
}

scoped_refptr<base::SingleThreadTaskRunner>
BlimpCompositor::GetCompositorTaskRunner() {
  if (compositor_thread_)
    return compositor_thread_->task_runner();

  base::Thread::Options thread_options;
#if defined(OS_ANDROID)
  thread_options.priority = base::ThreadPriority::DISPLAY;
#endif
  compositor_thread_.reset(new base::Thread("Compositor"));
  compositor_thread_->StartWithOptions(thread_options);

  scoped_refptr<base::SingleThreadTaskRunner> task_runner =
      compositor_thread_->task_runner();
  task_runner->PostTask(
      FROM_HERE,
      base::Bind(base::IgnoreResult(&base::ThreadRestrictions::SetIOAllowed),
                 false));
  // TODO(dtrainor): Determine whether or not we can disallow waiting.

  return task_runner;
}

}  // namespace blimp
