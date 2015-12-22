// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "base/lazy_instance.h"
#include "base/run_loop.h"
#include "build/build_config.h"
#include "ipc/ipc_perftest_support.h"
#include "ipc/mojo/ipc_channel_mojo.h"
#include "third_party/mojo/src/mojo/edk/embedder/embedder.h"
#include "third_party/mojo/src/mojo/edk/embedder/platform_channel_pair.h"

namespace {

// This is needed because we rely on //base/test:test_support_perf and
// it provides main() which doesn't have Mojo initialization.  We need
// some way to call Init() only once before using Mojo.
struct MojoInitialier {
  MojoInitialier() {
    mojo::embedder::Init();
  }
};

base::LazyInstance<MojoInitialier> g_mojo_initializer
    = LAZY_INSTANCE_INITIALIZER;

class MojoChannelPerfTest : public IPC::test::IPCChannelPerfTestBase {
public:
  typedef IPC::test::IPCChannelPerfTestBase Super;

  MojoChannelPerfTest();

  void TearDown() override {
    IPC::test::IPCChannelPerfTestBase::TearDown();
  }

  scoped_ptr<IPC::ChannelFactory> CreateChannelFactory(
      const IPC::ChannelHandle& handle,
      base::SequencedTaskRunner* runner) override {
    return IPC::ChannelMojo::CreateServerFactory(runner, handle);
  }

  bool DidStartClient() override {
    bool ok = IPCTestBase::DidStartClient();
    DCHECK(ok);
    return ok;
  }
};

MojoChannelPerfTest::MojoChannelPerfTest() {
  g_mojo_initializer.Get();
}


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

  std::vector<mojo::embedder::PlatformChannelPair*> channels;
  for (size_t i = 0; i < 10000; ++i) {
    LOG(INFO) << "channels size: " << channels.size();
    channels.push_back(new mojo::embedder::PlatformChannelPair());
  }
}

class MojoTestClient : public IPC::test::PingPongTestClient {
 public:
  typedef IPC::test::PingPongTestClient SuperType;

  MojoTestClient();

  scoped_ptr<IPC::Channel> CreateChannel(IPC::Listener* listener) override;
};

MojoTestClient::MojoTestClient() {
  g_mojo_initializer.Get();
}

scoped_ptr<IPC::Channel> MojoTestClient::CreateChannel(
    IPC::Listener* listener) {
  return scoped_ptr<IPC::Channel>(IPC::ChannelMojo::Create(
      task_runner(), IPCTestBase::GetChannelName("PerformanceClient"),
      IPC::Channel::MODE_CLIENT, listener));
}

MULTIPROCESS_IPC_TEST_CLIENT_MAIN(PerformanceClient) {
  MojoTestClient client;
  int rv = client.RunMain();

  base::RunLoop run_loop;
  run_loop.RunUntilIdle();

  return rv;
}

}  // namespace
