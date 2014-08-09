// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/at_exit.h"
#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "mojo/application_manager/application_loader.h"
#include "mojo/application_manager/application_manager.h"
#include "mojo/application_manager/background_shell_application_loader.h"
#include "mojo/application_manager/test.mojom.h"
#include "mojo/public/cpp/application/application_connection.h"
#include "mojo/public/cpp/application/application_delegate.h"
#include "mojo/public/cpp/application/application_impl.h"
#include "mojo/public/cpp/application/interface_factory.h"
#include "mojo/public/interfaces/application/service_provider.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace {

const char kTestURLString[] = "test:testService";
const char kTestAURLString[] = "test:TestA";
const char kTestBURLString[] = "test:TestB";

struct TestContext {
  TestContext() : num_impls(0), num_loader_deletes(0) {}
  std::string last_test_string;
  int num_impls;
  int num_loader_deletes;
};

class QuitMessageLoopErrorHandler : public ErrorHandler {
 public:
  QuitMessageLoopErrorHandler() {}
  virtual ~QuitMessageLoopErrorHandler() {}

  // |ErrorHandler| implementation:
  virtual void OnConnectionError() OVERRIDE {
    base::MessageLoop::current()->QuitWhenIdle();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(QuitMessageLoopErrorHandler);
};

class TestServiceImpl : public InterfaceImpl<TestService> {
 public:
  explicit TestServiceImpl(TestContext* context) : context_(context) {
    ++context_->num_impls;
  }

  virtual ~TestServiceImpl() { --context_->num_impls; }

  virtual void OnConnectionError() OVERRIDE {
    if (!base::MessageLoop::current()->is_running())
      return;
    base::MessageLoop::current()->Quit();
  }

  // TestService implementation:
  virtual void Test(const String& test_string) OVERRIDE {
    context_->last_test_string = test_string;
    client()->AckTest();
  }

 private:
  TestContext* context_;
};

class TestClientImpl : public TestClient {
 public:
  explicit TestClientImpl(TestServicePtr service)
      : service_(service.Pass()), quit_after_ack_(false) {
    service_.set_client(this);
  }

  virtual ~TestClientImpl() { service_.reset(); }

  virtual void AckTest() OVERRIDE {
    if (quit_after_ack_)
      base::MessageLoop::current()->Quit();
  }

  void Test(std::string test_string) {
    quit_after_ack_ = true;
    service_->Test(test_string);
  }

 private:
  TestServicePtr service_;
  bool quit_after_ack_;
  DISALLOW_COPY_AND_ASSIGN(TestClientImpl);
};

class TestApplicationLoader : public ApplicationLoader,
                              public ApplicationDelegate,
                              public InterfaceFactory<TestService> {
 public:
  TestApplicationLoader() : context_(NULL), num_loads_(0) {}

  virtual ~TestApplicationLoader() {
    if (context_)
      ++context_->num_loader_deletes;
    test_app_.reset(NULL);
  }

  void set_context(TestContext* context) { context_ = context; }
  int num_loads() const { return num_loads_; }

 private:
  // ApplicationLoader implementation.
  virtual void Load(ApplicationManager* manager,
                    const GURL& url,
                    scoped_refptr<LoadCallbacks> callbacks) OVERRIDE {
    ++num_loads_;
    test_app_.reset(
        new ApplicationImpl(this, callbacks->RegisterApplication().Pass()));
  }

  virtual void OnServiceError(ApplicationManager* manager,
                              const GURL& url) OVERRIDE {}

  // ApplicationDelegate implementation.
  virtual bool ConfigureIncomingConnection(
      ApplicationConnection* connection) OVERRIDE {
    connection->AddService(this);
    return true;
  }

  // InterfaceFactory implementation.
  virtual void Create(ApplicationConnection* connection,
                      InterfaceRequest<TestService> request) OVERRIDE {
    BindToRequest(new TestServiceImpl(context_), &request);
  }

  scoped_ptr<ApplicationImpl> test_app_;
  TestContext* context_;
  int num_loads_;
  DISALLOW_COPY_AND_ASSIGN(TestApplicationLoader);
};

class TesterContext {
 public:
  explicit TesterContext(base::MessageLoop* loop)
      : num_b_calls_(0),
        num_c_calls_(0),
        num_a_deletes_(0),
        num_b_deletes_(0),
        num_c_deletes_(0),
        tester_called_quit_(false),
        a_called_quit_(false),
        loop_(loop) {}

