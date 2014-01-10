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

}  // namespace system
}  // namespace mojo
