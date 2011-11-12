// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/pepper_plugin_delegate_impl.h"

#if defined(OS_POSIX)
#include <fcntl.h>
#include <sys/socket.h>
#endif  // defined(OS_POSIX)

#include "content/test/mock_render_process.h"
#include "testing/gtest/include/gtest/gtest.h"

class PepperPluginDelegateImplTest : public ::testing::Test {
 protected:
  MessageLoopForIO message_loop_;
  // We need a render process for ppapi::proxy::ProxyChannel to work.
  MockRenderProcess mock_process_;
};

// Try to initialize BrokerDispatcherWrapper with invalid ChannelHandle.
// Initialization should fail.
TEST_F(PepperPluginDelegateImplTest, BrokerDispatcherWrapperInitFailure) {
  BrokerDispatcherWrapper dispatcher_wrapper;
  base::ProcessHandle broker_process_handle = base::kNullProcessHandle;
  IPC::ChannelHandle invalid_channel;  // Invalid by default.

  // An invalid handle should result in a failure (false) without a LOG(FATAL),
  // such as the one in CreatePipe().  Call it twice because without the invalid
  // handle check, the posix code would hit a one-time path due to a static
  // variable and go through the LOG(FATAL) path.
  EXPECT_FALSE(dispatcher_wrapper.Init(broker_process_handle, invalid_channel));
  EXPECT_FALSE(dispatcher_wrapper.Init(broker_process_handle, invalid_channel));
}

// On valid ChannelHandle, initialization should succeed.
TEST_F(PepperPluginDelegateImplTest, BrokerDispatcherWrapperInitSuccess) {
  BrokerDispatcherWrapper dispatcher_wrapper;
  base::ProcessHandle broker_process_handle = base::kNullProcessHandle;
  const char kChannelName[] = "PepperPluginDelegateImplTestChannelName";
#if defined(OS_POSIX)
  int fds[2] = {-1, -1};
  ASSERT_EQ(0, socketpair(AF_UNIX, SOCK_STREAM, 0, fds));
  // Channel::ChannelImpl::CreatePipe needs the fd to be non-blocking.
  ASSERT_EQ(0, fcntl(fds[1], F_SETFL, O_NONBLOCK));
  base::FileDescriptor file_descriptor(fds[1], true);  // Auto close.
  IPC::ChannelHandle valid_channel(kChannelName, file_descriptor);
#else
  IPC::ChannelHandle valid_channel(kChannelName);
#endif  // defined(OS_POSIX));

  EXPECT_TRUE(dispatcher_wrapper.Init(broker_process_handle, valid_channel));

#if defined(OS_POSIX)
  EXPECT_EQ(0, ::close(fds[0]));
#endif  // defined(OS_POSIX));
}
