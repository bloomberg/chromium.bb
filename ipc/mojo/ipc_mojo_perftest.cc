// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "base/run_loop.h"
#include "base/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "ipc/ipc_perftest_support.h"
#include "ipc/mojo/ipc_channel_mojo.h"
#include "mojo/edk/embedder/embedder.h"
#include "mojo/edk/embedder/platform_channel_pair.h"
#include "mojo/edk/test/multiprocess_test_helper.h"
#include "mojo/edk/test/scoped_ipc_support.h"

namespace IPC {
namespace {

const char kPerftestToken[] = "perftest-token";

class MojoChannelPerfTest : public test::IPCChannelPerfTestBase {
 public:
  MojoChannelPerfTest() { token_ = mojo::edk::GenerateRandomToken(); }

  void TearDown() override {
    {
      base::AutoLock l(ipc_support_lock_);
      ipc_support_.reset();
    }
    test::IPCChannelPerfTestBase::TearDown();
  }

  scoped_ptr<ChannelFactory> CreateChannelFactory(
      const ChannelHandle& handle,
      base::SequencedTaskRunner* runner) override {
    EnsureIpcSupport();
    return ChannelMojo::CreateServerFactory(token_);
  }

  bool StartClient() override {
    EnsureIpcSupport();
    helper_.StartChildWithExtraSwitch("MojoPerfTestClient", kPerftestToken,
                                      token_);
    return true;
  }

  bool WaitForClientShutdown() override {
    return helper_.WaitForChildTestShutdown();
  }

  void EnsureIpcSupport() {
    base::AutoLock l(ipc_support_lock_);
    if (!ipc_support_) {
      ipc_support_.reset(
          new mojo::edk::test::ScopedIPCSupport(io_task_runner()));
    }
  }

  mojo::edk::test::MultiprocessTestHelper helper_;
  std::string token_;
  base::Lock ipc_support_lock_;
  scoped_ptr<mojo::edk::test::ScopedIPCSupport> ipc_support_;
};

TEST_F(MojoChannelPerfTest, ChannelPingPong) {
  RunTestChannelPingPong(GetDefaultTestParams());

  base::RunLoop run_loop;
  run_loop.RunUntilIdle();
}

TEST_F(MojoChannelPerfTest, ChannelProxyPingPong) {
  RunTestChannelProxyPingPong(GetDefaultTestParams());

  base::RunLoop run_loop;
  run_loop.RunUntilIdle();
}

// Test to see how many channels we can create.
TEST_F(MojoChannelPerfTest, DISABLED_MaxChannelCount) {
#if defined(OS_POSIX)
  LOG(INFO) << "base::GetMaxFds " << base::GetMaxFds();
  base::SetFdLimit(20000);
#endif

  std::vector<mojo::edk::PlatformChannelPair*> channels;
  for (size_t i = 0; i < 10000; ++i) {
    LOG(INFO) << "channels size: " << channels.size();
    channels.push_back(new mojo::edk::PlatformChannelPair());
  }
}

class MojoPerfTestClient : public test::PingPongTestClient {
 public:
  typedef test::PingPongTestClient SuperType;

  MojoPerfTestClient();

  scoped_ptr<Channel> CreateChannel(Listener* listener) override;

 private:
  mojo::edk::test::ScopedIPCSupport ipc_support_;
};

MojoPerfTestClient::MojoPerfTestClient()
    : ipc_support_(base::ThreadTaskRunnerHandle::Get()) {
  mojo::edk::test::MultiprocessTestHelper::ChildSetup();
}

scoped_ptr<Channel> MojoPerfTestClient::CreateChannel(Listener* listener) {
  return scoped_ptr<Channel>(ChannelMojo::Create(
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          kPerftestToken),
      Channel::MODE_CLIENT, listener));
}

MULTIPROCESS_TEST_MAIN(MojoPerfTestClientTestChildMain) {
  MojoPerfTestClient client;

  int rv = client.RunMain();

  base::RunLoop run_loop;
  run_loop.RunUntilIdle();

  return rv;
}

}  // namespace
}  // namespace IPC
