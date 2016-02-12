// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/client/feature/compositor/blimp_compositor.h"

#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/lazy_instance.h"
#include "base/numerics/safe_conversions.h"
#include "base/single_thread_task_runner.h"
#include "base/thread_task_runner_handle.h"
#include "base/threading/thread.h"
#include "base/threading/thread_local.h"
#include "base/threading/thread_restrictions.h"
#include "blimp/client/feature/compositor/blimp_context_provider.h"
#include "blimp/client/feature/compositor/blimp_layer_tree_settings.h"
#include "blimp/client/feature/compositor/blimp_output_surface.h"
#include "blimp/client/feature/render_widget_feature.h"
#include "blimp/common/compositor/blimp_image_serialization_processor.h"
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

}  // namespace

BlimpCompositor::BlimpCompositor(float dp_to_px,
                                 RenderWidgetFeature* render_widget_feature)
    : device_scale_factor_(dp_to_px),
      window_(gfx::kNullAcceleratedWidget),
      host_should_be_visible_(false),
      output_surface_request_pending_(false),
      remote_proto_channel_receiver_(nullptr),
      render_widget_feature_(render_widget_feature),
      image_serialization_processor_(
          make_scoped_ptr(new BlimpImageSerializationProcessor(
              BlimpImageSerializationProcessor::Mode::DESERIALIZATION))) {
  DCHECK(render_widget_feature_);
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
  host_should_be_visible_ = visible;
  SetVisibleInternal(host_should_be_visible_);
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
  SetVisibleInternal(host_should_be_visible_);
}

void BlimpCompositor::ReleaseAcceleratedWidget() {
  if (window_ == gfx::kNullAcceleratedWidget)
    return;

  // Hide the compositor and drop the output surface if necessary.
  SetVisibleInternal(false);

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
  // Destroy the state for the current LayerTreeHost. Since a new render widget
  // has been initialized, we will start receiving compositor messages from
  // this widget now.
  if (host_)
    DestroyLayerTreeHost();
}

void BlimpCompositor::OnCompositorMessageReceived(
    scoped_ptr<cc::proto::CompositorMessage> message) {
  DCHECK(message->has_to_impl());
  const cc::proto::CompositorMessageToImpl& to_impl_proto =
      message->to_impl();

  DCHECK(to_impl_proto.has_message_type());
  switch (to_impl_proto.message_type()) {
    case cc::proto::CompositorMessageToImpl::UNKNOWN:
      NOTIMPLEMENTED() << "Ignoring message of UNKNOWN type";
      break;
    case cc::proto::CompositorMessageToImpl::INITIALIZE_IMPL:
      DCHECK(!host_);
      DCHECK(to_impl_proto.has_initialize_impl_message());

      // Create the remote client LayerTreeHost for the compositor.
      CreateLayerTreeHost(to_impl_proto.initialize_impl_message());
      break;
    case cc::proto::CompositorMessageToImpl::CLOSE_IMPL:
      DCHECK(host_);

      // Destroy the remote client LayerTreeHost for the compositor.
      DestroyLayerTreeHost();
      break;
    default:
      // We should have a receiver if we're getting compositor messages that
      // are not INITIALIZE_IMPL or CLOSE_IMPL.
      DCHECK(remote_proto_channel_receiver_);
      remote_proto_channel_receiver_->OnProtoReceived(std::move(message));
  }
}

void BlimpCompositor::GenerateLayerTreeSettings(
    cc::LayerTreeSettings* settings) {
  PopulateCommonLayerTreeSettings(settings);
}

void BlimpCompositor::SendWebGestureEvent(
    const blink::WebGestureEvent& gesture_event) {
  render_widget_feature_->SendInputEvent(kDummyTabId, gesture_event);
}

void BlimpCompositor::SetVisibleInternal(bool visible) {
  if (!host_)
    return;

  if (visible && window_ != gfx::kNullAcceleratedWidget) {
    // If we're supposed to be visible and we have a valid
    // gfx::AcceleratedWidget make our compositor visible. If the compositor
    // had an outstanding output surface request, trigger the request again so
    // we build the output surface.
    host_->SetVisible(true);
    if (output_surface_request_pending_)
      HandlePendingOutputSurfaceRequest();
  } else if (!visible) {
    // If not visible, hide the compositor and have it drop it's output
    // surface.
    host_->SetVisible(false);
    if (!host_->output_surface_lost()) {
      host_->ReleaseOutputSurface();
    }
  }
}

void BlimpCompositor::CreateLayerTreeHost(
    const cc::proto::InitializeImpl& initialize_message) {
  DCHECK(!host_);

  if (!settings_) {
    settings_.reset(new cc::LayerTreeSettings);

    // TODO(khushalsagar): The server should selectively send only those
    // LayerTreeSettings which should remain consistent across the server and
    // client. Since it currently overrides all settings, ignore them.
    // See crbug/577985.
    GenerateLayerTreeSettings(settings_.get());
  }

  // Create the LayerTreeHost
  cc::LayerTreeHost::InitParams params;
  params.client = this;
  params.task_graph_runner = g_task_graph_runner.Pointer();
  params.gpu_memory_buffer_manager = &gpu_memory_buffer_manager_;
  params.main_task_runner = base::ThreadTaskRunnerHandle::Get();
  params.settings = settings_.get();
  params.image_serialization_processor = image_serialization_processor_.get();

  // TODO(khushalsagar): Add the GpuMemoryBufferManager to params

  host_ =
      cc::LayerTreeHost::CreateRemoteClient(this /* remote_proto_channel */,
                                            GetCompositorTaskRunner(), &params);

  // Now that we have a host, set the visiblity on it correctly.
  SetVisibleInternal(host_should_be_visible_);

  DCHECK(!input_manager_);
  input_manager_ =
      BlimpInputManager::Create(this,
                                base::ThreadTaskRunnerHandle::Get(),
                                GetCompositorTaskRunner(),
                                host_->GetInputHandler());
}

void BlimpCompositor::DestroyLayerTreeHost() {
  DCHECK(host_);
  // Tear down the output surface connection with the old LayerTreeHost
  // instance.
  SetVisibleInternal(false);

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
      BlimpContextProvider::Create(window_, &gpu_memory_buffer_manager_);

  host_->SetOutputSurface(
      make_scoped_ptr(new BlimpOutputSurface(context_provider)));
  output_surface_request_pending_ = false;
}

}  // namespace client
}  // namespace blimp
