// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/base_paths.h"
#include "base/base_switches.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "base/path_service.h"
#include "base/process/process.h"
#include "base/run_loop.h"
#include "mojo/edk/embedder/embedder.h"
#include "mojo/edk/embedder/platform_channel_pair.h"
#include "mojo/edk/embedder/scoped_platform_handle.h"
#include "mojo/shell/public/cpp/shell_test.h"
#include "mojo/shell/public/interfaces/shell.mojom.h"
#include "mojo/shell/runner/common/switches.h"
#include "mojo/shell/tests/lifecycle/lifecycle_unittest.mojom.h"

namespace mojo {
namespace shell {
namespace {

const char kTestAppName[] = "mojo:lifecycle_unittest_app";
const char kTestExeName[] = "exe:lifecycle_unittest_exe";
const char kTestPackageName[] = "mojo:lifecycle_unittest_package";
const char kTestPackageAppNameA[] = "mojo:lifecycle_unittest_package_app_a";
const char kTestPackageAppNameB[] = "mojo:lifecycle_unittest_package_app_b";
const char kTestName[] = "mojo:lifecycle_unittest";

void QuitLoop(base::RunLoop* loop) {
  loop->Quit();
}

void DecrementCountAndQuitWhenZero(base::RunLoop* loop, size_t* count) {
  if (!--(*count))
    loop->Quit();
}

struct Instance {
  Instance() : id(mojom::Connector::kInvalidApplicationID), pid(0) {}
  Instance(const std::string& name, const std::string& qualifier, uint32_t id,
           uint32_t pid)
      : name(name), qualifier(qualifier), id(id), pid(pid) {}

  std::string name;
  std::string qualifier;
  uint32_t id;
  uint32_t pid;
};

class InstanceState : public mojom::InstanceListener {
 public:
  InstanceState(mojom::InstanceListenerRequest request, base::RunLoop* loop)
      : binding_(this, std::move(request)), loop_(loop) {}
  ~InstanceState() override {}

  bool HasInstanceForName(const std::string& name) const {
    return instances_.find(name) != instances_.end();
  }
  size_t GetNewInstanceCount() const {
    return instances_.size() - initial_instances_.size();
  }
  void WaitForInstanceDestruction(base::RunLoop* loop) {
    DCHECK(!destruction_loop_);
    destruction_loop_ = loop;
    // First of all check to see if we should be spinning this loop at all -
    // the app(s) we're waiting on quitting may already have quit.
    TryToQuitDestructionLoop();
  }

 private:
  // mojom::InstanceListener:
  void SetExistingInstances(Array<mojom::InstanceInfoPtr> instances) override {
    for (const auto& instance : instances) {
      Instance i(instance->name, instance->qualifier, instance->id,
                 instance->pid);
      initial_instances_[i.name] = i;
      instances_[i.name] = i;
    }
    loop_->Quit();
  }
  void InstanceCreated(mojom::InstanceInfoPtr instance) override {
    instances_[instance->name] =
       Instance(instance->name, instance->qualifier, instance->id,
                instance->pid);
  }
  void InstanceDestroyed(uint32_t id) override {
    for (auto it = instances_.begin(); it != instances_.end(); ++it) {
      if (it->second.id == id) {
        instances_.erase(it);
        break;
      }
    }
    TryToQuitDestructionLoop();
  }
  void InstancePIDAvailable(uint32_t id, uint32_t pid) override {
    for (auto& instance : instances_) {
      if (instance.second.id == id) {
        instance.second.pid = pid;
        break;
      }
    }
  }

  void TryToQuitDestructionLoop() {
    if (!GetNewInstanceCount() && destruction_loop_) {
      destruction_loop_->Quit();
      destruction_loop_ = nullptr;
    }
  }

  // All currently running instances.
  std::map<std::string, Instance> instances_;
  // The initial set of instances.
  std::map<std::string, Instance> initial_instances_;

  Binding<mojom::InstanceListener> binding_;
  base::RunLoop* loop_;

