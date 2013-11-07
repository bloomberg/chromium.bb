// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_COMMON_SCOPED_MESSAGE_PIPE_H_
#define MOJO_COMMON_SCOPED_MESSAGE_PIPE_H_

#include "base/basictypes.h"
#include "mojo/common/mojo_common_export.h"
#include "mojo/public/system/core.h"

namespace mojo {
namespace common {

// Simple scoper that creates a message pipe in constructor and closes it in
// destructor. Test for success by comparing handles with MOJO_HANDLE_INVALID.
class MOJO_COMMON_EXPORT ScopedMessagePipe {
 public:
  ScopedMessagePipe();
  ~ScopedMessagePipe();

  MojoHandle handle_0() const { return handle_0_; }
  MojoHandle handle_1() const { return handle_1_; }

 private:
  MojoHandle handle_0_;
  MojoHandle handle_1_;

  DISALLOW_COPY_AND_ASSIGN(ScopedMessagePipe);
};

}  // namespace common
}  // namespace mojo

#endif  // MOJO_COMMON_SCOPED_MESSAGE_PIPE_H_
