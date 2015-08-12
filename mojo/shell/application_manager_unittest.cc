// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/at_exit.h"
#include "base/bind.h"
#include "base/macros.h"
#include "base/memory/scoped_vector.h"
#include "base/message_loop/message_loop.h"
#include "mojo/application/public/cpp/application_connection.h"
#include "mojo/application/public/cpp/application_delegate.h"
#include "mojo/application/public/cpp/application_impl.h"
#include "mojo/application/public/cpp/interface_factory.h"
#include "mojo/application/public/interfaces/service_provider.mojom.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "mojo/shell/application_loader.h"
#include "mojo/shell/application_manager.h"
#include "mojo/shell/fetcher.h"
#include "mojo/shell/test.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace shell {
namespace {

const char kTestURLString[] = "test:testService";
const char kTestAURLString[] = "test:TestA";
const char kTestBURLString[] = "test:TestB";

const char kTestMimeType[] = "test/mime-type";

class TestMimeTypeFetcher : public Fetcher {
 public:
  explicit TestMimeTypeFetcher(const FetchCallback& fetch_callback)
      : Fetcher(fetch_callback), url_("xxx") {
    loader_callback_.Run(make_scoped_ptr(this));
  }
  ~TestMimeTypeFetcher() override {}

  // Fetcher:
  const GURL& GetURL() const override { return url_; }
  GURL GetRedirectURL() const override { return GURL("yyy"); }
  GURL GetRedirectReferer() const override { return GURL(); }
  URLResponsePtr AsURLResponse(base::TaskRunner* task_runner,
                               uint32_t skip) override {
    return URLResponse::New().Pass();
  }
  void AsPath(
      base::TaskRunner* task_runner,
      base::Callback<void(const base::FilePath&, bool)> callback) override {}
  std::string MimeType() override { return kTestMimeType; }
  bool HasMojoMagic() override { return false; }
  bool PeekFirstLine(std::string* line) override { return false; }

 private:
  const GURL url_;

  DISALLOW_COPY_AND_ASSIGN(TestMimeTypeFetcher);
};

struct TestContext {
  TestContext() : num_impls(0), num_loader_deletes(0) {}
  std::string last_test_string;
  int num_impls;
  int num_loader_deletes;
};

void QuitClosure(bool* value) {
  *value = true;
  base::MessageLoop::current()->QuitWhenIdle();
}

class TestServiceImpl : public TestService {
 public:
  TestServiceImpl(TestContext* context, InterfaceRequest<TestService> request)
      : context_(context), binding_(this, request.Pass()) {
    ++context_->num_impls;
  }

  ~TestServiceImpl() override {
    --context_->num_impls;
    if (!base::MessageLoop::current()->is_running())
      return;
    base::MessageLoop::current()->Quit();
  }

  // TestService implementation:
  void Test(const String& test_string,
            const Callback<void()>& callback) override {
    context_->last_test_string = test_string;
    callback.Run();
  }

 private:
  TestContext* context_;
  StrongBinding<TestService> binding_;
};

class TestClient {
 public:
  explicit TestClient(TestServicePtr service)
      : service_(service.Pass()), quit_after_ack_(false) {}

  void AckTest() {
    if (quit_after_ack_)
      base::MessageLoop::current()->Quit();
  }

  void Test(const std::string& test_string) {
    quit_after_ack_ = true;
    service_->Test(test_string,
                   base::Bind(&TestClient::AckTest, base::Unretained(this)));
  }

