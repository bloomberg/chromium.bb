// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/bindings/allocation_scope.h"
#include "mojo/public/bindings/remote_ptr.h"
#include "mojo/public/environment/environment.h"
#include "mojo/public/shell/service.h"
#include "mojo/public/utility/run_loop.h"
#include "mojo/shell/service_connector.h"
#include "mojom/shell.h"
#include "mojom/test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace shell {
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
  explicit TestClientImpl(ScopedMessagePipeHandle service_handle)
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

class ServiceConnectorTest : public testing::Test,
                             public ServiceConnector::Loader {
 public:
  ServiceConnectorTest() {}

  virtual ~ServiceConnectorTest() {}

  virtual void SetUp() OVERRIDE {
    GURL test_url(kTestURLString);
    service_connector_.reset(new ServiceConnector);
    service_connector_->SetLoaderForURL(this, test_url);
    MessagePipe pipe;
    test_client_.reset(new TestClientImpl(pipe.handle0.Pass()));
    service_connector_->Connect(test_url, pipe.handle1.Pass());
  }

  virtual void TearDown() OVERRIDE {
    test_client_.reset(NULL);
    test_app_.reset(NULL);
    service_connector_.reset(NULL);
  }

  virtual void Load(const GURL& url,
                    ScopedMessagePipeHandle shell_handle) OVERRIDE {
    test_app_.reset(new ServiceFactory<TestServiceImpl, TestContext>(
        shell_handle.Pass(), &context_));
  }

  bool HasFactoryForTestURL() {
    ServiceConnector::TestAPI connector_test_api(service_connector_.get());
    return connector_test_api.HasFactoryForURL(GURL(kTestURLString));
  }

 protected:
  mojo::Environment env_;
  mojo::RunLoop loop_;
  TestContext context_;
  scoped_ptr<ServiceFactory<TestServiceImpl, TestContext> > test_app_;
  scoped_ptr<TestClientImpl> test_client_;
  scoped_ptr<ServiceConnector> service_connector_;
  DISALLOW_COPY_AND_ASSIGN(ServiceConnectorTest);
};

TEST_F(ServiceConnectorTest, Basic) {
  test_client_->Test("test");
  loop_.Run();
  EXPECT_EQ(std::string("test"), context_.last_test_string);
}

TEST_F(ServiceConnectorTest, ClientError) {
  test_client_->Test("test");
  EXPECT_TRUE(HasFactoryForTestURL());
  loop_.Run();
  EXPECT_EQ(1, context_.num_impls);
  test_client_.reset(NULL);
  loop_.Run();
  EXPECT_EQ(0, context_.num_impls);
  EXPECT_FALSE(HasFactoryForTestURL());
}
}  // namespace shell
}  // namespace mojo
