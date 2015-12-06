// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/edk/system/message_pipe_test_utils.h"

#include "mojo/edk/system/test_utils.h"

namespace mojo {
namespace edk {
namespace test {

#if !defined(OS_IOS)
MultiprocessMessagePipeTestBase::MultiprocessMessagePipeTestBase() {
}

MultiprocessMessagePipeTestBase::~MultiprocessMessagePipeTestBase() {
}
#endif

}  // namespace test
}  // namespace edk
}  // namespace mojo
