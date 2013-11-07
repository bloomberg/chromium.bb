// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/common/scoped_message_pipe.h"

namespace mojo {
namespace common {

ScopedMessagePipe::ScopedMessagePipe()
    : handle_0_(MOJO_HANDLE_INVALID),
      handle_1_(MOJO_HANDLE_INVALID) {
  MojoCreateMessagePipe(&handle_0_, &handle_1_);
}

ScopedMessagePipe::~ScopedMessagePipe() {
  if (handle_0_ != MOJO_HANDLE_INVALID)
    MojoClose(handle_0_);

  if (handle_1_ != MOJO_HANDLE_INVALID)
    MojoClose(handle_1_);
}

}  // namespace common
}  // namespace mojo
