// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_LIB_PIPE_CONTROL_MESSAGE_PROXY_H_
#define MOJO_PUBLIC_CPP_BINDINGS_LIB_PIPE_CONTROL_MESSAGE_PROXY_H_

#include "base/macros.h"
#include "mojo/public/cpp/bindings/lib/interface_id.h"

namespace mojo {

class MessageReceiver;

namespace internal {

// Proxy for request messages defined in pipe_control_messages.mojom.
class PipeControlMessageProxy {
 public:
  // Doesn't take ownership of |receiver|. It must outlive this object.
  explicit PipeControlMessageProxy(MessageReceiver* receiver);

  void NotifyPeerEndpointClosed(InterfaceId id);
  void NotifyEndpointClosedBeforeSent(InterfaceId id);

 private:
  // Not owned.
  MessageReceiver* receiver_;

  DISALLOW_COPY_AND_ASSIGN(PipeControlMessageProxy);
};

}  // namespace internal
}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_LIB_PIPE_CONTROL_MESSAGE_PROXY_H_
