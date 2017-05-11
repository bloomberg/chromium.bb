// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/edk/embedder/incoming_broker_client_invitation.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "mojo/edk/embedder/embedder_internal.h"
#include "mojo/edk/embedder/platform_channel_pair.h"
#include "mojo/edk/embedder/scoped_platform_handle.h"
#include "mojo/edk/system/core.h"

namespace mojo {
namespace edk {

IncomingBrokerClientInvitation::IncomingBrokerClientInvitation() {
  DCHECK(internal::g_core);
}

IncomingBrokerClientInvitation::~IncomingBrokerClientInvitation() = default;

void IncomingBrokerClientInvitation::Accept(ConnectionParams params) {
  internal::g_core->AcceptBrokerClientInvitation(std::move(params));
}

void IncomingBrokerClientInvitation::AcceptFromCommandLine(
    TransportProtocol protocol) {
  ScopedPlatformHandle platform_channel =
      PlatformChannelPair::PassClientHandleFromParentProcess(
          *base::CommandLine::ForCurrentProcess());
  DCHECK(platform_channel.is_valid());
  Accept(ConnectionParams(protocol, std::move(platform_channel)));
}

ScopedMessagePipeHandle IncomingBrokerClientInvitation::ExtractMessagePipe(
    const std::string& name) {
  return internal::g_core->ExtractMessagePipeFromInvitation(name);
}

}  // namespace edk
}  // namespace mojo
