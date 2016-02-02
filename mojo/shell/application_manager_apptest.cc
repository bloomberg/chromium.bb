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
#include "mojo/common/weak_binding_set.h"
#include "mojo/converters/network/network_type_converters.h"
#include "mojo/shell/application_manager_apptests.mojom.h"
#include "mojo/shell/public/cpp/application_impl.h"
#include "mojo/shell/public/cpp/application_test_base.h"
#include "mojo/shell/public/cpp/interface_factory.h"
#include "mojo/shell/public/interfaces/application_manager.mojom.h"

using mojo::shell::test::mojom::CreateInstanceForHandleTest;

namespace mojo {
namespace shell {
namespace {

class ApplicationManagerAppTestDelegate
    : public ApplicationDelegate,
      public InterfaceFactory<CreateInstanceForHandleTest>,
      public CreateInstanceForHandleTest {
 public:
  ApplicationManagerAppTestDelegate()
      : target_id_(Shell::kInvalidApplicationID),
        binding_(this) {}
  ~ApplicationManagerAppTestDelegate() override {}

  uint32_t target_id() const { return target_id_; }

 private:
  // ApplicationDelegate:
  void Initialize(ApplicationImpl* app) override {}
  bool ConfigureIncomingConnection(ApplicationConnection* connection) override {
    connection->AddService<CreateInstanceForHandleTest>(this);
    return true;
  }

  // InterfaceFactory<CreateInstanceForHandleTest>:
  void Create(
      ApplicationConnection* connection,
      InterfaceRequest<CreateInstanceForHandleTest> request) override {
    binding_.Bind(std::move(request));
  }

  // CreateInstanceForHandleTest:
  void SetTargetID(uint32_t target_id) override {
    target_id_ = target_id;
    base::MessageLoop::current()->QuitWhenIdle();
  }

  uint32_t target_id_;

  Binding<CreateInstanceForHandleTest> binding_;

  DISALLOW_COPY_AND_ASSIGN(ApplicationManagerAppTestDelegate);
};

}  // namespace

class ApplicationManagerAppTest : public mojo::test::ApplicationTestBase,
                                  public mojom::ApplicationManagerListener {
 public:
  ApplicationManagerAppTest() : delegate_(nullptr), binding_(this) {}
  ~ApplicationManagerAppTest() override {}

  void OnDriverQuit() {
    base::MessageLoop::current()->QuitNow();
  }

 protected:
  struct ApplicationInfo {
    ApplicationInfo(uint32_t id,
                    const std::string& url,
                    const std::string& name)
        : id(id), url(url), pid(base::kNullProcessId), name(name) {}

    uint32_t id;
    std::string url;
    base::ProcessId pid;
    std::string name;
  };

  void AddListenerAndWaitForApplications() {
    mojom::ApplicationManagerPtr application_manager;
    application_impl()->ConnectToService("mojo:shell", &application_manager);

    application_manager->AddListener(binding_.CreateInterfacePtrAndBind());
    binding_.WaitForIncomingMethodCall();
  }

  uint32_t target_id() const {
    DCHECK(delegate_);
    return delegate_->target_id();
  }

  const std::vector<ApplicationInfo>& applications() const {
    return applications_;
  }

  ApplicationManagerAppTestDelegate* delegate() { return delegate_; }

 private:
  // test::ApplicationTestBase:
  ApplicationDelegate* GetApplicationDelegate() override {
    delegate_ = new ApplicationManagerAppTestDelegate;
    return delegate_;
  }

  // mojom::ApplicationManagerListener:
  void SetRunningApplications(
      Array<mojom::ApplicationInfoPtr> applications) override {}
  void ApplicationInstanceCreated(
      mojom::ApplicationInfoPtr application) override {
    applications_.push_back(ApplicationInfo(application->id, application->url,
                                            application->name));
  }
  void ApplicationInstanceDestroyed(uint32_t id) override {
    for (auto it = applications_.begin(); it != applications_.end(); ++it) {
      auto& application = *it;
      if (application.id == id) {
        applications_.erase(it);
        break;
      }
    }
  }
  void ApplicationPIDAvailable(uint32_t id, uint32_t pid) override {
    for (auto& application : applications_) {
      if (application.id == id) {
        application.pid = pid;
        break;
      }
    }
  }

  ApplicationManagerAppTestDelegate* delegate_;
  Binding<mojom::ApplicationManagerListener> binding_;
  std::vector<ApplicationInfo> applications_;

  DISALLOW_COPY_AND_ASSIGN(ApplicationManagerAppTest);
};

TEST_F(ApplicationManagerAppTest, CreateInstanceForHandle) {
  AddListenerAndWaitForApplications();

  // 1. Launch a process. (Actually, have the runner launch a process that
  //    launches a process. #becauselinkerrors).
  mojo::shell::test::mojom::DriverPtr driver;
  scoped_ptr<ApplicationConnection> connection =
      application_impl()->ConnectToApplication(
          "exe:application_manager_apptest_driver");
  connection->ConnectToService(&driver);

  // 2. Wait for the target to connect to us. (via
  //    mojo:application_manager_apptests)
  base::MessageLoop::current()->Run();

  uint32_t remote_id = Shell::kInvalidApplicationID;
  EXPECT_TRUE(connection->GetRemoteApplicationID(&remote_id));
  EXPECT_NE(Shell::kInvalidApplicationID, remote_id);

  // 2. Validate that the right applications/processes were created.
  //    Note that the target process will be created even if the tests are
  //    run with --single-process.
  EXPECT_EQ(2u, applications().size());
  {
    auto& application = applications().front();
    EXPECT_EQ(remote_id, application.id);
    EXPECT_EQ("exe://application_manager_apptest_driver/", application.url);
    EXPECT_NE(base::kNullProcessId, application.pid);
  }
  {
    auto& application = applications().back();
    // We learn about the target process id via a ping from it.
    EXPECT_EQ(target_id(), application.id);
    EXPECT_EQ("exe://application_manager_apptest_target/", application.url);
    EXPECT_NE(base::kNullProcessId, application.pid);
  }

  driver.set_connection_error_handler(
      base::Bind(&ApplicationManagerAppTest::OnDriverQuit,
                 base::Unretained(this)));
  driver->QuitDriver();
  base::MessageLoop::current()->Run();
}

}  // namespace shell
}  // namespace mojo
