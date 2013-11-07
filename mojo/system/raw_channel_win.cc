// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/system/raw_channel.h"

#include <stddef.h>

#include "base/logging.h"

namespace mojo {
namespace system {

// -----------------------------------------------------------------------------

// Static factory method declared in raw_channel.h.
// static
RawChannel* RawChannel::Create(const PlatformChannelHandle& handle,
                               Delegate* delegate,
                               base::MessageLoop* message_loop) {
  // TODO(vtl)
  NOTIMPLEMENTED();
  return NULL;
}

}  // namespace system
}  // namespace mojo
