// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <utility>

#include "base/bind.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/process/process_handle.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "mojo/shell/public/cpp/interface_factory.h"
#include "mojo/shell/public/cpp/shell_client.h"
#include "mojo/shell/public/cpp/shell_test.h"
#include "mojo/shell/public/interfaces/application_manager.mojom.h"
#include "mojo/shell/tests/application_manager/application_manager_unittest.mojom.h"

namespace mojo {
namespace shell {
namespace {

class ApplicationManagerTestClient
    : public mojo::test::ShellTestClient,
      public InterfaceFactory<test::mojom::CreateInstanceForHandleTest>,
      public test::mojom::CreateInstanceForHandleTest {
 public:
  explicit ApplicationManagerTestClient(mojo::test::ShellTest* test)
      : ShellTestClient(test),
        target_id_(mojom::Connector::kInvalidApplicationID),
        binding_(this) {}
  ~ApplicationManagerTestClient() override {}

  uint32_t target_id() const { return target_id_; }

 private:
  // mojo::ShellClient:
  bool AcceptConnection(Connection* connection) override {
    connection->AddInterface<test::mojom::CreateInstanceForHandleTest>(this);
    return true;
  }

  // InterfaceFactory<test::mojom::CreateInstanceForHandleTest>:
  void Create(
      Connection* connection,
      test::mojom::CreateInstanceForHandleTestRequest request) override {
    binding_.Bind(std::move(request));
  }

  // test::mojom::CreateInstanceForHandleTest:
  void SetTargetID(uint32_t target_id) override {
    target_id_ = target_id;
    base::MessageLoop::current()->QuitWhenIdle();
  }

  uint32_t target_id_;

  Binding<test::mojom::CreateInstanceForHandleTest> binding_;

  DISALLOW_COPY_AND_ASSIGN(ApplicationManagerTestClient);
};

}  // namespace

class ApplicationManagerTest : public mojo::test::ShellTest,
                               public mojom::InstanceListener {
 public:
  ApplicationManagerTest()
      : ShellTest("mojo:application_manager_unittest"),
        shell_client_(nullptr),
        binding_(this) {}
  ~ApplicationManagerTest() override {}

  void OnDriverQuit() {
    base::MessageLoop::current()->QuitNow();
  }

 protected:
  struct InstanceInfo {
    InstanceInfo(uint32_t id, const std::string& name)
        : id(id), name(name), pid(base::kNullProcessId) {}

    uint32_t id;
    std::string name;
    base::ProcessId pid;
  };

  void AddListenerAndWaitForApplications() {
    mojom::ApplicationManagerPtr application_manager;
    connector()->ConnectToInterface("mojo:shell", &application_manager);

    application_manager->AddInstanceListener(
        binding_.CreateInterfacePtrAndBind());
    binding_.WaitForIncomingMethodCall();
  }

  bool ContainsInstanceWithName(const std::string& name) const {
    for (const auto& instance : initial_instances_) {
      if (instance.name == name)
        return true;
    }
    for (const auto& instance : instances_) {
      if (instance.name == name)
        return true;
    }
    return false;
  }

  uint32_t target_id() const {
    DCHECK(shell_client_);
    return shell_client_->target_id();
  }

  const std::vector<InstanceInfo>& instances() const {
    return instances_;
  }

 private:
  // test::ShellTest:
  scoped_ptr<ShellClient> CreateShellClient() override {
    shell_client_ = new ApplicationManagerTestClient(this);
    return make_scoped_ptr(shell_client_);
  }

  // mojom::InstanceListener:
  void SetExistingInstances(Array<mojom::InstanceInfoPtr> instances) override {
    for (size_t i = 0; i < instances.size(); ++i) {
      initial_instances_.push_back(InstanceInfo(instances[i]->id,
                                                instances[i]->name));
    }
  }
  void InstanceCreated(mojom::InstanceInfoPtr instance) override {
    instances_.push_back(InstanceInfo(instance->id, instance->name));
  }
  void InstanceDestroyed(uint32_t id) override {
    for (auto it = instances_.begin(); it != instances_.end(); ++it) {
      auto& instance = *it;
      if (instance.id == id) {
        instances_.erase(it);
        break;
      }
    }
  }
  void InstancePIDAvailable(uint32_t id, uint32_t pid) override {
    for (auto& instance : instances_) {
      if (instance.id == id) {
        instance.pid = pid;
        break;
      }
    }
  }

  ApplicationManagerTestClient* shell_client_;
  Binding<mojom::InstanceListener> binding_;
  std::vector<InstanceInfo> instances_;
  std::vector<InstanceInfo> initial_instances_;

  DISALLOW_COPY_AND_ASSIGN(ApplicationManagerTest);
};

TEST_F(ApplicationManagerTest, CreateInstanceForHandle) {
  AddListenerAndWaitForApplications();

  // 1. Launch a process. (Actually, have the runner launch a process that
  //    launches a process.)
  mojo::shell::test::mojom::DriverPtr driver;
  scoped_ptr<Connection> connection =
      connector()->Connect("exe:application_manager_unittest_driver");
  connection->GetInterface(&driver);

  // 2. Wait for the target to connect to us. (via
  //    mojo:application_manager_unittest)
  base::MessageLoop::current()->Run();

  uint32_t remote_id = mojom::Connector::kInvalidApplicationID;
  EXPECT_TRUE(connection->GetRemoteApplicationID(&remote_id));
  EXPECT_NE(mojom::Connector::kInvalidApplicationID, remote_id);

  // 3. Validate that this test suite's name was received from the application
  //    manager.
  EXPECT_TRUE(ContainsInstanceWithName("mojo:application_manager_unittest"));

  // 4. Validate that the right applications/processes were created.
  //    Note that the target process will be created even if the tests are
  //    run with --single-process.
  EXPECT_EQ(2u, instances().size());
  {
    auto& instance = instances().front();
    EXPECT_EQ(remote_id, instance.id);
    EXPECT_EQ("exe:application_manager_unittest_driver", instance.name);
    EXPECT_NE(base::kNullProcessId, instance.pid);
  }
  {
    auto& instance = instances().back();
    // We learn about the target process id via a ping from it.
    EXPECT_EQ(target_id(), instance.id);
    EXPECT_EQ("exe:application_manager_unittest_target", instance.name);
    EXPECT_NE(base::kNullProcessId, instance.pid);
  }

  driver.set_connection_error_handler(
      base::Bind(&ApplicationManagerTest::OnDriverQuit,
                 base::Unretained(this)));
  driver->QuitDriver();
  base::MessageLoop::current()->Run();
}

}  // namespace shell
}  // namespace mojo
