// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipc/mojo/ipc_mojo_bootstrap.h"

#include <stdint.h>

#include "base/base_paths.h"
#include "base/files/file.h"
#include "base/message_loop/message_loop.h"
#include "build/build_config.h"
#include "ipc/ipc_test_base.h"

#if defined(OS_POSIX)
#include "base/file_descriptor_posix.h"
#endif

namespace {

class IPCMojoBootstrapTest : public IPCTestBase {
 protected:
};

class TestingDelegate : public IPC::MojoBootstrap::Delegate {
 public:
  TestingDelegate() : passed_(false) {}

  void OnPipeAvailable(mojo::edk::ScopedPlatformHandle handle,
                       int32_t peer_pid) override;
  void OnBootstrapError() override;

  bool passed() const { return passed_; }

 private:
  bool passed_;
};

void TestingDelegate::OnPipeAvailable(mojo::edk::ScopedPlatformHandle handle,
                                      int32_t peer_pid) {
  passed_ = true;
  base::MessageLoop::current()->QuitWhenIdle();
}

void TestingDelegate::OnBootstrapError() {
  base::MessageLoop::current()->QuitWhenIdle();
}

// Times out on Android; see http://crbug.com/502290
#if defined(OS_ANDROID)
#define MAYBE_Connect DISABLED_Connect
#else
#define MAYBE_Connect Connect
#endif
TEST_F(IPCMojoBootstrapTest, MAYBE_Connect) {
  Init("IPCMojoBootstrapTestClient");

  TestingDelegate delegate;
  scoped_ptr<IPC::MojoBootstrap> bootstrap = IPC::MojoBootstrap::Create(
      GetTestChannelHandle(), IPC::Channel::MODE_SERVER, &delegate);

  ASSERT_TRUE(bootstrap->Connect());
#if defined(OS_POSIX)
  ASSERT_TRUE(StartClientWithFD(bootstrap->GetClientFileDescriptor()));
#else
  ASSERT_TRUE(StartClient());
#endif

  base::MessageLoop::current()->Run();

  EXPECT_TRUE(delegate.passed());
  EXPECT_TRUE(WaitForClientShutdown());
}

// A long running process that connects to us.
MULTIPROCESS_IPC_TEST_CLIENT_MAIN(IPCMojoBootstrapTestClient) {
  base::MessageLoopForIO main_message_loop;

  TestingDelegate delegate;
  scoped_ptr<IPC::MojoBootstrap> bootstrap = IPC::MojoBootstrap::Create(
      IPCTestBase::GetChannelName("IPCMojoBootstrapTestClient"),
      IPC::Channel::MODE_CLIENT, &delegate);

  bootstrap->Connect();

  base::MessageLoop::current()->Run();

  EXPECT_TRUE(delegate.passed());

  return 0;
}

}  // namespace
