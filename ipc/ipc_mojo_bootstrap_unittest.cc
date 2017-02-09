// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipc/ipc_mojo_bootstrap.h"

#include <stdint.h>
#include <memory>

#include "base/base_paths.h"
#include "base/files/file.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "ipc/ipc.mojom.h"
#include "ipc/ipc_test_base.h"
#include "mojo/edk/embedder/embedder.h"
#include "mojo/edk/test/mojo_test_base.h"
#include "mojo/edk/test/multiprocess_test_helper.h"
#include "mojo/public/cpp/bindings/associated_binding.h"

#if defined(OS_POSIX)
#include "base/file_descriptor_posix.h"
#endif

namespace {

constexpr int32_t kTestServerPid = 42;
constexpr int32_t kTestClientPid = 4242;

class PeerPidReceiver : public IPC::mojom::Channel {
 public:
  PeerPidReceiver(IPC::mojom::ChannelAssociatedRequest request,
                  const base::Closure& on_peer_pid_set)
      : binding_(this, std::move(request)), on_peer_pid_set_(on_peer_pid_set) {}
  ~PeerPidReceiver() override {}

  // mojom::Channel:
  void SetPeerPid(int32_t pid) override {
    peer_pid_ = pid;
    on_peer_pid_set_.Run();
  }

  void Receive(const std::vector<uint8_t>& data,
               base::Optional<std::vector<IPC::mojom::SerializedHandlePtr>>
                   handles) override {}

  void GetAssociatedInterface(
      const std::string& name,
      IPC::mojom::GenericInterfaceAssociatedRequest request) override {}

  int32_t peer_pid() const { return peer_pid_; }

 private:
  mojo::AssociatedBinding<IPC::mojom::Channel> binding_;
  const base::Closure on_peer_pid_set_;
  int32_t peer_pid_ = -1;

  DISALLOW_COPY_AND_ASSIGN(PeerPidReceiver);
};

class IPCMojoBootstrapTest : public testing::Test {
 protected:
  mojo::edk::test::MultiprocessTestHelper helper_;
};

TEST_F(IPCMojoBootstrapTest, Connect) {
  base::MessageLoop message_loop;
  std::unique_ptr<IPC::MojoBootstrap> bootstrap = IPC::MojoBootstrap::Create(
      helper_.StartChild("IPCMojoBootstrapTestClient"),
      IPC::Channel::MODE_SERVER, base::ThreadTaskRunnerHandle::Get());

  IPC::mojom::ChannelAssociatedPtr sender;
  IPC::mojom::ChannelAssociatedRequest receiver;
  bootstrap->Connect(&sender, &receiver);
  sender->SetPeerPid(kTestServerPid);

  base::RunLoop run_loop;
  PeerPidReceiver impl(std::move(receiver), run_loop.QuitClosure());
  run_loop.Run();

  EXPECT_EQ(kTestClientPid, impl.peer_pid());

  EXPECT_TRUE(helper_.WaitForChildTestShutdown());
}

// A long running process that connects to us.
MULTIPROCESS_TEST_MAIN_WITH_SETUP(
    IPCMojoBootstrapTestClientTestChildMain,
    ::mojo::edk::test::MultiprocessTestHelper::ChildSetup) {
  base::MessageLoop message_loop;
  std::unique_ptr<IPC::MojoBootstrap> bootstrap = IPC::MojoBootstrap::Create(
      std::move(mojo::edk::test::MultiprocessTestHelper::primordial_pipe),
      IPC::Channel::MODE_CLIENT, base::ThreadTaskRunnerHandle::Get());

  IPC::mojom::ChannelAssociatedPtr sender;
  IPC::mojom::ChannelAssociatedRequest receiver;
  bootstrap->Connect(&sender, &receiver);
  sender->SetPeerPid(kTestClientPid);

  base::RunLoop run_loop;
  PeerPidReceiver impl(std::move(receiver), run_loop.QuitClosure());
  run_loop.Run();

  EXPECT_EQ(kTestServerPid, impl.peer_pid());

  return 0;
}

}  // namespace
