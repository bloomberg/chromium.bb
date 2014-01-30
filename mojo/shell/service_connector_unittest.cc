// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop/message_loop.h"
#include "mojo/public/bindings/allocation_scope.h"
#include "mojo/public/bindings/remote_ptr.h"
#include "mojo/shell/service_connector.h"
#include "mojom/shell.h"
#include "mojom/test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace shell {
namespace {

class TestApp : public ShellClient {
 public:
  TestApp(ScopedMessagePipeHandle shell_handle)
      : shell_(shell_handle.Pass(), this) {
  }
  virtual ~TestApp() {
  }
  virtual void AcceptConnection(ScopedMessagePipeHandle client_handle)
      MOJO_OVERRIDE {
    service_.reset(new TestServiceImpl(this, client_handle.Pass()));
  }
  std::string GetLastTestString() {
    return service_->last_test_string_;
  }

 private:
  class TestServiceImpl : public TestService {
   public:
    TestServiceImpl(TestApp* service, ScopedMessagePipeHandle client_handle)
        : service_(service),
          client_(client_handle.Pass(), this) {
    }
    virtual ~TestServiceImpl() {
    }
    virtual void Test(const mojo::String& test_string) OVERRIDE {
      last_test_string_ = test_string.To<std::string>();
      client_->AckTest();
    }
    TestApp* service_;
    RemotePtr<TestClient> client_;
    std::string last_test_string_;
  };
  RemotePtr<Shell> shell_;
  scoped_ptr<TestServiceImpl> service_;
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
    test_app_.reset(new TestApp(shell_handle.Pass()));
  }

 protected:
  base::MessageLoop loop_;
  scoped_ptr<TestApp> test_app_;
  scoped_ptr<TestClientImpl> test_client_;
  scoped_ptr<ServiceConnector> service_connector_;
  DISALLOW_COPY_AND_ASSIGN(ServiceConnectorTest);
};

TEST_F(ServiceConnectorTest, Basic) {
  test_client_->Test("test");
  loop_.Run();
  EXPECT_EQ(std::string("test"), test_app_->GetLastTestString());
}

}  // namespace
}  // namespace shell
}  // namespace mojo