  // Set when the client wants to wait for this object to track the destruction
  // of an instance before proceeding.
  base::RunLoop* destruction_loop_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(InstanceState);
};

}

class LifecycleTest : public mojo::test::ShellTest {
 public:
  LifecycleTest() : ShellTest(kTestName) {}
  ~LifecycleTest() override {}

 protected:
  // mojo::test::ShellTest:
  void SetUp() override {
    mojo::test::ShellTest::SetUp();
    InitPackage();
    instances_ = TrackInstances();
  }
  void TearDown() override {
    instances_.reset();
    mojo::test::ShellTest::TearDown();
  }

  bool CanRunCrashTest() {
    return !base::CommandLine::ForCurrentProcess()->HasSwitch("single-process");
  }

  void InitPackage() {
    test::mojom::LifecycleControlPtr lifecycle = ConnectTo(kTestPackageName);
    base::RunLoop loop;
    lifecycle.set_connection_error_handler(base::Bind(&QuitLoop, &loop));
    lifecycle->GracefulQuit();
    loop.Run();
  }

  test::mojom::LifecycleControlPtr ConnectTo(const std::string& name) {
    test::mojom::LifecycleControlPtr lifecycle;
    connector()->ConnectToInterface(name, &lifecycle);
    PingPong(lifecycle.get());
    return lifecycle;
  }

  base::Process LaunchProcess() {
    base::FilePath target_path;
    CHECK(base::PathService::Get(base::DIR_EXE, &target_path));
  #if defined(OS_WIN)
    target_path = target_path.Append(
        FILE_PATH_LITERAL("lifecycle_unittest_exe.exe"));
  #else
    target_path = target_path.Append(
        FILE_PATH_LITERAL("lifecycle_unittest_exe"));
  #endif

    base::CommandLine child_command_line(target_path);
    // Forward the wait-for-debugger flag but nothing else - we don't want to
    // stamp on the platform-channel flag.
    if (base::CommandLine::ForCurrentProcess()->HasSwitch(
            switches::kWaitForDebugger)) {
      child_command_line.AppendSwitch(switches::kWaitForDebugger);
    }

    mojo::shell::mojom::PIDReceiverPtr receiver;
    mojo::InterfaceRequest<mojo::shell::mojom::PIDReceiver> request =
        GetProxy(&receiver);

    // Create the channel to be shared with the target process. Pass one end
    // on the command line.
    mojo::edk::PlatformChannelPair platform_channel_pair;
    mojo::edk::HandlePassingInformation handle_passing_info;
    platform_channel_pair.PrepareToPassClientHandleToChildProcess(
        &child_command_line, &handle_passing_info);

    // Generate a token for the child to find and connect to a primordial pipe
    // and pass that as well.
    std::string primordial_pipe_token = mojo::edk::GenerateRandomToken();
    child_command_line.AppendSwitchASCII(switches::kPrimordialPipeToken,
                                         primordial_pipe_token);

    // Allocate the pipe locally.
    mojo::ScopedMessagePipeHandle pipe =
        mojo::edk::CreateParentMessagePipe(primordial_pipe_token);

    mojo::shell::mojom::CapabilityFilterPtr filter(
        mojo::shell::mojom::CapabilityFilter::New());
    mojo::Array<mojo::String> test_interfaces;
    test_interfaces.push_back("*");
    filter->filter.insert("*", std::move(test_interfaces));

    mojo::shell::mojom::ShellPtr shell;
    connector()->ConnectToInterface("mojo:shell", &shell);

    mojo::shell::mojom::ShellClientFactoryPtr factory;
    factory.Bind(mojo::InterfacePtrInfo<mojo::shell::mojom::ShellClientFactory>(
        std::move(pipe), 0u));

    shell->CreateInstanceForFactory(std::move(factory), kTestExeName,
                                    mojom::Connector::kUserInherit,
                                    std::move(filter), std::move(request));

    base::LaunchOptions options;
  #if defined(OS_WIN)
    options.handles_to_inherit = &handle_passing_info;
  #elif defined(OS_POSIX)
    options.fds_to_remap = &handle_passing_info;
  #endif
    base::Process process = base::LaunchProcess(child_command_line, options);
    DCHECK(process.IsValid());
    receiver->SetPID(process.Pid());
    mojo::edk::ChildProcessLaunched(process.Handle(),
                                    platform_channel_pair.PassServerHandle());
    return process;
  }