 private:
  TestServicePtr service_;
  bool quit_after_ack_;
  DISALLOW_COPY_AND_ASSIGN(TestClient);
};

class TestApplicationLoader : public ApplicationLoader,
                              public ApplicationDelegate,
                              public InterfaceFactory<TestService> {
 public:
  TestApplicationLoader() : context_(nullptr), num_loads_(0) {}

  ~TestApplicationLoader() override {
    if (context_)
      ++context_->num_loader_deletes;
    test_app_.reset();
  }

  void set_context(TestContext* context) { context_ = context; }
  int num_loads() const { return num_loads_; }
  const GURL& last_requestor_url() const { return last_requestor_url_; }

 private:
  // ApplicationLoader implementation.
  void Load(const GURL& url,
            InterfaceRequest<Application> application_request) override {
    ++num_loads_;
    test_app_.reset(new ApplicationImpl(this, application_request.Pass()));
  }

  // ApplicationDelegate implementation.
  bool ConfigureIncomingConnection(ApplicationConnection* connection) override {
    connection->AddService(this);
    last_requestor_url_ = GURL(connection->GetRemoteApplicationURL());
    return true;
  }

  // InterfaceFactory implementation.
  void Create(ApplicationConnection* connection,
              InterfaceRequest<TestService> request) override {
    new TestServiceImpl(context_, request.Pass());
  }

  scoped_ptr<ApplicationImpl> test_app_;
  TestContext* context_;
  int num_loads_;
  GURL last_requestor_url_;

  DISALLOW_COPY_AND_ASSIGN(TestApplicationLoader);
};

class ClosingApplicationLoader : public ApplicationLoader {
 private:
  // ApplicationLoader implementation.
  void Load(const GURL& url,
            InterfaceRequest<Application> application_request) override {}
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
class TestAImpl : public TestA {
 public:
  TestAImpl(ApplicationImpl* app_impl,
            TesterContext* test_context,
            InterfaceRequest<TestA> request)
      : test_context_(test_context), binding_(this, request.Pass()) {
    mojo::URLRequestPtr request2(mojo::URLRequest::New());
    request2->url = mojo::String::From(kTestBURLString);
    connection_ = app_impl->ConnectToApplication(request2.Pass());
    connection_->ConnectToService(&b_);
  }

  ~TestAImpl() override {
    test_context_->IncrementNumADeletes();
    if (base::MessageLoop::current()->is_running())
      Quit();
  }

 private:
  void CallB() override {
    b_->B(base::Bind(&TestAImpl::Quit, base::Unretained(this)));
  }

  void CallCFromB() override {
    b_->CallC(base::Bind(&TestAImpl::Quit, base::Unretained(this)));
  }

  void Quit() {
    base::MessageLoop::current()->Quit();
    test_context_->set_a_called_quit();
    test_context_->QuitSoon();
  }

  scoped_ptr<ApplicationConnection> connection_;
  TesterContext* test_context_;
  TestBPtr b_;
  StrongBinding<TestA> binding_;
};

class TestBImpl : public TestB {
 public:
  TestBImpl(ApplicationConnection* connection,
            TesterContext* test_context,
            InterfaceRequest<TestB> request)
      : test_context_(test_context), binding_(this, request.Pass()) {
    connection->ConnectToService(&c_);
  }

  ~TestBImpl() override {
    test_context_->IncrementNumBDeletes();
    if (base::MessageLoop::current()->is_running())
      base::MessageLoop::current()->Quit();
    test_context_->QuitSoon();
  }

 private:
  void B(const Callback<void()>& callback) override {
    test_context_->IncrementNumBCalls();
    callback.Run();
  }

  void CallC(const Callback<void()>& callback) override {
    test_context_->IncrementNumBCalls();
    c_->C(callback);
  }

  TesterContext* test_context_;
  TestCPtr c_;
  StrongBinding<TestB> binding_;
};

class TestCImpl : public TestC {
 public:
  TestCImpl(ApplicationConnection* connection,
            TesterContext* test_context,
            InterfaceRequest<TestC> request)
      : test_context_(test_context), binding_(this, request.Pass()) {}

  ~TestCImpl() override { test_context_->IncrementNumCDeletes(); }

 private:
  void C(const Callback<void()>& callback) override {
    test_context_->IncrementNumCCalls();
    callback.Run();
  }