  void IncrementNumBCalls() {
    base::AutoLock lock(lock_);
    num_b_calls_++;
  }

  void IncrementNumCCalls() {
    base::AutoLock lock(lock_);
    num_c_calls_++;
  }

  void IncrementNumADeletes() {
    base::AutoLock lock(lock_);
    num_a_deletes_++;
  }

  void IncrementNumBDeletes() {
    base::AutoLock lock(lock_);
    num_b_deletes_++;
  }

  void IncrementNumCDeletes() {
    base::AutoLock lock(lock_);
    num_c_deletes_++;
  }

  void set_tester_called_quit() {
    base::AutoLock lock(lock_);
    tester_called_quit_ = true;
  }

  void set_a_called_quit() {
    base::AutoLock lock(lock_);
    a_called_quit_ = true;
  }

  int num_b_calls() {
    base::AutoLock lock(lock_);
    return num_b_calls_;
  }
  int num_c_calls() {
    base::AutoLock lock(lock_);
    return num_c_calls_;
  }
  int num_a_deletes() {
    base::AutoLock lock(lock_);
    return num_a_deletes_;
  }
  int num_b_deletes() {
    base::AutoLock lock(lock_);
    return num_b_deletes_;
  }
  int num_c_deletes() {
    base::AutoLock lock(lock_);
    return num_c_deletes_;
  }
  bool tester_called_quit() {
    base::AutoLock lock(lock_);
    return tester_called_quit_;
  }
  bool a_called_quit() {
    base::AutoLock lock(lock_);
    return a_called_quit_;
  }

  void QuitSoon() {
    loop_->PostTask(FROM_HERE, base::MessageLoop::QuitWhenIdleClosure());
  }

 private:
  // lock_ protects all members except for loop_ which must be unchanged for the
  // lifetime of this class.
  base::Lock lock_;
  int num_b_calls_;
  int num_c_calls_;
  int num_a_deletes_;
  int num_b_deletes_;
  int num_c_deletes_;
  bool tester_called_quit_;
  bool a_called_quit_;

  base::MessageLoop* loop_;
};

// Used to test that the requestor url will be correctly passed.
class TestAImpl : public InterfaceImpl<TestA> {
 public:
  TestAImpl(ApplicationConnection* connection, TesterContext* test_context)
      : test_context_(test_context) {
    connection->ConnectToApplication(kTestBURLString)->ConnectToService(&b_);
  }
  virtual ~TestAImpl() {
    test_context_->IncrementNumADeletes();
    if (base::MessageLoop::current()->is_running())
      Quit();
  }

 private:
  virtual void CallB() OVERRIDE {
    b_->B(base::Bind(&TestAImpl::Quit, base::Unretained(this)));
  }

  virtual void CallCFromB() OVERRIDE {
    b_->CallC(base::Bind(&TestAImpl::Quit, base::Unretained(this)));
  }

  void Quit() {
    base::MessageLoop::current()->Quit();
    test_context_->set_a_called_quit();
    test_context_->QuitSoon();
  }

  TesterContext* test_context_;
  TestBPtr b_;
};

class TestBImpl : public InterfaceImpl<TestB> {
 public:
  TestBImpl(ApplicationConnection* connection, TesterContext* test_context)
      : test_context_(test_context) {
    connection->ConnectToService(&c_);
  }

  virtual ~TestBImpl() {
    test_context_->IncrementNumBDeletes();
    if (base::MessageLoop::current()->is_running())
      base::MessageLoop::current()->Quit();
    test_context_->QuitSoon();
  }

 private:
  virtual void B(const mojo::Callback<void()>& callback) OVERRIDE {
    test_context_->IncrementNumBCalls();
    callback.Run();
  }

  virtual void CallC(const mojo::Callback<void()>& callback) OVERRIDE {
    test_context_->IncrementNumBCalls();
    c_->C(callback);
  }

  TesterContext* test_context_;
  TestCPtr c_;
};

class TestCImpl : public InterfaceImpl<TestC> {
 public:
  TestCImpl(ApplicationConnection* connection, TesterContext* test_context)
      : test_context_(test_context) {}

