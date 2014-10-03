// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/lazy_instance.h"
#include "ipc/ipc_perftest_support.h"
#include "ipc/mojo/ipc_channel_mojo.h"
#include "ipc/mojo/ipc_channel_mojo_host.h"
#include "mojo/embedder/test_embedder.h"

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

  virtual scoped_ptr<IPC::ChannelFactory> CreateChannelFactory(
      const IPC::ChannelHandle& handle,
      base::TaskRunner* runner) OVERRIDE {
    host_.reset(new IPC::ChannelMojoHost(task_runner()));
    return IPC::ChannelMojo::CreateServerFactory(host_->channel_delegate(),
                                                 handle);
  }

  virtual bool DidStartClient() OVERRIDE {
    bool ok = IPCTestBase::DidStartClient();
    DCHECK(ok);
    host_->OnClientLaunched(client_process());
    return ok;
  }

  void set_io_thread_task_runner(base::TaskRunner* runner) {
    io_thread_task_runner_ = runner;
  }

 private:
  base::TaskRunner* io_thread_task_runner_;
  scoped_ptr<IPC::ChannelMojoHost> host_;
};

MojoChannelPerfTest::MojoChannelPerfTest()
    : io_thread_task_runner_() {
  g_mojo_initializer.Get();
}


TEST_F(MojoChannelPerfTest, ChannelPingPong) {
  RunTestChannelPingPong(GetDefaultTestParams());
}

TEST_F(MojoChannelPerfTest, ChannelProxyPingPong) {
  RunTestChannelProxyPingPong(GetDefaultTestParams());
}

class MojoTestClient : public IPC::test::PingPongTestClient {
 public:
  typedef IPC::test::PingPongTestClient SuperType;

  MojoTestClient();

  virtual scoped_ptr<IPC::Channel> CreateChannel(
      IPC::Listener* listener) OVERRIDE;
};

MojoTestClient::MojoTestClient() {
  g_mojo_initializer.Get();
}

scoped_ptr<IPC::Channel> MojoTestClient::CreateChannel(
    IPC::Listener* listener) {
  return scoped_ptr<IPC::Channel>(
      IPC::ChannelMojo::Create(NULL,
                               IPCTestBase::GetChannelName("PerformanceClient"),
                               IPC::Channel::MODE_CLIENT,
                               listener));
}

MULTIPROCESS_IPC_TEST_CLIENT_MAIN(PerformanceClient) {
  MojoTestClient client;
  return client.RunMain();
}

}  // namespace
