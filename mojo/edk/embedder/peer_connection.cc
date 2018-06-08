// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/edk/embedder/peer_connection.h"

#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "mojo/edk/system/core.h"
#include "mojo/public/cpp/platform/platform_channel.h"
#include "mojo/public/cpp/system/invitation.h"

namespace mojo {
namespace edk {

PeerConnection::PeerConnection() : token_(base::UnguessableToken::Create()) {}

PeerConnection::~PeerConnection() {
  // We send a dummy invitation over a temporary channel, re-using |token_| as
  // the name. This ensures that the connection set up by Connect(), if any,
  // will be replaced with a short-lived, self-terminating connection.
  //
  // This is a bit of a hack since Mojo does not provide any API for explicitly
  // terminating isolated connections, but this is a decision made to minimize
  // the API surface dedicated to isolated connections in anticipation of the
  // concept being deprecated eventually.
  PlatformChannel channel;
  OutgoingInvitation::SendIsolated(channel.TakeLocalEndpoint(),
                                   token_.ToString());
}

ScopedMessagePipeHandle PeerConnection::Connect(ConnectionParams params) {
  ports::PortRef peer_port;
  auto pipe = ScopedMessagePipeHandle(
      MessagePipeHandle(Core::Get()->CreatePartialMessagePipe(&peer_port)));
  Core::Get()->ConnectIsolated(std::move(params), peer_port, token_.ToString());
  return pipe;
}

}  // namespace edk
}  // namespace mojo