  virtual ~TestCImpl() { test_context_->IncrementNumCDeletes(); }

 private:
  virtual void C(const mojo::Callback<void()>& callback) OVERRIDE {
    test_context_->IncrementNumCCalls();
    callback.Run();
  }
  TesterContext* test_context_;
};

class Tester : public ApplicationDelegate,
               public ApplicationLoader,
               public InterfaceFactory<TestA>,
               public InterfaceFactory<TestB>,
               public InterfaceFactory<TestC> {
 public:
  Tester(TesterContext* context, const std::string& requestor_url)
      : context_(context), requestor_url_(requestor_url) {}
  virtual ~Tester() {}

 private:
  virtual void Load(ApplicationManager* manager,
                    const GURL& url,
                    scoped_refptr<LoadCallbacks> callbacks) OVERRIDE {
    app_.reset(
        new ApplicationImpl(this, callbacks->RegisterApplication().Pass()));
  }

  virtual void OnServiceError(ApplicationManager* manager,
                              const GURL& url) OVERRIDE {}

  virtual bool ConfigureIncomingConnection(
      ApplicationConnection* connection) OVERRIDE {
    if (!requestor_url_.empty() &&
        requestor_url_ != connection->GetRemoteApplicationURL()) {
      context_->set_tester_called_quit();
      context_->QuitSoon();
      base::MessageLoop::current()->Quit();
      return false;
    }
    // If we're coming from A, then add B, otherwise A.
    if (connection->GetRemoteApplicationURL() == kTestAURLString)
      connection->AddService<TestB>(this);
    else
      connection->AddService<TestA>(this);
    return true;
  }

  virtual bool ConfigureOutgoingConnection(
      ApplicationConnection* connection) OVERRIDE {
    // If we're connecting to B, then add C.
    if (connection->GetRemoteApplicationURL() == kTestBURLString)
      connection->AddService<TestC>(this);
    return true;
  }

  virtual void Create(ApplicationConnection* connection,
                      InterfaceRequest<TestA> request) OVERRIDE {
    BindToRequest(new TestAImpl(connection, context_), &request);
  }

  virtual void Create(ApplicationConnection* connection,
                      InterfaceRequest<TestB> request) OVERRIDE {
    BindToRequest(new TestBImpl(connection, context_), &request);
  }

  virtual void Create(ApplicationConnection* connection,
                      InterfaceRequest<TestC> request) OVERRIDE {
    BindToRequest(new TestCImpl(connection, context_), &request);
  }

  TesterContext* context_;
  scoped_ptr<ApplicationImpl> app_;
  std::string requestor_url_;
};

class TestServiceInterceptor : public ApplicationManager::Interceptor {
 public:
  TestServiceInterceptor() : call_count_(0) {}

  virtual ServiceProviderPtr OnConnectToClient(
      const GURL& url,
      ServiceProviderPtr service_provider) OVERRIDE {
    ++call_count_;
    url_ = url;
    return service_provider.Pass();
  }

  std::string url_spec() const {
    if (!url_.is_valid())
      return "invalid url";
    return url_.spec();
  }

  int call_count() const { return call_count_; }

 private:
  int call_count_;
  GURL url_;
  DISALLOW_COPY_AND_ASSIGN(TestServiceInterceptor);
};

}  // namespace

class ApplicationManagerTest : public testing::Test {
 public:
  ApplicationManagerTest() : tester_context_(&loop_) {}

  virtual ~ApplicationManagerTest() {}

  virtual void SetUp() OVERRIDE {
    application_manager_.reset(new ApplicationManager);
    TestApplicationLoader* default_loader = new TestApplicationLoader;
    default_loader->set_context(&context_);
    application_manager_->set_default_loader(
        scoped_ptr<ApplicationLoader>(default_loader));

    TestServicePtr service_proxy;
    application_manager_->ConnectToService(GURL(kTestURLString),
                                           &service_proxy);
    test_client_.reset(new TestClientImpl(service_proxy.Pass()));
  }

  virtual void TearDown() OVERRIDE {
    test_client_.reset(NULL);
    application_manager_.reset(NULL);
  }

