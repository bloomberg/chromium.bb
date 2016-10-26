// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/engine/renderer/blimp_remote_compositor_bridge.h"

#include "cc/blimp/compositor_proto_state.h"
#include "cc/blimp/remote_compositor_bridge_client.h"
#include "cc/output/swap_promise.h"
#include "cc/proto/compositor_message.pb.h"

namespace blimp {
namespace engine {

BlimpRemoteCompositorBridge::BlimpRemoteCompositorBridge(
    cc::RemoteProtoChannel* remote_proto_channel,
    scoped_refptr<base::SingleThreadTaskRunner> compositor_main_task_runner)
    : RemoteCompositorBridge(std::move(compositor_main_task_runner)),
      remote_proto_channel_(remote_proto_channel),
      scheduler_(compositor_main_task_runner_.get(), this) {
  remote_proto_channel_->SetProtoReceiver(this);
}

BlimpRemoteCompositorBridge::~BlimpRemoteCompositorBridge() = default;

void BlimpRemoteCompositorBridge::BindToClient(
    cc::RemoteCompositorBridgeClient* client) {
  DCHECK(!client_);
  client_ = client;
}

void BlimpRemoteCompositorBridge::ScheduleMainFrame() {
  scheduler_.ScheduleFrameUpdate();
}

void BlimpRemoteCompositorBridge::ProcessCompositorStateUpdate(
    std::unique_ptr<cc::CompositorProtoState> compositor_proto_state) {
  remote_proto_channel_->SendCompositorProto(
      *compositor_proto_state->compositor_message);
  scheduler_.DidSendFrameUpdateToClient();

  // Activate the swap promises after the frame is queued.
  for (const auto& swap_promise : compositor_proto_state->swap_promises)
    swap_promise->DidActivate();
}

void BlimpRemoteCompositorBridge::OnProtoReceived(
    std::unique_ptr<cc::proto::CompositorMessage> proto) {
  DCHECK(proto->frame_ack());
  scheduler_.DidReceiveFrameUpdateAck();
}

void BlimpRemoteCompositorBridge::StartFrameUpdate() {
  client_->BeginMainFrame();
}

}  // namespace engine
}  // namespace blimp
