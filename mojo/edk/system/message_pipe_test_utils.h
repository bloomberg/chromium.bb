// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_EDK_SYSTEM_MESSAGE_PIPE_TEST_UTILS_H_
#define MOJO_EDK_SYSTEM_MESSAGE_PIPE_TEST_UTILS_H_

#include "base/message_loop/message_loop.h"
#include "base/test/test_io_thread.h"
#include "mojo/edk/embedder/simple_platform_support.h"
#include "mojo/edk/system/test_utils.h"
#include "mojo/edk/test/multiprocess_test_helper.h"
#include "mojo/edk/test/scoped_ipc_support.h"
#include "mojo/public/cpp/system/macros.h"

namespace mojo {
namespace edk {

class ChannelEndpoint;
class MessagePipe;

namespace test {

#if !defined(OS_IOS)
class MultiprocessMessagePipeTestBase : public testing::Test {
 public:
  MultiprocessMessagePipeTestBase();
  MultiprocessMessagePipeTestBase(
      base::MessageLoop::Type main_message_loop_type);
  ~MultiprocessMessagePipeTestBase() override;

 protected:
  PlatformSupport* platform_support() { return &platform_support_; }
  test::MultiprocessTestHelper* helper() { return &helper_; }

 private:
  SimplePlatformSupport platform_support_;
  base::MessageLoop message_loop_;
  base::TestIOThread test_io_thread_;
  test::ScopedIPCSupport ipc_support_;
  test::MultiprocessTestHelper helper_;

  MOJO_DISALLOW_COPY_AND_ASSIGN(MultiprocessMessagePipeTestBase);
};
#endif

}  // namespace test
}  // namespace edk
}  // namespace mojo

#endif  // MOJO_EDK_SYSTEM_MESSAGE_PIPE_TEST_UTILS_H_
