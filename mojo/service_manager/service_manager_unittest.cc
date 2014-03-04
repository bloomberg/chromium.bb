// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/bindings/allocation_scope.h"
#include "mojo/public/bindings/remote_ptr.h"
#include "mojo/public/environment/environment.h"
#include "mojo/public/shell/application.h"
#include "mojo/public/shell/shell.mojom.h"
#include "mojo/public/utility/run_loop.h"
#include "mojo/service_manager/service_loader.h"
#include "mojo/service_manager/service_manager.h"
#include "mojo/service_manager/test.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace {

const char kTestURLString[] = "test:testService";

struct TestContext {
  TestContext() : num_impls(0) {}
  std::string last_test_string;
  int num_impls;
};

class TestServiceImpl :
    public Service<TestService, TestServiceImpl, TestContext> {
 public:
  TestServiceImpl() {}

  virtual ~TestServiceImpl() {
    --context()->num_impls;
  }

  virtual void Test(const mojo::String& test_string) OVERRIDE {
    context()->last_test_string = test_string.To<std::string>();
    client()->AckTest();
  }

  void Initialize(ServiceFactory<TestServiceImpl, TestContext>* service_factory,
                  ScopedMessagePipeHandle client_handle) {
    Service<TestService, TestServiceImpl, TestContext>::Initialize(
        service_factory, client_handle.Pass());
    ++context()->num_impls;
  }
};

class TestClientImpl : public TestClient {
 public:
  explicit TestClientImpl(ScopedTestServiceHandle service_handle)
      : service_(service_handle.Pass(), this),
        quit_after_ack_(false) {
  }

  virtual ~TestClientImpl() {}

  virtual void AckTest() OVERRIDE {
    if (quit_after_ack_)
      mojo::RunLoop::current()->Quit();
  }

  void Test(std::string test_string) {
    AllocationScope scope;
    quit_after_ack_ = true;
    service_->Test(mojo::String(test_string));
  }

 private:
  RemotePtr<TestService> service_;
  bool quit_after_ack_;
  DISALLOW_COPY_AND_ASSIGN(TestClientImpl);
};
}  // namespace

class ServiceManagerTest : public testing::Test, public ServiceLoader {
 public:
  ServiceManagerTest() {}

  virtual ~ServiceManagerTest() {}

  virtual void SetUp() OVERRIDE {
    GURL test_url(kTestURLString);
    service_manager_.reset(new ServiceManager);
    service_manager_->SetLoaderForURL(this, test_url);

    InterfacePipe<TestService, AnyInterface> pipe;
    test_client_.reset(new TestClientImpl(pipe.handle_to_self.Pass()));
    service_manager_->Connect(test_url, pipe.handle_to_peer.Pass());
  }

  virtual void TearDown() OVERRIDE {
    test_client_.reset(NULL);
    test_app_.reset(NULL);
    service_manager_.reset(NULL);
  }

  virtual void LoadService(ServiceManager* manager,
                           const GURL& url,
                           ScopedShellHandle shell_handle) OVERRIDE {
    test_app_.reset(new Application(shell_handle.Pass()));
    test_app_->AddServiceFactory(
        new ServiceFactory<TestServiceImpl, TestContext>(&context_));
  }

  bool HasFactoryForTestURL() {
    ServiceManager::TestAPI manager_test_api(service_manager_.get());
    return manager_test_api.HasFactoryForURL(GURL(kTestURLString));
  }

 protected:
  mojo::Environment env_;
  mojo::RunLoop loop_;
  TestContext context_;
  scoped_ptr<Application> test_app_;
  scoped_ptr<TestClientImpl> test_client_;
  scoped_ptr<ServiceManager> service_manager_;
  DISALLOW_COPY_AND_ASSIGN(ServiceManagerTest);
};

TEST_F(ServiceManagerTest, Basic) {
  test_client_->Test("test");
  loop_.Run();
  EXPECT_EQ(std::string("test"), context_.last_test_string);
}

TEST_F(ServiceManagerTest, ClientError) {
  test_client_->Test("test");
  EXPECT_TRUE(HasFactoryForTestURL());
  loop_.Run();
  EXPECT_EQ(1, context_.num_impls);
  test_client_.reset(NULL);
  loop_.Run();
  EXPECT_EQ(0, context_.num_impls);
  EXPECT_FALSE(HasFactoryForTestURL());
}
}  // namespace mojo
