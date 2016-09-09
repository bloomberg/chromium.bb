// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/remote_proto_channel_bridge.h"

#include <memory>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "cc/proto/compositor_message.pb.h"

namespace cc {

FakeRemoteProtoChannel::FakeRemoteProtoChannel(RemoteProtoChannelBridge* bridge)
    : bridge_(bridge), receiver_(nullptr) {
  DCHECK(bridge_);
}

FakeRemoteProtoChannel::~FakeRemoteProtoChannel() {}

void FakeRemoteProtoChannel::SetProtoReceiver(ProtoReceiver* receiver) {
  receiver_ = receiver;
}

void FakeRemoteProtoChannel::OnProtoReceived(
    std::unique_ptr<proto::CompositorMessage> proto) {
  DCHECK(receiver_);

  receiver_->OnProtoReceived(std::move(proto));
}

bool FakeRemoteProtoChannel::HasReceiver() const {
  return receiver_ != nullptr;
}

FakeRemoteProtoChannelMain::FakeRemoteProtoChannelMain(
    RemoteProtoChannelBridge* bridge)
    : FakeRemoteProtoChannel(bridge) {}

void FakeRemoteProtoChannelMain::SendCompositorProto(
    const proto::CompositorMessage& proto) {
  DCHECK(proto.has_to_impl());

  // Check for CompositorMessageToImpl::InitializeImpl and CloseImpl message
  // types to create and destroy the remote client LayerTreeHost.
  proto::CompositorMessageToImpl to_impl_proto = proto.to_impl();
  switch (to_impl_proto.message_type()) {
    case proto::CompositorMessageToImpl::UNKNOWN:
      return;
    default:
      bridge_->channel_impl.OnProtoReceived(
          base::MakeUnique<proto::CompositorMessage>(proto));
  }
}

FakeRemoteProtoChannelImpl::FakeRemoteProtoChannelImpl(
    RemoteProtoChannelBridge* bridge)
    : FakeRemoteProtoChannel(bridge) {}

void FakeRemoteProtoChannelImpl::SendCompositorProto(
    const proto::CompositorMessage& proto) {
  bridge_->channel_main.OnProtoReceived(
      base::MakeUnique<proto::CompositorMessage>(proto));
}

RemoteProtoChannelBridge::RemoteProtoChannelBridge()
    : channel_main(this), channel_impl(this) {}

RemoteProtoChannelBridge::~RemoteProtoChannelBridge() {}

}  // namespace cc
