// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/client/compositor/blimp_compositor.h"

#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/lazy_instance.h"
#include "base/numerics/safe_conversions.h"
#include "base/single_thread_task_runner.h"
#include "base/thread_task_runner_handle.h"
#include "base/threading/thread.h"
#include "base/threading/thread_local.h"
#include "base/threading/thread_restrictions.h"
#include "blimp/client/compositor/blimp_context_provider.h"
#include "blimp/client/compositor/blimp_layer_tree_settings.h"
#include "blimp/client/compositor/blimp_output_surface.h"
#include "blimp/client/compositor/test/dummy_layer_driver.h"
#include "blimp/client/session/render_widget_feature.h"
#include "blimp/common/compositor/blimp_task_graph_runner.h"
#include "cc/layers/layer.h"
#include "cc/layers/layer_settings.h"
#include "cc/output/output_surface.h"
#include "cc/proto/compositor_message.pb.h"
#include "cc/trees/layer_tree_host.h"
#include "net/base/net_errors.h"
#include "ui/gl/gl_surface.h"

namespace blimp {
namespace client {
namespace {

base::LazyInstance<blimp::BlimpTaskGraphRunner> g_task_graph_runner =
    LAZY_INSTANCE_INITIALIZER;

const int kDummyTabId = 0;

// TODO(dtrainor): Replace this when Layer content comes from the server (see
// crbug.com/527200 for details).
base::LazyInstance<DummyLayerDriver> g_dummy_layer_driver =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

BlimpCompositor::BlimpCompositor(float dp_to_px,
                                 RenderWidgetFeature* render_widget_feature)
    : device_scale_factor_(dp_to_px),
      window_(gfx::kNullAcceleratedWidget),
      host_should_be_visible_(false),
      output_surface_request_pending_(false),
      remote_proto_channel_receiver_(nullptr),
      render_widget_feature_(render_widget_feature) {
  render_widget_feature_->SetDelegate(kDummyTabId, this);
}

BlimpCompositor::~BlimpCompositor() {
  render_widget_feature_->RemoveDelegate(kDummyTabId);
  SetVisible(false);

  // Destroy |host_| first, as it has a reference to the |settings_| and runs
  // tasks on |compositor_thread_|.
  host_.reset();
  settings_.reset();

  // We must destroy |host_| before the |input_manager_|.
  input_manager_.reset();

  if (compositor_thread_)
    compositor_thread_->Stop();
}

void BlimpCompositor::SetVisible(bool visible) {
  // For testing.  Remove once we bind to the network layer.
  if (!host_)
    CreateLayerTreeHost(nullptr);

  host_should_be_visible_ = visible;
  if (!host_ || host_->visible() == visible)
    return;

  if (visible) {
    // If visible, show the compositor.  If the compositor had an outstanding
    // output surface request, trigger the request again so we build the output
    // surface.
    host_->SetVisible(true);
    if (output_surface_request_pending_)
      HandlePendingOutputSurfaceRequest();
  } else {
    // If not visible, hide the compositor and have it drop it's output surface.
    DCHECK(host_->visible());
    host_->SetVisible(false);
    if (!host_->output_surface_lost())
      host_->ReleaseOutputSurface();
  }
}

void BlimpCompositor::SetSize(const gfx::Size& size) {
  viewport_size_ = size;
  if (host_)
    host_->SetViewportSize(viewport_size_);
}

void BlimpCompositor::SetAcceleratedWidget(gfx::AcceleratedWidget widget) {
  if (widget == window_)
    return;

  DCHECK(window_ == gfx::kNullAcceleratedWidget);
  window_ = widget;

  // The compositor should not be visible if there is no output surface.
  DCHECK(!host_ || !host_->visible());

  // This will properly set visibility and will build the output surface if
  // necessary.
  SetVisible(host_should_be_visible_);
}

void BlimpCompositor::ReleaseAcceleratedWidget() {
  if (window_ == gfx::kNullAcceleratedWidget)
    return;

  // Hide the compositor and drop the output surface if necessary.
  SetVisible(false);

  window_ = gfx::kNullAcceleratedWidget;
}

bool BlimpCompositor::OnTouchEvent(const ui::MotionEvent& motion_event) {
  if (input_manager_)
    return input_manager_->OnTouchEvent(motion_event);
  return false;
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
  output_surface_request_pending_ = true;
  HandlePendingOutputSurfaceRequest();
}

void BlimpCompositor::DidInitializeOutputSurface() {
}

void BlimpCompositor::DidFailToInitializeOutputSurface() {}

void BlimpCompositor::WillCommit() {}

void BlimpCompositor::DidCommit() {}

void BlimpCompositor::DidCommitAndDrawFrame() {}

void BlimpCompositor::DidCompleteSwapBuffers() {}

void BlimpCompositor::DidCompletePageScaleAnimation() {}

void BlimpCompositor::RecordFrameTimingEvents(
    scoped_ptr<cc::FrameTimingTracker::CompositeTimingSet> composite_events,
    scoped_ptr<cc::FrameTimingTracker::MainFrameTimingSet> main_frame_events) {}

void BlimpCompositor::SetProtoReceiver(ProtoReceiver* receiver) {
  remote_proto_channel_receiver_ = receiver;
}

void BlimpCompositor::SendCompositorProto(
    const cc::proto::CompositorMessage& proto) {
  render_widget_feature_->SendCompositorMessage(kDummyTabId, proto);
}

void BlimpCompositor::OnRenderWidgetInitialized() {
  DVLOG(1) << "OnRenderWidgetInitialized";

  // Tear down the output surface connection with the old LayerTreeHost
  // instance.
  SetVisible(false);

  // Destroy the old LayerTreeHost state.
  host_.reset();

  // Destroy the old input manager state.
  // It is important to destroy the LayerTreeHost before destroying the input
  // manager as it has a reference to the cc::InputHandlerClient owned by the
  // BlimpInputManager.
  input_manager_.reset();

  // Reset other state.
  output_surface_request_pending_ = false;

  // Make sure we don't have a receiver at this point.
  DCHECK(!remote_proto_channel_receiver_);

  // TODO(khushalsagar): re-initialize compositor and input state for the new
  // render widget.
  SetVisible(true);
}

void BlimpCompositor::OnCompositorMessageReceived(
    scoped_ptr<cc::proto::CompositorMessage> message) {
  // TODO(dtrainor, khushalsagar): Look into the CompositorMessage.  If it is
  // initialize or shutdown, create or destroy |host_|.

  // We should have a receiver if we're getting compositor messages that aren't
  // initialize.
  DCHECK(remote_proto_channel_receiver_);
  remote_proto_channel_receiver_->OnProtoReceived(std::move(message));
}

void BlimpCompositor::GenerateLayerTreeSettings(
    cc::LayerTreeSettings* settings) {
  PopulateCommonLayerTreeSettings(settings);
}

void BlimpCompositor::SendWebInputEvent(
    const blink::WebInputEvent& input_event) {
  render_widget_feature_->SendInputEvent(kDummyTabId, input_event);
}

void BlimpCompositor::CreateLayerTreeHost(
    scoped_ptr<cc::proto::CompositorMessage> message) {
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

  // If we're supposed to be visible and we have a valid gfx::AcceleratedWidget
  // make our compositor visible.
  if (host_should_be_visible_ && window_ != gfx::kNullAcceleratedWidget)
    host_->SetVisible(true);

  host_->SetViewportSize(viewport_size_);
  host_->SetDeviceScaleFactor(device_scale_factor_);

  // For testing, set the dummy Layer.
  scoped_refptr<cc::Layer> root(
      cc::Layer::Create(BlimpCompositor::LayerSettings()));
  host_->SetRootLayer(root);
  g_dummy_layer_driver.Pointer()->SetParentLayer(root);

  // TODO(khushalsagar): Create this after successful initialization of the
  // remote client compositor when implemented.
  DCHECK(!input_manager_);
  input_manager_ =
      BlimpInputManager::Create(this,
                                base::ThreadTaskRunnerHandle::Get(),
                                GetCompositorTaskRunner(),
                                host_->GetInputHandler());
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

void BlimpCompositor::HandlePendingOutputSurfaceRequest() {
  DCHECK(output_surface_request_pending_);

  // We might have had a request from a LayerTreeHost that was then
  // hidden (and hidden means we don't have a native surface).
  // Also make sure we only handle this once.
  if (!host_->visible() || window_ == gfx::kNullAcceleratedWidget)
    return;

  scoped_refptr<BlimpContextProvider> context_provider =
      BlimpContextProvider::Create(window_);

  host_->SetOutputSurface(
      make_scoped_ptr(new BlimpOutputSurface(context_provider)));
  output_surface_request_pending_ = false;
}

cc::LayerSettings BlimpCompositor::LayerSettings() {
  cc::LayerSettings settings;
  settings.use_compositor_animation_timelines = true;
  return settings;
}

}  // namespace client
}  // namespace blimp
