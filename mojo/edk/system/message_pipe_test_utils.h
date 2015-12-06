// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_EDK_SYSTEM_MESSAGE_PIPE_TEST_UTILS_H_
#define MOJO_EDK_SYSTEM_MESSAGE_PIPE_TEST_UTILS_H_

#include "mojo/edk/test/multiprocess_test_helper.h"
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
  ~MultiprocessMessagePipeTestBase() override;

 protected:
  test::MultiprocessTestHelper* helper() { return &helper_; }

 private:
  test::MultiprocessTestHelper helper_;

  MOJO_DISALLOW_COPY_AND_ASSIGN(MultiprocessMessagePipeTestBase);
};
#endif

}  // namespace test
}  // namespace edk
}  // namespace mojo

#endif  // MOJO_EDK_SYSTEM_MESSAGE_PIPE_TEST_UTILS_H_