  TesterContext* test_context_;
  StrongBinding<TestC> binding_;
};

class Tester : public ApplicationDelegate,
               public ApplicationLoader,
               public InterfaceFactory<TestA>,
               public InterfaceFactory<TestB>,
               public InterfaceFactory<TestC> {
 public:
  Tester(TesterContext* context, const std::string& requestor_url)
      : context_(context), requestor_url_(requestor_url) {}
  ~Tester() override {}

 private:
  void Load(const GURL& url,
            InterfaceRequest<Application> application_request) override {
    app_.reset(new ApplicationImpl(this, application_request.Pass()));
  }

  bool ConfigureIncomingConnection(ApplicationConnection* connection) override {
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

  bool ConfigureOutgoingConnection(ApplicationConnection* connection) override {
    // If we're connecting to B, then add C.
    if (connection->GetRemoteApplicationURL() == kTestBURLString)
      connection->AddService<TestC>(this);
    return true;
  }

  void Create(ApplicationConnection* connection,
              InterfaceRequest<TestA> request) override {
    a_bindings_.push_back(new TestAImpl(app_.get(), context_, request.Pass()));
  }

  void Create(ApplicationConnection* connection,
              InterfaceRequest<TestB> request) override {
    new TestBImpl(connection, context_, request.Pass());
  }

  void Create(ApplicationConnection* connection,
              InterfaceRequest<TestC> request) override {
    new TestCImpl(connection, context_, request.Pass());
  }

  TesterContext* context_;
  scoped_ptr<ApplicationImpl> app_;
  std::string requestor_url_;
  ScopedVector<TestAImpl> a_bindings_;
};

class TestDelegate : public ApplicationManager::Delegate {
 public:
  TestDelegate() : create_test_fetcher_(false) {}
  ~TestDelegate() override {}

  void AddMapping(const GURL& from, const GURL& to) { mappings_[from] = to; }

  void set_create_test_fetcher(bool create_test_fetcher) {
    create_test_fetcher_ = create_test_fetcher;
  }

  // ApplicationManager::Delegate
  GURL ResolveMappings(const GURL& url) override {
    auto it = mappings_.find(url);
    if (it != mappings_.end())
      return it->second;
    return url;
  }
  GURL ResolveMojoURL(const GURL& url) override {
    GURL mapped_url = ResolveMappings(url);
    // The shell automatically map mojo URLs.
    if (mapped_url.scheme() == "mojo") {
      url::Replacements<char> replacements;
      replacements.SetScheme("file", url::Component(0, 4));
      mapped_url = mapped_url.ReplaceComponents(replacements);
    }
    return mapped_url;
  }
  bool CreateFetcher(const GURL& url,
                     const Fetcher::FetchCallback& loader_callback) override {
    if (!create_test_fetcher_)
      return false;
    new TestMimeTypeFetcher(loader_callback);
    return true;
  }

 private:
  std::map<GURL, GURL> mappings_;
  bool create_test_fetcher_;

  DISALLOW_COPY_AND_ASSIGN(TestDelegate);
};

class ApplicationManagerTest : public testing::Test {
 public:
  ApplicationManagerTest() : tester_context_(&loop_) {}

  ~ApplicationManagerTest() override {}

  void SetUp() override {
    application_manager_.reset(new ApplicationManager(&test_delegate_));
    test_loader_ = new TestApplicationLoader;
    test_loader_->set_context(&context_);
    application_manager_->set_default_loader(
        scoped_ptr<ApplicationLoader>(test_loader_));

    TestServicePtr service_proxy;
    application_manager_->ConnectToService(GURL(kTestURLString),
                                           &service_proxy);
    test_client_.reset(new TestClient(service_proxy.Pass()));
  }

  void TearDown() override {
    test_client_.reset();
    application_manager_.reset();
  }

  void AddLoaderForURL(const GURL& url, const std::string& requestor_url) {
    application_manager_->SetLoaderForURL(
        make_scoped_ptr(new Tester(&tester_context_, requestor_url)), url);
  }

  bool HasRunningInstanceForURL(const GURL& url) {
    ApplicationManager::TestAPI manager_test_api(application_manager_.get());
    return manager_test_api.HasRunningInstanceForURL(url);
  }

 protected:
  base::ShadowingAtExitManager at_exit_;
  TestDelegate test_delegate_;
  TestApplicationLoader* test_loader_;
  TesterContext tester_context_;
  TestContext context_;
  base::MessageLoop loop_;
  scoped_ptr<TestClient> test_client_;
  scoped_ptr<ApplicationManager> application_manager_;
  DISALLOW_COPY_AND_ASSIGN(ApplicationManagerTest);
};

TEST_F(ApplicationManagerTest, Basic) {
  test_client_->Test("test");
  loop_.Run();
  EXPECT_EQ(std::string("test"), context_.last_test_string);
}

// Confirm that url mappings are respected.
TEST_F(ApplicationManagerTest, URLMapping) {
  ApplicationManager am(&test_delegate_);
  GURL test_url("test:test");
  GURL test_url2("test:test2");
  test_delegate_.AddMapping(test_url, test_url2);
  TestApplicationLoader* loader = new TestApplicationLoader;
  loader->set_context(&context_);
  am.SetLoaderForURL(scoped_ptr<ApplicationLoader>(loader), test_url2);
  {
    // Connext to the mapped url
    TestServicePtr test_service;
    am.ConnectToService(test_url, &test_service);
    TestClient test_client(test_service.Pass());
    test_client.Test("test");
    loop_.Run();
  }
  {
    // Connext to the target url
    TestServicePtr test_service;
    am.ConnectToService(test_url2, &test_service);
    TestClient test_client(test_service.Pass());
    test_client.Test("test");
    loop_.Run();
  }
}

TEST_F(ApplicationManagerTest, ClientError) {
  test_client_->Test("test");
  EXPECT_TRUE(HasRunningInstanceForURL(GURL(kTestURLString)));
  loop_.Run();
  EXPECT_EQ(1, context_.num_impls);
  test_client_.reset();
  loop_.Run();
  EXPECT_EQ(0, context_.num_impls);
  EXPECT_TRUE(HasRunningInstanceForURL(GURL(kTestURLString)));
}

TEST_F(ApplicationManagerTest, Deletes) {
  {
    ApplicationManager am(&test_delegate_);
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
    am.set_default_loader(scoped_ptr<ApplicationLoader>(default_loader));
    am.SetLoaderForURL(scoped_ptr<ApplicationLoader>(url_loader1),
                       GURL("test:test1"));
    am.SetLoaderForURL(scoped_ptr<ApplicationLoader>(url_loader2),
                       GURL("test:test1"));
    am.SetLoaderForScheme(scoped_ptr<ApplicationLoader>(scheme_loader1),
                          "test");
    am.SetLoaderForScheme(scoped_ptr<ApplicationLoader>(scheme_loader2),
                          "test");
  }
  EXPECT_EQ(5, context_.num_loader_deletes);
}

// Confirm that both urls and schemes can have their loaders explicitly set.
TEST_F(ApplicationManagerTest, SetLoaders) {
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
  c.set_connection_error_handler(
      []() { base::MessageLoop::current()->QuitWhenIdle(); });

  loop_.Run();
  EXPECT_TRUE(c.encountered_error());
}

TEST_F(ApplicationManagerTest, MappedURLsShouldNotCauseDuplicateLoad) {
  test_delegate_.AddMapping(GURL("foo:foo2"), GURL("foo:foo"));
  // 1 because ApplicationManagerTest connects once at startup.
  EXPECT_EQ(1, test_loader_->num_loads());

  TestServicePtr test_service;
  application_manager_->ConnectToService(GURL("foo:foo"), &test_service);
  EXPECT_EQ(2, test_loader_->num_loads());

  TestServicePtr test_service2;
  application_manager_->ConnectToService(GURL("foo:foo2"), &test_service2);
  EXPECT_EQ(2, test_loader_->num_loads());

  TestServicePtr test_service3;
  application_manager_->ConnectToService(GURL("bar:bar"), &test_service2);
  EXPECT_EQ(3, test_loader_->num_loads());
}

TEST_F(ApplicationManagerTest, MappedURLsShouldWorkWithLoaders) {
  TestApplicationLoader* custom_loader = new TestApplicationLoader;
  TestContext context;
  custom_loader->set_context(&context);
  application_manager_->SetLoaderForURL(make_scoped_ptr(custom_loader),
                                        GURL("mojo:foo"));
  test_delegate_.AddMapping(GURL("mojo:foo2"), GURL("mojo:foo"));

  TestServicePtr test_service;
  application_manager_->ConnectToService(GURL("mojo:foo2"), &test_service);
  EXPECT_EQ(1, custom_loader->num_loads());
  custom_loader->set_context(nullptr);

  EXPECT_TRUE(HasRunningInstanceForURL(GURL("mojo:foo2")));
  EXPECT_FALSE(HasRunningInstanceForURL(GURL("mojo:foo")));
}

TEST_F(ApplicationManagerTest, TestQueryWithLoaders) {
  TestApplicationLoader* url_loader = new TestApplicationLoader;
  TestApplicationLoader* scheme_loader = new TestApplicationLoader;
  application_manager_->SetLoaderForURL(
      scoped_ptr<ApplicationLoader>(url_loader), GURL("test:test1"));
  application_manager_->SetLoaderForScheme(
      scoped_ptr<ApplicationLoader>(scheme_loader), "test");

  // test::test1 should go to url_loader.
  TestServicePtr test_service;
  application_manager_->ConnectToService(GURL("test:test1?foo=bar"),
                                         &test_service);
  EXPECT_EQ(1, url_loader->num_loads());
  EXPECT_EQ(0, scheme_loader->num_loads());

  // test::test2 should go to scheme loader.
  application_manager_->ConnectToService(GURL("test:test2?foo=bar"),
                                         &test_service);
  EXPECT_EQ(1, url_loader->num_loads());
  EXPECT_EQ(1, scheme_loader->num_loads());
}

TEST_F(ApplicationManagerTest, TestEndApplicationClosure) {
  ClosingApplicationLoader* loader = new ClosingApplicationLoader();
  application_manager_->SetLoaderForScheme(
      scoped_ptr<ApplicationLoader>(loader), "test");

  bool called = false;
  mojo::URLRequestPtr request(mojo::URLRequest::New());
  request->url = mojo::String::From("test:test");
  application_manager_->ConnectToApplication(
      nullptr, request.Pass(), std::string(), GURL(), nullptr, nullptr,
      GetPermissiveCapabilityFilter(),
      base::Bind(&QuitClosure, base::Unretained(&called)));
  loop_.Run();
  EXPECT_TRUE(called);
}

TEST(ApplicationManagerTest2, ContentHandlerConnectionGetsRequestorURL) {
  const GURL content_handler_url("http://test.content.handler");
  const GURL requestor_url("http://requestor.url");
  TestContext test_context;
  base::MessageLoop loop;
  TestDelegate test_delegate;
  test_delegate.set_create_test_fetcher(true);
  ApplicationManager application_manager(&test_delegate);
  application_manager.set_default_loader(nullptr);
  application_manager.RegisterContentHandler(kTestMimeType,
                                             content_handler_url);

  TestApplicationLoader* loader = new TestApplicationLoader;
  loader->set_context(&test_context);
  application_manager.SetLoaderForURL(scoped_ptr<ApplicationLoader>(loader),
                                      content_handler_url);

  bool called = false;
  mojo::URLRequestPtr request(mojo::URLRequest::New());
  request->url = mojo::String::From("test:test");
  application_manager.ConnectToApplication(
      nullptr, request.Pass(), std::string(), requestor_url, nullptr, nullptr,
      GetPermissiveCapabilityFilter(),
      base::Bind(&QuitClosure, base::Unretained(&called)));
  loop.Run();
  EXPECT_TRUE(called);

  ASSERT_EQ(1, loader->num_loads());
  EXPECT_EQ(requestor_url, loader->last_requestor_url());
}

}  // namespace
}  // namespace shell
}  // namespace mojo