  scoped_ptr<BackgroundShellApplicationLoader> MakeLoader(
      const std::string& requestor_url) {
    scoped_ptr<ApplicationLoader> real_loader(
        new Tester(&tester_context_, requestor_url));
    scoped_ptr<BackgroundShellApplicationLoader> loader(
        new BackgroundShellApplicationLoader(real_loader.Pass(),
                                             std::string(),
                                             base::MessageLoop::TYPE_DEFAULT));
    return loader.Pass();
  }

  void AddLoaderForURL(const GURL& url, const std::string& requestor_url) {
    application_manager_->SetLoaderForURL(
        MakeLoader(requestor_url).PassAs<ApplicationLoader>(), url);
  }

  bool HasFactoryForTestURL() {
    ApplicationManager::TestAPI manager_test_api(application_manager_.get());
    return manager_test_api.HasFactoryForURL(GURL(kTestURLString));
  }

 protected:
  base::ShadowingAtExitManager at_exit_;
  TesterContext tester_context_;
  TestContext context_;
  base::MessageLoop loop_;
  scoped_ptr<TestClientImpl> test_client_;
  scoped_ptr<ApplicationManager> application_manager_;
  DISALLOW_COPY_AND_ASSIGN(ApplicationManagerTest);
};

TEST_F(ApplicationManagerTest, Basic) {
  test_client_->Test("test");
  loop_.Run();
  EXPECT_EQ(std::string("test"), context_.last_test_string);
}

TEST_F(ApplicationManagerTest, ClientError) {
  test_client_->Test("test");
  EXPECT_TRUE(HasFactoryForTestURL());
  loop_.Run();
  EXPECT_EQ(1, context_.num_impls);
  test_client_.reset(NULL);
  loop_.Run();
  EXPECT_EQ(0, context_.num_impls);
  EXPECT_TRUE(HasFactoryForTestURL());
}

TEST_F(ApplicationManagerTest, Deletes) {
  {
    ApplicationManager sm;
    TestApplicationLoader* default_loader = new TestApplicationLoader;
    default_loader->set_context(&context_);
    TestApplicationLoader* url_loader1 = new TestApplicationLoader;
    TestApplicationLoader* url_loader2 = new TestApplicationLoader;
    url_loader1->set_context(&context_);
    url_loader2->set_context(&context_);
    TestApplicationLoader* scheme_loader1 = new TestApplicationLoader;
    TestApplicationLoader* scheme_loader2 = new TestApplicationLoader;
    scheme_loader1->set_context(&context_);
    scheme_loader2->set_context(&context_);
    sm.set_default_loader(scoped_ptr<ApplicationLoader>(default_loader));
    sm.SetLoaderForURL(scoped_ptr<ApplicationLoader>(url_loader1),
                       GURL("test:test1"));
    sm.SetLoaderForURL(scoped_ptr<ApplicationLoader>(url_loader2),
                       GURL("test:test1"));
    sm.SetLoaderForScheme(scoped_ptr<ApplicationLoader>(scheme_loader1),
                          "test");
    sm.SetLoaderForScheme(scoped_ptr<ApplicationLoader>(scheme_loader2),
                          "test");
  }
  EXPECT_EQ(5, context_.num_loader_deletes);
}

// Confirm that both urls and schemes can have their loaders explicitly set.
TEST_F(ApplicationManagerTest, SetLoaders) {
  ApplicationManager sm;
  TestApplicationLoader* default_loader = new TestApplicationLoader;
  TestApplicationLoader* url_loader = new TestApplicationLoader;
  TestApplicationLoader* scheme_loader = new TestApplicationLoader;
  application_manager_->set_default_loader(
      scoped_ptr<ApplicationLoader>(default_loader));
  application_manager_->SetLoaderForURL(
      scoped_ptr<ApplicationLoader>(url_loader), GURL("test:test1"));
  application_manager_->SetLoaderForScheme(
      scoped_ptr<ApplicationLoader>(scheme_loader), "test");

  // test::test1 should go to url_loader.
  TestServicePtr test_service;
  application_manager_->ConnectToService(GURL("test:test1"), &test_service);
  EXPECT_EQ(1, url_loader->num_loads());
  EXPECT_EQ(0, scheme_loader->num_loads());
  EXPECT_EQ(0, default_loader->num_loads());

  // test::test2 should go to scheme loader.
  application_manager_->ConnectToService(GURL("test:test2"), &test_service);
  EXPECT_EQ(1, url_loader->num_loads());
  EXPECT_EQ(1, scheme_loader->num_loads());
  EXPECT_EQ(0, default_loader->num_loads());

  // http::test1 should go to default loader.
  application_manager_->ConnectToService(GURL("http:test1"), &test_service);
  EXPECT_EQ(1, url_loader->num_loads());
  EXPECT_EQ(1, scheme_loader->num_loads());
  EXPECT_EQ(1, default_loader->num_loads());
}

