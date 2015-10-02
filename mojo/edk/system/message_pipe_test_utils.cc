// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/edk/system/message_pipe_test_utils.h"

#include "mojo/edk/system/test_utils.h"

namespace mojo {
namespace edk {
namespace test {

#if !defined(OS_IOS)
MultiprocessMessagePipeTestBase::MultiprocessMessagePipeTestBase()
    : test_io_thread_(base::TestIOThread::kAutoStart),
      ipc_support_(test_io_thread_.task_runner()) {
}

MultiprocessMessagePipeTestBase::MultiprocessMessagePipeTestBase(
    base::MessageLoop::Type main_message_loop_type)
    : message_loop_(main_message_loop_type),
      test_io_thread_(base::TestIOThread::kAutoStart),
      ipc_support_(test_io_thread_.task_runner()) {
}

MultiprocessMessagePipeTestBase::~MultiprocessMessagePipeTestBase() {
}
#endif

}  // namespace test
}  // namespace edk
}  // namespace mojo
