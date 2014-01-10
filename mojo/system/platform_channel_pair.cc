// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/system/platform_channel_pair.h"

#include "base/logging.h"
#include "mojo/system/platform_channel.h"

namespace mojo {
namespace system {

PlatformChannelPair::~PlatformChannelPair() {
  server_handle_.CloseIfNecessary();
  client_handle_.CloseIfNecessary();
}

scoped_ptr<PlatformChannel> PlatformChannelPair::CreateServerChannel() {
  if (!server_handle_.is_valid()) {
    LOG(WARNING) << "Server handle invalid";
    return scoped_ptr<PlatformChannel>();
  }

  scoped_ptr<PlatformChannel> rv =
      PlatformChannel::CreateFromHandle(server_handle_);
  server_handle_ = PlatformChannelHandle();
  return rv.Pass();
}

scoped_ptr<PlatformChannel> PlatformChannelPair::CreateClientChannel() {
  if (!client_handle_.is_valid()) {
    LOG(WARNING) << "Client handle invalid";
    return scoped_ptr<PlatformChannel>();
  }

  scoped_ptr<PlatformChannel> rv =
      PlatformChannel::CreateFromHandle(client_handle_);
  client_handle_ = PlatformChannelHandle();
  return rv.Pass();
}

}  // namespace system
}  // namespace mojo
