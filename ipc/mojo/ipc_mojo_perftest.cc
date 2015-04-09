// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/lazy_instance.h"
#include "base/run_loop.h"
#include "ipc/ipc_perftest_support.h"
#include "ipc/mojo/ipc_channel_mojo.h"
#include "ipc/mojo/ipc_channel_mojo_host.h"
#include "third_party/mojo/src/mojo/edk/embedder/test_embedder.h"

namespace {

// This is needed because we rely on //base/test:test_support_perf and
// it provides main() which doesn't have Mojo initialization.  We need
// some way to call InitWithSimplePlatformSupport() only once before
// using Mojo.
struct MojoInitialier {
  MojoInitialier() {
    mojo::embedder::test::InitWithSimplePlatformSupport();
  }
};

base::LazyInstance<MojoInitialier> g_mojo_initializer
    = LAZY_INSTANCE_INITIALIZER;

class MojoChannelPerfTest : public IPC::test::IPCChannelPerfTestBase {
public:
  typedef IPC::test::IPCChannelPerfTestBase Super;

  MojoChannelPerfTest();

  void TearDown() override {
    ipc_support_.reset();
    IPC::test::IPCChannelPerfTestBase::TearDown();
  }

  scoped_ptr<IPC::ChannelFactory> CreateChannelFactory(
      const IPC::ChannelHandle& handle,
      base::SequencedTaskRunner* runner) override {
    ipc_support_.reset(new IPC::ScopedIPCSupport(runner));
    host_.reset(new IPC::ChannelMojoHost(runner));
    return IPC::ChannelMojo::CreateServerFactory(host_->channel_delegate(),
                                                 handle);
  }

  bool DidStartClient() override {
    bool ok = IPCTestBase::DidStartClient();
    DCHECK(ok);
    host_->OnClientLaunched(client_process().Handle());
    return ok;
  }

 private:
  scoped_ptr<IPC::ScopedIPCSupport> ipc_support_;
  scoped_ptr<IPC::ChannelMojoHost> host_;
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

class MojoTestClient : public IPC::test::PingPongTestClient {
 public:
  typedef IPC::test::PingPongTestClient SuperType;

  MojoTestClient();

  scoped_ptr<IPC::Channel> CreateChannel(IPC::Listener* listener) override;

 private:
  scoped_ptr<IPC::ScopedIPCSupport> ipc_support_;
};

MojoTestClient::MojoTestClient() {
  g_mojo_initializer.Get();
}

scoped_ptr<IPC::Channel> MojoTestClient::CreateChannel(
    IPC::Listener* listener) {
  ipc_support_.reset(new IPC::ScopedIPCSupport(task_runner()));
  return scoped_ptr<IPC::Channel>(
      IPC::ChannelMojo::Create(NULL,
                               IPCTestBase::GetChannelName("PerformanceClient"),
                               IPC::Channel::MODE_CLIENT,
                               listener));
}

MULTIPROCESS_IPC_TEST_CLIENT_MAIN(PerformanceClient) {
  MojoTestClient client;
  int rv = client.RunMain();

  base::RunLoop run_loop;
  run_loop.RunUntilIdle();

  return rv;
}

}  // namespace
