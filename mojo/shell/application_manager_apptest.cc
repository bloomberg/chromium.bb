// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "mojo/application/public/cpp/application_impl.h"
#include "mojo/application/public/cpp/application_test_base.h"
#include "mojo/application/public/cpp/interface_factory.h"
#include "mojo/converters/network/network_type_converters.h"
#include "mojo/shell/application_manager_apptests.mojom.h"

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
    binding_.Bind(request.Pass());
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

class ApplicationManagerAppTest : public mojo::test::ApplicationTestBase {
 public:
  ApplicationManagerAppTest() : delegate_(nullptr) {}
  ~ApplicationManagerAppTest() override {}

  void OnDriverQuit() {
    base::MessageLoop::current()->QuitNow();
  }

 protected:
  const std::string& data() const {
    DCHECK(delegate_);
    return delegate_->data();
  }

  ApplicationManagerAppTestDelegate* delegate() { return delegate_; }

 private:
  // test::ApplicationTestBase:
  ApplicationDelegate* GetApplicationDelegate() override {
    delegate_ = new ApplicationManagerAppTestDelegate;
    return delegate_;
  }

  ApplicationManagerAppTestDelegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(ApplicationManagerAppTest);
};

TEST_F(ApplicationManagerAppTest, CreateInstanceForHandle) {
  // 1. Launch a process. (Actually, have the runner launch a process that
  //    launches a process. #becauselinkerrors).
  mojo::shell::test::mojom::DriverPtr driver;
  application_impl()->ConnectToService(
      URLRequest::From(std::string("exe:application_manager_apptest_driver")),
      &driver);

  // 2. Wait for it to connect to us. (via mojo:application_manager_apptests)
  base::MessageLoop::current()->Run();

  // 3. Profit!
  EXPECT_EQ(data(), "From Target");

  driver.set_connection_error_handler(
      base::Bind(&ApplicationManagerAppTest::OnDriverQuit,
                 base::Unretained(this)));
  driver->QuitDriver();
  base::MessageLoop::current()->Run();
}

}  // namespace shell
}  // namespace mojo