  void PingPong(test::mojom::LifecycleControl* lifecycle) {
    base::RunLoop loop;
    lifecycle->Ping(base::Bind(&QuitLoop, &loop));
    loop.Run();
  }

  InstanceState* instances() { return instances_.get(); }

  void WaitForInstanceDestruction() {
    base::RunLoop loop;
    instances()->WaitForInstanceDestruction(&loop);
    loop.Run();
  }

 private:
  scoped_ptr<InstanceState> TrackInstances() {
    mojom::ShellPtr shell;
    connector()->ConnectToInterface("mojo:shell", &shell);
    mojom::InstanceListenerPtr listener;
    base::RunLoop loop;
    InstanceState* state = new InstanceState(GetProxy(&listener), &loop);
    shell->AddInstanceListener(std::move(listener));
    loop.Run();
    return make_scoped_ptr(state);
  }

  scoped_ptr<InstanceState> instances_;

  DISALLOW_COPY_AND_ASSIGN(LifecycleTest);
};

TEST_F(LifecycleTest, Standalone_GracefulQuit) {
  test::mojom::LifecycleControlPtr lifecycle = ConnectTo(kTestAppName);

  EXPECT_TRUE(instances()->HasInstanceForName(kTestAppName));
  EXPECT_EQ(1u, instances()->GetNewInstanceCount());

  base::RunLoop loop;
  lifecycle.set_connection_error_handler(base::Bind(&QuitLoop, &loop));
  lifecycle->GracefulQuit();
  loop.Run();

  WaitForInstanceDestruction();
  EXPECT_FALSE(instances()->HasInstanceForName(kTestAppName));
  EXPECT_EQ(0u, instances()->GetNewInstanceCount());
}

TEST_F(LifecycleTest, Standalone_Crash) {
  if (!CanRunCrashTest()) {
    LOG(INFO) << "Skipping Standalone_Crash test in --single-process mode.";
    return;
  }

  test::mojom::LifecycleControlPtr lifecycle = ConnectTo(kTestAppName);

  EXPECT_TRUE(instances()->HasInstanceForName(kTestAppName));
  EXPECT_EQ(1u, instances()->GetNewInstanceCount());

  base::RunLoop loop;
  lifecycle.set_connection_error_handler(base::Bind(&QuitLoop, &loop));
  lifecycle->Crash();
  loop.Run();

  WaitForInstanceDestruction();
  EXPECT_FALSE(instances()->HasInstanceForName(kTestAppName));
  EXPECT_EQ(0u, instances()->GetNewInstanceCount());
}

TEST_F(LifecycleTest, Standalone_CloseShellConnection) {
  test::mojom::LifecycleControlPtr lifecycle = ConnectTo(kTestAppName);

  EXPECT_TRUE(instances()->HasInstanceForName(kTestAppName));
  EXPECT_EQ(1u, instances()->GetNewInstanceCount());

  base::RunLoop loop;
  lifecycle.set_connection_error_handler(base::Bind(&QuitLoop, &loop));
  lifecycle->CloseShellConnection();

  WaitForInstanceDestruction();

  // |lifecycle| pipe should still be valid.
  PingPong(lifecycle.get());
}

TEST_F(LifecycleTest, PackagedApp_GracefulQuit) {
  test::mojom::LifecycleControlPtr lifecycle = ConnectTo(kTestPackageAppNameA);

  // There should be two new instances - one for the app and one for the package
  // that vended it.
  EXPECT_TRUE(instances()->HasInstanceForName(kTestPackageAppNameA));
  EXPECT_TRUE(instances()->HasInstanceForName(kTestPackageName));
  EXPECT_EQ(2u, instances()->GetNewInstanceCount());

  base::RunLoop loop;
  lifecycle.set_connection_error_handler(base::Bind(&QuitLoop, &loop));
  lifecycle->GracefulQuit();
  loop.Run();

  WaitForInstanceDestruction();
  EXPECT_FALSE(instances()->HasInstanceForName(kTestPackageName));
  EXPECT_FALSE(instances()->HasInstanceForName(kTestAppName));
  EXPECT_EQ(0u, instances()->GetNewInstanceCount());
}

TEST_F(LifecycleTest, PackagedApp_Crash) {
  if (!CanRunCrashTest()) {
    LOG(INFO) << "Skipping Standalone_Crash test in --single-process mode.";
    return;
  }

  test::mojom::LifecycleControlPtr lifecycle = ConnectTo(kTestPackageAppNameA);

  // There should be two new instances - one for the app and one for the package
  // that vended it.
  EXPECT_TRUE(instances()->HasInstanceForName(kTestPackageAppNameA));
  EXPECT_TRUE(instances()->HasInstanceForName(kTestPackageName));
  EXPECT_EQ(2u, instances()->GetNewInstanceCount());

  base::RunLoop loop;
  lifecycle.set_connection_error_handler(base::Bind(&QuitLoop, &loop));
  lifecycle->Crash();
  loop.Run();

  WaitForInstanceDestruction();
  EXPECT_FALSE(instances()->HasInstanceForName(kTestPackageName));
  EXPECT_FALSE(instances()->HasInstanceForName(kTestPackageAppNameA));
  EXPECT_EQ(0u, instances()->GetNewInstanceCount());
}

TEST_F(LifecycleTest, PackagedApp_CloseShellConnection) {
  test::mojom::LifecycleControlPtr lifecycle = ConnectTo(kTestPackageAppNameA);

  EXPECT_TRUE(instances()->HasInstanceForName(kTestPackageAppNameA));
  EXPECT_TRUE(instances()->HasInstanceForName(kTestPackageName));
  EXPECT_EQ(2u, instances()->GetNewInstanceCount());

  base::RunLoop loop;
  lifecycle.set_connection_error_handler(base::Bind(&QuitLoop, &loop));
  lifecycle->CloseShellConnection();

  WaitForInstanceDestruction();
  EXPECT_FALSE(instances()->HasInstanceForName(kTestPackageAppNameA));
  EXPECT_FALSE(instances()->HasInstanceForName(kTestPackageName));
  EXPECT_EQ(0u, instances()->GetNewInstanceCount());

  // |lifecycle| pipe should still be valid.
  PingPong(lifecycle.get());
}


// When a single package provides multiple apps out of one process, crashing one
// app crashes all.
TEST_F(LifecycleTest, PackagedApp_CrashCrashesOtherProvidedApp) {
  if (!CanRunCrashTest()) {
    LOG(INFO) << "Skipping Standalone_Crash test in --single-process mode.";
    return;
  }

  test::mojom::LifecycleControlPtr lifecycle_a =
      ConnectTo(kTestPackageAppNameA);
  test::mojom::LifecycleControlPtr lifecycle_b =
      ConnectTo(kTestPackageAppNameB);
  test::mojom::LifecycleControlPtr lifecycle_package =
      ConnectTo(kTestPackageName);

  // There should be three instances, one for each packaged app and the package
  // itself.
  EXPECT_TRUE(instances()->HasInstanceForName(kTestPackageAppNameA));
  EXPECT_TRUE(instances()->HasInstanceForName(kTestPackageAppNameB));
  EXPECT_TRUE(instances()->HasInstanceForName(kTestPackageName));
  size_t instance_count = instances()->GetNewInstanceCount();
  EXPECT_EQ(3u, instance_count);

  base::RunLoop loop;
  lifecycle_a.set_connection_error_handler(
      base::Bind(&DecrementCountAndQuitWhenZero, &loop, &instance_count));
  lifecycle_b.set_connection_error_handler(
      base::Bind(&DecrementCountAndQuitWhenZero, &loop, &instance_count));
  lifecycle_package.set_connection_error_handler(
      base::Bind(&DecrementCountAndQuitWhenZero, &loop, &instance_count));

  // Now crash one of the packaged apps.
  lifecycle_a->Crash();
  loop.Run();

  WaitForInstanceDestruction();
  EXPECT_FALSE(instances()->HasInstanceForName(kTestPackageName));
  EXPECT_FALSE(instances()->HasInstanceForName(kTestPackageAppNameA));
  EXPECT_FALSE(instances()->HasInstanceForName(kTestPackageAppNameB));
  EXPECT_EQ(0u, instances()->GetNewInstanceCount());
}

// When a single package provides multiple apps out of one process, crashing one
// app crashes all.
TEST_F(LifecycleTest, PackagedApp_GracefulQuitPackageQuitsAll) {
  test::mojom::LifecycleControlPtr lifecycle_a =
      ConnectTo(kTestPackageAppNameA);
  test::mojom::LifecycleControlPtr lifecycle_b =
      ConnectTo(kTestPackageAppNameB);
  test::mojom::LifecycleControlPtr lifecycle_package =
      ConnectTo(kTestPackageName);

  // There should be three instances, one for each packaged app and the package
  // itself.
  EXPECT_TRUE(instances()->HasInstanceForName(kTestPackageAppNameA));
  EXPECT_TRUE(instances()->HasInstanceForName(kTestPackageAppNameB));
  EXPECT_TRUE(instances()->HasInstanceForName(kTestPackageName));
  size_t instance_count = instances()->GetNewInstanceCount();
  EXPECT_EQ(3u, instance_count);

  base::RunLoop loop;
  lifecycle_a.set_connection_error_handler(
      base::Bind(&DecrementCountAndQuitWhenZero, &loop, &instance_count));
  lifecycle_b.set_connection_error_handler(
      base::Bind(&DecrementCountAndQuitWhenZero, &loop, &instance_count));
  lifecycle_package.set_connection_error_handler(
      base::Bind(&DecrementCountAndQuitWhenZero, &loop, &instance_count));

  // Now quit the package. All the packaged apps should close.
  lifecycle_package->GracefulQuit();
  loop.Run();

  WaitForInstanceDestruction();
  EXPECT_FALSE(instances()->HasInstanceForName(kTestPackageName));
  EXPECT_FALSE(instances()->HasInstanceForName(kTestPackageAppNameA));
  EXPECT_FALSE(instances()->HasInstanceForName(kTestPackageAppNameB));
  EXPECT_EQ(0u, instances()->GetNewInstanceCount());
}

TEST_F(LifecycleTest, Exe_GracefulQuit) {
  base::Process process = LaunchProcess();

  test::mojom::LifecycleControlPtr lifecycle = ConnectTo(kTestExeName);

  EXPECT_TRUE(instances()->HasInstanceForName(kTestExeName));
  EXPECT_EQ(1u, instances()->GetNewInstanceCount());

  base::RunLoop loop;
  lifecycle.set_connection_error_handler(base::Bind(&QuitLoop, &loop));
  lifecycle->GracefulQuit();
  loop.Run();

  WaitForInstanceDestruction();
  EXPECT_FALSE(instances()->HasInstanceForName(kTestExeName));
  EXPECT_EQ(0u, instances()->GetNewInstanceCount());

  process.Terminate(9, true);
}

TEST_F(LifecycleTest, Exe_TerminateProcess) {
  base::Process process = LaunchProcess();

  test::mojom::LifecycleControlPtr lifecycle = ConnectTo(kTestExeName);

  EXPECT_TRUE(instances()->HasInstanceForName(kTestExeName));
  EXPECT_EQ(1u, instances()->GetNewInstanceCount());

  base::RunLoop loop;
  lifecycle.set_connection_error_handler(base::Bind(&QuitLoop, &loop));
  process.Terminate(9, true);
  loop.Run();

  WaitForInstanceDestruction();
  EXPECT_FALSE(instances()->HasInstanceForName(kTestExeName));
  EXPECT_EQ(0u, instances()->GetNewInstanceCount());
}


}  // namespace shell
}  // namespace mojo
