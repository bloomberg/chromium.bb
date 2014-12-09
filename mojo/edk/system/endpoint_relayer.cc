// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/edk/system/endpoint_relayer.h"

#include "base/logging.h"
#include "mojo/edk/system/channel_endpoint.h"
#include "mojo/edk/system/message_in_transit.h"

namespace mojo {
namespace system {

EndpointRelayer::EndpointRelayer() {
}

// static
unsigned EndpointRelayer::GetPeerPort(unsigned port) {
  DCHECK(port == 0 || port == 1);
  return port ^ 1;
}

void EndpointRelayer::Init(ChannelEndpoint* endpoint0,
                           ChannelEndpoint* endpoint1) {
  DCHECK(endpoint0);
  DCHECK(endpoint1);
  DCHECK(!endpoints_[0]);
  DCHECK(!endpoints_[1]);
  endpoints_[0] = endpoint0;
  endpoints_[1] = endpoint1;
}

bool EndpointRelayer::OnReadMessage(unsigned port, MessageInTransit* message) {
  DCHECK(message);

  base::AutoLock locker(lock_);

  // If we're no longer the client, then reject the message.
  if (!endpoints_[port])
    return false;

  // Otherwise, consume it even if the peer port is closed.
  unsigned peer_port = GetPeerPort(port);
  if (endpoints_[peer_port])
    endpoints_[peer_port]->EnqueueMessage(make_scoped_ptr(message));
  return true;
}

void EndpointRelayer::OnDetachFromChannel(unsigned port) {
  base::AutoLock locker(lock_);

  if (endpoints_[port]) {
    endpoints_[port]->DetachFromClient();
    endpoints_[port] = nullptr;
  }

  unsigned peer_port = GetPeerPort(port);
  if (endpoints_[peer_port]) {
    endpoints_[peer_port]->DetachFromClient();
    endpoints_[peer_port] = nullptr;
  }
}

EndpointRelayer::~EndpointRelayer() {
  DCHECK(!endpoints_[0]);
  DCHECK(!endpoints_[1]);
}

}  // namespace system
}  // namespace mojo