// Confirm that the url of a service is correctly passed to another service that
// it loads.
TEST_F(ApplicationManagerTest, ACallB) {
  // Any url can load a.
  AddLoaderForURL(GURL(kTestAURLString), std::string());

  // Only a can load b.
  AddLoaderForURL(GURL(kTestBURLString), kTestAURLString);

  TestAPtr a;
  application_manager_->ConnectToService(GURL(kTestAURLString), &a);
  a->CallB();
  loop_.Run();
  EXPECT_EQ(1, tester_context_.num_b_calls());
  EXPECT_TRUE(tester_context_.a_called_quit());
}

// A calls B which calls C.
TEST_F(ApplicationManagerTest, BCallC) {
  // Any url can load a.
  AddLoaderForURL(GURL(kTestAURLString), std::string());

  // Only a can load b.
  AddLoaderForURL(GURL(kTestBURLString), kTestAURLString);

  TestAPtr a;
  application_manager_->ConnectToService(GURL(kTestAURLString), &a);
  a->CallCFromB();
  loop_.Run();

  EXPECT_EQ(1, tester_context_.num_b_calls());
  EXPECT_EQ(1, tester_context_.num_c_calls());
  EXPECT_TRUE(tester_context_.a_called_quit());
}

// Confirm that a service impl will be deleted if the app that connected to
// it goes away.
TEST_F(ApplicationManagerTest, BDeleted) {
  AddLoaderForURL(GURL(kTestAURLString), std::string());
  AddLoaderForURL(GURL(kTestBURLString), std::string());

  TestAPtr a;
  application_manager_->ConnectToService(GURL(kTestAURLString), &a);

  a->CallB();
  loop_.Run();

  // Kills the a app.
  application_manager_->SetLoaderForURL(scoped_ptr<ApplicationLoader>(),
                                        GURL(kTestAURLString));
  loop_.Run();

  EXPECT_EQ(1, tester_context_.num_b_deletes());
}

// Confirm that the url of a service is correctly passed to another service that
// it loads, and that it can be rejected.
TEST_F(ApplicationManagerTest, ANoLoadB) {
  // Any url can load a.
  AddLoaderForURL(GURL(kTestAURLString), std::string());

  // Only c can load b, so this will fail.
  AddLoaderForURL(GURL(kTestBURLString), "test:TestC");

  TestAPtr a;
  application_manager_->ConnectToService(GURL(kTestAURLString), &a);
  a->CallB();
  loop_.Run();
  EXPECT_EQ(0, tester_context_.num_b_calls());

  EXPECT_FALSE(tester_context_.a_called_quit());
  EXPECT_TRUE(tester_context_.tester_called_quit());
}

TEST_F(ApplicationManagerTest, NoServiceNoLoad) {
  AddLoaderForURL(GURL(kTestAURLString), std::string());

  // There is no TestC service implementation registered with
  // ApplicationManager, so this cannot succeed (but also shouldn't crash).
  TestCPtr c;
  application_manager_->ConnectToService(GURL(kTestAURLString), &c);
  QuitMessageLoopErrorHandler quitter;
  c.set_error_handler(&quitter);

  loop_.Run();
  EXPECT_TRUE(c.encountered_error());
}

TEST_F(ApplicationManagerTest, Interceptor) {
  TestServiceInterceptor interceptor;
  TestApplicationLoader* default_loader = new TestApplicationLoader;
  application_manager_->set_default_loader(
      scoped_ptr<ApplicationLoader>(default_loader));
  application_manager_->SetInterceptor(&interceptor);

  std::string url("test:test3");
  TestServicePtr test_service;
  application_manager_->ConnectToService(GURL(url), &test_service);

  EXPECT_EQ(1, interceptor.call_count());
  EXPECT_EQ(url, interceptor.url_spec());
  EXPECT_EQ(1, default_loader->num_loads());
}

}  // namespace mojo
