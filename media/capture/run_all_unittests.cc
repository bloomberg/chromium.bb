// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "base/test/test_suite.h"
#include "base/threading/thread.h"
#include "mojo/edk/embedder/embedder.h"
#include "mojo/edk/embedder/scoped_ipc_support.h"
#include "testing/gtest/include/gtest/gtest.h"

class MojoEnabledTestEnvironment final : public testing::Environment {
 public:
  MojoEnabledTestEnvironment() : mojo_ipc_thread_("MojoIpcThread") {}

  ~MojoEnabledTestEnvironment() final = default;

  void SetUp() final {
    mojo::edk::Init();
    mojo_ipc_thread_.StartWithOptions(
        base::Thread::Options(base::MessageLoop::TYPE_IO, 0));
    mojo_ipc_support_.reset(new mojo::edk::ScopedIPCSupport(
        mojo_ipc_thread_.task_runner(),
        mojo::edk::ScopedIPCSupport::ShutdownPolicy::FAST));
    VLOG(1) << "Mojo initialized";
  }

  void TearDown() final {
    mojo_ipc_support_.reset();
    VLOG(1) << "Mojo IPC tear down";
  }

 private:
  base::Thread mojo_ipc_thread_;
  std::unique_ptr<mojo::edk::ScopedIPCSupport> mojo_ipc_support_;
};

int main(int argc, char* argv[]) {
  base::TestSuite test_suite(argc, argv);
  testing::AddGlobalTestEnvironment(new MojoEnabledTestEnvironment());
  return base::LaunchUnitTests(
      argc, argv,
      base::BindRepeating(&base::TestSuite::Run,
                          base::Unretained(&test_suite)));
}
