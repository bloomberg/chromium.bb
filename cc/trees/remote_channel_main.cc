// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/remote_channel_main.h"

#include "base/memory/scoped_ptr.h"
#include "cc/proto/compositor_message.pb.h"
#include "cc/proto/compositor_message_to_impl.pb.h"
#include "cc/proto/compositor_message_to_main.pb.h"
#include "cc/trees/layer_tree_host.h"

namespace cc {

scoped_ptr<RemoteChannelMain> RemoteChannelMain::Create(
    RemoteProtoChannel* remote_proto_channel,
    ProxyMain* proxy_main,
    TaskRunnerProvider* task_runner_provider) {
  return make_scoped_ptr(new RemoteChannelMain(remote_proto_channel, proxy_main,
                                               task_runner_provider));
}

RemoteChannelMain::RemoteChannelMain(RemoteProtoChannel* remote_proto_channel,
                                     ProxyMain* proxy_main,
                                     TaskRunnerProvider* task_runner_provider)
    : remote_proto_channel_(remote_proto_channel),
      proxy_main_(proxy_main),
      task_runner_provider_(task_runner_provider),
      initialized_(false) {
  DCHECK(remote_proto_channel_);
  DCHECK(proxy_main_);
  DCHECK(task_runner_provider_);
  DCHECK(task_runner_provider_->IsMainThread());
  remote_proto_channel_->SetProtoReceiver(this);
}

RemoteChannelMain::~RemoteChannelMain() {
  DCHECK(task_runner_provider_->IsMainThread());
  DCHECK(!initialized_);

  remote_proto_channel_->SetProtoReceiver(nullptr);
}

void RemoteChannelMain::OnProtoReceived(
    scoped_ptr<proto::CompositorMessage> proto) {}

void RemoteChannelMain::SetThrottleFrameProductionOnImpl(bool throttle) {}

void RemoteChannelMain::UpdateTopControlsStateOnImpl(
    TopControlsState constraints,
    TopControlsState current,
    bool animate) {}

void RemoteChannelMain::InitializeOutputSurfaceOnImpl(
    OutputSurface* output_surface) {
  NOTREACHED() << "Should not be called on the server LayerTreeHost";
}

void RemoteChannelMain::MainThreadHasStoppedFlingingOnImpl() {
  proto::CompositorMessage proto;
  proto::CompositorMessageToImpl* to_impl_proto = proto.mutable_to_impl();
  to_impl_proto->set_message_type(
      proto::CompositorMessageToImpl::MAIN_THREAD_HAS_STOPPED_FLINGING_ON_IMPL);

  SendMessageProto(proto);
}

void RemoteChannelMain::SetInputThrottledUntilCommitOnImpl(bool is_throttled) {}

void RemoteChannelMain::SetDeferCommitsOnImpl(bool defer_commits) {}

void RemoteChannelMain::FinishAllRenderingOnImpl(CompletionEvent* completion) {
  completion->Signal();
}

void RemoteChannelMain::SetVisibleOnImpl(bool visible) {
  NOTIMPLEMENTED() << "Visibility is not controlled by the server";
}

void RemoteChannelMain::ReleaseOutputSurfaceOnImpl(
    CompletionEvent* completion) {
  NOTREACHED() << "Should not be called on the server LayerTreeHost";
  completion->Signal();
}

void RemoteChannelMain::MainFrameWillHappenOnImplForTesting(
    CompletionEvent* completion,
    bool* main_frame_will_happen) {
  // For the LayerTreeTests in remote mode, LayerTreeTest directly calls
  // RemoteChannelImpl::MainFrameWillHappenForTesting and avoids adding a
  // message type for tests to the compositor protocol.
  NOTREACHED();
}

void RemoteChannelMain::SetNeedsRedrawOnImpl(const gfx::Rect& damage_rect) {}

void RemoteChannelMain::SetNeedsCommitOnImpl() {}

void RemoteChannelMain::BeginMainFrameAbortedOnImpl(
    CommitEarlyOutReason reason,
    base::TimeTicks main_thread_start_time) {}

void RemoteChannelMain::StartCommitOnImpl(
    CompletionEvent* completion,
    LayerTreeHost* layer_tree_host,
    base::TimeTicks main_thread_start_time,
    bool hold_commit_for_activation) {
  completion->Signal();
}

void RemoteChannelMain::SynchronouslyInitializeImpl(
    LayerTreeHost* layer_tree_host,
    scoped_ptr<BeginFrameSource> external_begin_frame_source) {
  DCHECK(!initialized_);

  proto::CompositorMessage proto;
  proto::CompositorMessageToImpl* to_impl_proto = proto.mutable_to_impl();
  to_impl_proto->set_message_type(
      proto::CompositorMessageToImpl::INITIALIZE_IMPL);
  proto::InitializeImpl* initialize_impl_proto =
      to_impl_proto->mutable_initialize_impl_message();
  proto::LayerTreeSettings* settings_proto =
      initialize_impl_proto->mutable_layer_tree_settings();
  layer_tree_host->settings().ToProtobuf(settings_proto);

  SendMessageProto(proto);
  initialized_ = true;
}

void RemoteChannelMain::SynchronouslyCloseImpl() {
  DCHECK(initialized_);
  proto::CompositorMessage proto;
  proto::CompositorMessageToImpl* to_impl_proto = proto.mutable_to_impl();
  to_impl_proto->set_message_type(proto::CompositorMessageToImpl::CLOSE_IMPL);

  SendMessageProto(proto);
  initialized_ = false;
}

void RemoteChannelMain::SendMessageProto(
    const proto::CompositorMessage& proto) {
  remote_proto_channel_->SendCompositorProto(proto);
}

}  // namespace cc
