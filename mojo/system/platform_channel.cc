// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/system/platform_channel.h"

#include "base/logging.h"

namespace mojo {
namespace system {

PlatformChannel::~PlatformChannel() {
  handle_.CloseIfNecessary();
}

// static
scoped_ptr<PlatformChannel> PlatformChannel::CreateFromHandle(
    const PlatformChannelHandle& handle) {
  DCHECK(handle.is_valid());
  scoped_ptr<PlatformChannel> rv(new PlatformChannel());
  *rv->mutable_handle() = handle;
  return rv.Pass();
}

PlatformChannelHandle PlatformChannel::PassHandle() {
  DCHECK(is_valid());
  PlatformChannelHandle rv = handle_;
  handle_ = PlatformChannelHandle();
  return rv;
}

PlatformChannel::PlatformChannel() {
}

// -----------------------------------------------------------------------------

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
