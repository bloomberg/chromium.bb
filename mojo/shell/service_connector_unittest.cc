// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop/message_loop.h"
#include "mojo/public/bindings/allocation_scope.h"
#include "mojo/public/bindings/remote_ptr.h"
#include "mojo/public/shell/service.h"
#include "mojo/shell/service_connector.h"
#include "mojom/shell.h"
#include "mojom/test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace shell {
namespace {

struct Context {
  std::string last_test_string;
};

class TestServiceImpl : public Service<TestService, TestServiceImpl, Context> {
 public:
  virtual void Test(const mojo::String& test_string) OVERRIDE {
    context()->last_test_string = test_string.To<std::string>();
    client()->AckTest();
  }
};

class TestClientImpl : public TestClient {
 public:
  explicit TestClientImpl(ScopedMessagePipeHandle service_handle)
      : service_(service_handle.Pass(), this),
        quit_after_ack_(false) {
  }
  virtual ~TestClientImpl() {
  }
  virtual void AckTest() OVERRIDE {
    if (quit_after_ack_)
      base::MessageLoop::current()->QuitNow();
  }
  void Test(std::string test_string) {
    AllocationScope scope;
    quit_after_ack_ = true;
    service_->Test(mojo::String(test_string));
  }
  RemotePtr<TestService> service_;
  bool quit_after_ack_;
};

class ServiceConnectorTest : public testing::Test,
                             public ServiceConnector::Loader {
 public:
  ServiceConnectorTest() {
  }

  virtual ~ServiceConnectorTest() {
  }

  virtual void SetUp() OVERRIDE {
    GURL test_url("test:testService");
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
    test_app_.reset(new ServiceFactory<TestServiceImpl, Context>(
        shell_handle.Pass(), &context_));
  }

 protected:
  base::MessageLoop loop_;
  Context context_;
  scoped_ptr<ServiceFactory<TestServiceImpl, Context> > test_app_;
  scoped_ptr<TestClientImpl> test_client_;
  scoped_ptr<ServiceConnector> service_connector_;
  DISALLOW_COPY_AND_ASSIGN(ServiceConnectorTest);
};

TEST_F(ServiceConnectorTest, Basic) {
  test_client_->Test("test");
  loop_.Run();
  EXPECT_EQ(std::string("test"), context_.last_test_string);
}

}  // namespace
}  // namespace shell
}  // namespace mojo
