// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TREES_REMOTE_PROTO_CHANNEL_H_
#define CC_TREES_REMOTE_PROTO_CHANNEL_H_

#include "base/memory/scoped_ptr.h"

namespace cc {

namespace proto {
class CompositorMessage;
}

// Provides a bridge for getting compositor protobuf messages to/from the
// outside world.
class RemoteProtoChannel {
 public:
  // Meant to be implemented by a RemoteChannel that needs to receive and parse
  // incoming protobufs.
  class ProtoReceiver {
   public:
    virtual void OnProtoReceived(
        scoped_ptr<proto::CompositorMessage> proto) = 0;

   protected:
    virtual ~ProtoReceiver() {}
  };

  virtual void SetProtoReceiver(ProtoReceiver* receiver) = 0;
  virtual void SendCompositorProto(const proto::CompositorMessage& proto) = 0;

 protected:
  virtual ~RemoteProtoChannel() {}
};

}  // namespace cc

#endif  // CC_TREES_REMOTE_PROTO_CHANNEL_H_
