// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop/message_loop.h"
#include "mojo/public/cpp/bindings/allocation_scope.h"
#include "mojo/public/cpp/bindings/remote_ptr.h"
#include "mojo/public/cpp/environment/environment.h"
#include "mojo/public/cpp/shell/application.h"
#include "mojo/public/interfaces/shell/shell.mojom.h"
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
      base::MessageLoop::current()->Quit();
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

  virtual void OnServiceError(ServiceManager* manager,
                              const GURL& url) OVERRIDE {
    base::MessageLoop::current()->PostTask(FROM_HERE,
                                           base::MessageLoop::QuitClosure());
  }

  bool HasFactoryForTestURL() {
    ServiceManager::TestAPI manager_test_api(service_manager_.get());
    return manager_test_api.HasFactoryForURL(GURL(kTestURLString));
  }

 protected:
  mojo::Environment env_;
  base::MessageLoop loop_;
  TestContext context_;
  scoped_ptr<Application> test_app_;
  scoped_ptr<TestClientImpl> test_client_;
  scoped_ptr<ServiceManager> service_manager_;
  DISALLOW_COPY_AND_ASSIGN(ServiceManagerTest);
};

class TestServiceLoader : public ServiceLoader {
 public:
  TestServiceLoader() : num_loads_(0) {}
  int num_loads() const { return num_loads_; }

 private:
  virtual void LoadService(ServiceManager* manager,
                           const GURL& url,
                           ScopedShellHandle service_handle) OVERRIDE {
    ++num_loads_;
  }
  virtual void OnServiceError(ServiceManager* manager, const GURL& url)
      OVERRIDE {}

  int num_loads_;
  DISALLOW_COPY_AND_ASSIGN(TestServiceLoader);
};

class TestServiceInterceptor : public ServiceManager::Interceptor {
 public:
  TestServiceInterceptor() : call_count_(0) {}

  virtual ScopedMessagePipeHandle OnConnectToClient(
      const GURL& url, ScopedMessagePipeHandle handle) OVERRIDE {
    ++call_count_;
    url_ = url;
    return handle.Pass();
  }

  std::string url_spec() const {
    if (!url_.is_valid())
      return "invalid url";
    return url_.spec();
  }

  int call_count() const {
    return call_count_;
  }

 private:
  int call_count_;
  GURL url_;
  DISALLOW_COPY_AND_ASSIGN(TestServiceInterceptor);
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

// Confirm that both urls and schemes can have their loaders explicitly set.
TEST_F(ServiceManagerTest, SetLoaders) {
  ServiceManager sm;
  TestServiceLoader default_loader;
  TestServiceLoader url_loader;
  TestServiceLoader scheme_loader;
  sm.set_default_loader(&default_loader);
  sm.SetLoaderForURL(&url_loader, GURL("test:test1"));
  sm.SetLoaderForScheme(&scheme_loader, "test");

  // test::test1 should go to url_loader.
  InterfacePipe<TestService, AnyInterface> pipe1;
  sm.Connect(GURL("test:test1"), pipe1.handle_to_peer.Pass());
  EXPECT_EQ(1, url_loader.num_loads());
  EXPECT_EQ(0, scheme_loader.num_loads());
  EXPECT_EQ(0, default_loader.num_loads());

  // test::test2 should go to scheme loader.
  InterfacePipe<TestService, AnyInterface> pipe2;
  sm.Connect(GURL("test:test2"), pipe2.handle_to_peer.Pass());
  EXPECT_EQ(1, url_loader.num_loads());
  EXPECT_EQ(1, scheme_loader.num_loads());
  EXPECT_EQ(0, default_loader.num_loads());

  // http::test1 should go to default loader.
  InterfacePipe<TestService, AnyInterface> pipe3;
  sm.Connect(GURL("http:test1"), pipe3.handle_to_peer.Pass());
  EXPECT_EQ(1, url_loader.num_loads());
  EXPECT_EQ(1, scheme_loader.num_loads());
  EXPECT_EQ(1, default_loader.num_loads());
}

TEST_F(ServiceManagerTest, Interceptor) {
  ServiceManager sm;
  TestServiceLoader default_loader;
  TestServiceInterceptor interceptor;
  sm.set_default_loader(&default_loader);
  sm.SetInterceptor(&interceptor);

  std::string url("test:test3");
  InterfacePipe<TestService, AnyInterface> pipe1;
  sm.Connect(GURL(url), pipe1.handle_to_peer.Pass());
  EXPECT_EQ(1, interceptor.call_count());
  EXPECT_EQ(url, interceptor.url_spec());
  EXPECT_EQ(1, default_loader.num_loads());
}

}  // namespace mojo
