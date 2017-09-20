// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/test/ash_test_suite.h"
#include "base/bind.h"
#include "base/debug/debugger.h"
#include "base/process/launch.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "base/test/scoped_task_environment.h"
#include "base/threading/thread.h"
#include "build/build_config.h"
#include "components/exo/wayland/clients/test/wayland_client_test.h"
#include "mojo/edk/embedder/embedder.h"

namespace exo {
namespace {

class ExoClientPerfTestSuite : public ash::AshTestSuite {
 public:
  ExoClientPerfTestSuite(int argc, char** argv)
      : ash::AshTestSuite(argc, argv) {}

  int Run() {
    Initialize();

    base::Thread client_thread("ClientThread");
    client_thread.Start();

    base::RunLoop run_loop;
    client_thread.message_loop()->task_runner()->PostTask(
        FROM_HERE,
        base::Bind(&ExoClientPerfTestSuite::RunTestsOnClientThread,
                   base::Unretained(this), run_loop.QuitWhenIdleClosure()));
    run_loop.Run();

    Shutdown();
    return result_;
  }

  // Overriden from ash::AshTestSuite:
  void Initialize() override {
    ash::AshTestSuite::Initialize();
    if (!base::debug::BeingDebugged())
      base::RaiseProcessToHighPriority();

    // Initialize task envrionment here instead of Test::SetUp(), because all
    // tests and their SetUp() will be running in client thread.
    scoped_task_environment_ =
        std::make_unique<base::test::ScopedTaskEnvironment>(
            base::test::ScopedTaskEnvironment::MainThreadType::UI);

    // Set the UI thread message loop to WaylandClientTest, so all tests can
    // post tasks to UI thread.
    WaylandClientTest::SetUIMessageLoop(base::MessageLoop::current());
  }

  void Shutdown() override {
    WaylandClientTest::SetUIMessageLoop(nullptr);
    scoped_task_environment_ = nullptr;
    ash::AshTestSuite::Shutdown();
  }

 private:
  void RunTestsOnClientThread(const base::Closure& finished_closure) {
    result_ = RUN_ALL_TESTS();
    finished_closure.Run();
  }

  std::unique_ptr<base::test::ScopedTaskEnvironment> scoped_task_environment_;

  // Result of RUN_ALL_TESTS().
  int result_ = 1;

  DISALLOW_COPY_AND_ASSIGN(ExoClientPerfTestSuite);
};

}  // namespace
}  // namespace exo

int main(int argc, char** argv) {
  mojo::edk::Init();

  exo::ExoClientPerfTestSuite test_suite(argc, argv);

  return base::LaunchUnitTestsSerially(
      argc, argv,
      base::Bind(&exo::ExoClientPerfTestSuite::Run,
                 base::Unretained(&test_suite)));
}
