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
  ApplicationManagerAppTestDelegate() : binding_(this) {}
  ~ApplicationManagerAppTestDelegate() override {}

  const std::string& data() const { return data_; }

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
  void Ping(const String& data) override {
    data_ = data;
    base::MessageLoop::current()->QuitWhenIdle();
  }

  std::string data_;

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
    ApplicationInfo(int id, const std::string& url)
        : id(id), url(url), pid(base::kNullProcessId) {}

    int id;
    std::string url;
    base::ProcessId pid;
  };

  void AddListenerAndWaitForApplications() {
    mojom::ApplicationManagerPtr application_manager;
    application_impl()->ConnectToService("mojo:shell", &application_manager);

    mojom::ApplicationManagerListenerPtr listener;
    InterfaceRequest<mojom::ApplicationManagerListener> request =
        GetProxy(&listener);
    application_manager->AddListener(std::move(listener));
    binding_.Bind(std::move(request));
    binding_.WaitForIncomingMethodCall();
  }

  const std::string& data() const {
    DCHECK(delegate_);
    return delegate_->data();
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
    applications_.push_back(ApplicationInfo(application->id, application->url));
  }
  void ApplicationInstanceDestroyed(int id) override {
    for (auto it = applications_.begin(); it != applications_.end(); ++it) {
      auto& application = *it;
      if (application.id == id) {
        applications_.erase(it);
        break;
      }
    }
  }
  void ApplicationPIDAvailable(int id, uint32_t pid) override {
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
  application_impl()->ConnectToService("exe:application_manager_apptest_driver",
                                       &driver);

  // 2. Wait for the target to connect to us. (via
  //    mojo:application_manager_apptests)
  base::MessageLoop::current()->Run();

  // 3.1. Validate that we got the ping from the target process...
  EXPECT_EQ("From Target", data());

  // 3.2. ... and that the right applications/processes were created.
  //      Note that the target process will be created even if the tests are
  //      run with --single-process.
  EXPECT_EQ(2u, applications().size());
  {
    auto& application = applications().front();
    EXPECT_EQ("exe://application_manager_apptest_driver/", application.url);
    EXPECT_NE(base::kNullProcessId, application.pid);
  }
  {
    auto& application = applications().back();
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
