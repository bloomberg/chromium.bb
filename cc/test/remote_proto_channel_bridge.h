// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_REMOTE_PROTO_CHANNEL_BRIDGE_H_
#define CC_TEST_REMOTE_PROTO_CHANNEL_BRIDGE_H_

#include "base/macros.h"
#include "cc/test/test_hooks.h"
#include "cc/trees/remote_proto_channel.h"

// The RemoteProtoChannelBridge mocks out the external network layer for the
// unit tests by sending messages directly between the RemoteProtoChannels for
// the server and client LayerTreeHost.
namespace cc {
class RemoteProtoChannelBridge;

class FakeRemoteProtoChannel : public RemoteProtoChannel {
 public:
  explicit FakeRemoteProtoChannel(RemoteProtoChannelBridge* bridge);
  ~FakeRemoteProtoChannel() override;

  // RemoteProtoChannel implementation
  void SetProtoReceiver(ProtoReceiver* receiver) override;

  virtual void OnProtoReceived(scoped_ptr<proto::CompositorMessage> proto);

  bool HasReceiver() const;

 protected:
  RemoteProtoChannelBridge* bridge_;

 private:
  RemoteProtoChannel::ProtoReceiver* receiver_;
};

class FakeRemoteProtoChannelMain : public FakeRemoteProtoChannel {
 public:
  FakeRemoteProtoChannelMain(RemoteProtoChannelBridge* bridge,
                             TestHooks* test_hooks);

  void SendCompositorProto(const proto::CompositorMessage& proto) override;

 private:
  TestHooks* test_hooks_;
};

class FakeRemoteProtoChannelImpl : public FakeRemoteProtoChannel {
 public:
  explicit FakeRemoteProtoChannelImpl(RemoteProtoChannelBridge* bridge);

  void SendCompositorProto(const proto::CompositorMessage& proto) override;
};

class RemoteProtoChannelBridge {
 public:
  explicit RemoteProtoChannelBridge(TestHooks* test_hooks);
  ~RemoteProtoChannelBridge();

  FakeRemoteProtoChannelMain channel_main;
  FakeRemoteProtoChannelImpl channel_impl;
};

}  // namespace cc

#endif  // CC_TEST_REMOTE_PROTO_CHANNEL_BRIDGE_H_
