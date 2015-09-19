// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/at_exit.h"
#include "base/bind.h"
#include "base/macros.h"
#include "base/memory/scoped_vector.h"
#include "base/message_loop/message_loop.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "mojo/application/public/cpp/application_connection.h"
#include "mojo/application/public/cpp/application_delegate.h"
#include "mojo/application/public/cpp/application_impl.h"
#include "mojo/application/public/cpp/connect.h"
#include "mojo/application/public/cpp/interface_factory.h"
#include "mojo/application/public/cpp/service_provider_impl.h"
#include "mojo/application/public/interfaces/content_handler.mojom.h"
#include "mojo/common/weak_binding_set.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "mojo/shell/application_loader.h"
#include "mojo/shell/application_manager.h"
#include "mojo/shell/capability_filter_unittest.mojom.h"
#include "mojo/shell/test_package_manager.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace shell {
namespace {

const char kTestMimeType[] = "test/mime-type";

// Lives on the main thread of the test.
// Listens for services exposed/blocked and for application connections being
// closed. Quits |loop| when all expectations are met.
class ConnectionValidator : public ApplicationLoader,
                            public ApplicationDelegate,
                            public InterfaceFactory<Validator>,
                            public Validator {
 public:
  ConnectionValidator(const std::set<std::string>& expectations,
                      base::MessageLoop* loop)
      : app_(nullptr),
        expectations_(expectations),
        loop_(loop) {}
  ~ConnectionValidator() override {}

  bool expectations_met() {
    return unexpected_.empty() && expectations_.empty();
  }

  void PrintUnmetExpectations() {
    for (auto expectation : expectations_)
      ADD_FAILURE() << "Unmet: " << expectation;
    for (auto unexpected : unexpected_)
      ADD_FAILURE() << "Unexpected: " << unexpected;
  }

 private:
  // Overridden from ApplicationLoader:
  void Load(const GURL& url, InterfaceRequest<Application> request) override {
    app_.reset(new ApplicationImpl(this, request.Pass()));
  }

  // Overridden from ApplicationDelegate:
  bool ConfigureIncomingConnection(ApplicationConnection* connection) override {
    connection->AddService<Validator>(this);
    return true;
  }

  // Overridden from InterfaceFactory<Validator>:
  void Create(ApplicationConnection* connection,
              InterfaceRequest<Validator> request) override {
    validator_bindings_.AddBinding(this, request.Pass());
  }

  // Overridden from Validator:
  void AddServiceCalled(const String& app_url,
                        const String& service_url,
                        const String& name,
                        bool blocked) override {
    Validate(base::StringPrintf("%s %s %s %s",
        blocked ? "B" : "E", app_url.data(), service_url.data(), name.data()));
  }
  void ConnectionClosed(const String& app_url,
                        const String& service_url) override {
    Validate(base::StringPrintf("C %s %s", app_url.data(), service_url.data()));
  }

  void Validate(const std::string& result) {
    DVLOG(1) << "Validate: " << result;
    auto i = expectations_.find(result);
    if (i != expectations_.end()) {
      expectations_.erase(i);
      if (expectations_.empty())
        loop_->Quit();
    } else {
      // This is a test failure, and will result in PrintUnexpectedExpecations()
      // being called.
      unexpected_.insert(result);
      loop_->Quit();
    }
  }

  scoped_ptr<ApplicationImpl> app_;
  std::set<std::string> expectations_;
  std::set<std::string> unexpected_;
  base::MessageLoop* loop_;
  WeakBindingSet<Validator> validator_bindings_;

  DISALLOW_COPY_AND_ASSIGN(ConnectionValidator);
};

// This class models an application who will use the shell to interact with a
// system service. The shell may limit this application's visibility of the full
// set of interfaces exposed by that service.
class TestApplication : public ApplicationDelegate {
 public:
  TestApplication() : app_(nullptr) {}
  ~TestApplication() override {}

 private:
  // Overridden from ApplicationDelegate:
  void Initialize(ApplicationImpl* app) override {
    app_ = app;
  }
  bool ConfigureIncomingConnection(ApplicationConnection* connection) override {
    // TestApplications receive their Validator via the inbound connection.
    connection->ConnectToService(&validator_);

    URLRequestPtr request(URLRequest::New());
    request->url = String::From("test:service");
    connection1_ = app_->ConnectToApplication(request.Pass());
    connection1_->SetRemoteServiceProviderConnectionErrorHandler(
        base::Bind(&TestApplication::ConnectionClosed,
                   base::Unretained(this), "test:service"));

    URLRequestPtr request2(URLRequest::New());
    request2->url = String::From("test:service2");
    connection2_ = app_->ConnectToApplication(request2.Pass());
    connection2_->SetRemoteServiceProviderConnectionErrorHandler(
        base::Bind(&TestApplication::ConnectionClosed,
                    base::Unretained(this), "test:service2"));
    return true;
  }

  void ConnectionClosed(const std::string& service_url) {
    validator_->ConnectionClosed(app_->url(), service_url);
  }

  ApplicationImpl* app_;
  ValidatorPtr validator_;
  scoped_ptr<ApplicationConnection> connection1_;
  scoped_ptr<ApplicationConnection> connection2_;

  DISALLOW_COPY_AND_ASSIGN(TestApplication);
};

class TestContentHandler : public ApplicationDelegate,
                           public InterfaceFactory<ContentHandler>,
                           public ContentHandler {
 public:
  TestContentHandler() : app_(nullptr) {}
  ~TestContentHandler() override {}

 private:
  // Overridden from ApplicationDelegate:
  void Initialize(ApplicationImpl* app) override {
    app_ = app;
  }
  bool ConfigureIncomingConnection(ApplicationConnection* connection) override {
    connection->AddService<ContentHandler>(this);
    return true;
  }

  // Overridden from InterfaceFactory<ContentHandler>:
  void Create(ApplicationConnection* connection,
              InterfaceRequest<ContentHandler> request) override {
    bindings_.AddBinding(this, request.Pass());
  }

  // Overridden from ContentHandler:
  void StartApplication(InterfaceRequest<Application> application,
                        URLResponsePtr response) override {
    scoped_ptr<ApplicationDelegate> delegate(new TestApplication);
    embedded_apps_.push_back(
        new ApplicationImpl(delegate.get(), application.Pass()));
    embedded_app_delegates_.push_back(delegate.Pass());
  }

  ApplicationImpl* app_;
  WeakBindingSet<ContentHandler> bindings_;
  ScopedVector<ApplicationDelegate> embedded_app_delegates_;
  ScopedVector<ApplicationImpl> embedded_apps_;

  DISALLOW_COPY_AND_ASSIGN(TestContentHandler);
};

// This class models a system service that exposes two interfaces, Safe and
// Unsafe. The interface Unsafe is not to be exposed to untrusted applications.
class ServiceApplication : public ApplicationDelegate,
                           public InterfaceFactory<Safe>,
                           public InterfaceFactory<Unsafe>,
                           public Safe,
                           public Unsafe {
 public:
  ServiceApplication() : app_(nullptr) {}
  ~ServiceApplication() override {}

 private:
  // Overridden from ApplicationDelegate:
  void Initialize(ApplicationImpl* app) override {
    app_ = app;
    // ServiceApplications have no capability filter and can thus connect
    // directly to the validator application.
    URLRequestPtr request(URLRequest::New());
    request->url = String::From("test:validator");
    app_->ConnectToService(request.Pass(), &validator_);
  }
  bool ConfigureIncomingConnection(ApplicationConnection* connection) override {
    AddService<Safe>(connection);
    AddService<Unsafe>(connection);
    return true;
  }

  // Overridden from InterfaceFactory<Safe>:
  void Create(ApplicationConnection* connection,
              InterfaceRequest<Safe> request) override {
    safe_bindings_.AddBinding(this, request.Pass());
  }

  // Overridden from InterfaceFactory<Unsafe>:
  void Create(ApplicationConnection* connection,
              InterfaceRequest<Unsafe> request) override {
    unsafe_bindings_.AddBinding(this, request.Pass());
  }

  template <typename Interface>
  void AddService(ApplicationConnection* connection) {
    validator_->AddServiceCalled(connection->GetRemoteApplicationURL(),
                                 connection->GetConnectionURL(),
                                 Interface::Name_,
                                 !connection->AddService<Interface>(this));
  }

  ApplicationImpl* app_;
  ValidatorPtr validator_;
  WeakBindingSet<Safe> safe_bindings_;
  WeakBindingSet<Unsafe> unsafe_bindings_;

  DISALLOW_COPY_AND_ASSIGN(ServiceApplication);
};

// A custom Fetcher used to trigger a content handler for kTestMimeType for a
// specific test.
class TestFetcher : public Fetcher {
 public:
  TestFetcher(const GURL& url, const FetchCallback& callback)
      : Fetcher(callback),
        url_(url) {
    loader_callback_.Run(make_scoped_ptr(this));
  }
  ~TestFetcher() override {}

 private:
  // Overridden from Fetcher:
  const GURL& GetURL() const override { return url_; }
  GURL GetRedirectURL() const override { return GURL(); }
  GURL GetRedirectReferer() const override { return GURL(); }
  URLResponsePtr AsURLResponse(base::TaskRunner* task_runner,
                               uint32_t skip) override {
    URLResponsePtr response(URLResponse::New());
    response->url = url_.spec();
    return response.Pass();
  }
  void AsPath(
      base::TaskRunner* task_runner,
      base::Callback<void(const base::FilePath&, bool)> callback) override {}
  std::string MimeType() override { return kTestMimeType; }
  bool HasMojoMagic() override { return false; }
  bool PeekFirstLine(std::string* line) override { return false; }

  const GURL url_;

  DISALLOW_COPY_AND_ASSIGN(TestFetcher);
};

class CFTestPackageManager : public TestPackageManager {
 public:
  CFTestPackageManager() {}
  ~CFTestPackageManager() override {}

   void set_use_test_fetcher(bool use_test_fetcher) {
     use_test_fetcher_ = use_test_fetcher;
   }

 private:
  // Overridden from TestPackageManager:
  void FetchRequest(URLRequestPtr request,
                    const Fetcher::FetchCallback& loader_callback) override {
    if (use_test_fetcher_)
      new TestFetcher(GURL(request->url), loader_callback);
  }

  bool use_test_fetcher_;

  DISALLOW_COPY_AND_ASSIGN(CFTestPackageManager);
};

class TestLoader : public ApplicationLoader {
 public:
  explicit TestLoader(ApplicationDelegate* delegate) : delegate_(delegate) {}
  ~TestLoader() override {}

 private:
  // Overridden from ApplicationLoader:
  void Load(const GURL& url, InterfaceRequest<Application> request) override {
    app_.reset(new ApplicationImpl(delegate_.get(), request.Pass()));
  }

  scoped_ptr<ApplicationDelegate> delegate_;
  scoped_ptr<ApplicationImpl> app_;

  DISALLOW_COPY_AND_ASSIGN(TestLoader);
};

class CapabilityFilterTest : public testing::Test {
 public:
   CapabilityFilterTest()
      : test_package_manager_(nullptr),
        validator_(nullptr) {}
   ~CapabilityFilterTest() override {}

 protected:
  void RunApplication(const std::string& url, const CapabilityFilter& filter) {
    ServiceProviderPtr services;

    // We expose Validator to the test application via ConnectToApplication
    // because we don't allow the test application to connect to test:validator.
    // Adding it to the CapabilityFilter would interfere with the test.
    ServiceProviderPtr exposed_services;
    (new ServiceProviderImpl(GetProxy(&exposed_services)))->
        AddService<Validator>(validator_);
    scoped_ptr<ConnectToApplicationParams> params(
        new ConnectToApplicationParams);
    params->SetTarget(Identity(GURL(url), std::string(), filter));
    params->set_services(GetProxy(&services));
    params->set_exposed_services(exposed_services.Pass());
    params->set_on_application_end(base::MessageLoop::QuitWhenIdleClosure());
    application_manager_->ConnectToApplication(params.Pass());
  }

  void InitValidator(const std::set<std::string>& expectations) {
    validator_ = new ConnectionValidator(expectations, &loop_);
    application_manager()->SetLoaderForURL(make_scoped_ptr(validator_),
                                           GURL("test:validator"));
  }

  template <class T>
  void CreateLoader(const std::string& url) {
    application_manager_->SetLoaderForURL(
        make_scoped_ptr(new TestLoader(new T)), GURL(url));
  }

  virtual void RunTest() {
    loop()->Run();
    EXPECT_TRUE(validator_->expectations_met());
    if (!validator_->expectations_met())
      validator_->PrintUnmetExpectations();
  }

  void RunContentHandlerTest() {
    set_use_test_fetcher();

    GURL content_handler_url("test:content_handler");
    test_package_manager_->RegisterContentHandler(kTestMimeType,
                                                  content_handler_url);

    CreateLoader<TestContentHandler>(content_handler_url.spec());
    RunTest();
  }

  base::MessageLoop* loop() { return &loop_;  }
  ApplicationManager* application_manager() {
    return application_manager_.get();
  }
  ConnectionValidator* validator() { return validator_; }
  void set_use_test_fetcher() {
    test_package_manager_->set_use_test_fetcher(true);
  }

  // Overridden from testing::Test:
  void SetUp() override {
    test_package_manager_ = new CFTestPackageManager;
    application_manager_.reset(
        new ApplicationManager(make_scoped_ptr(test_package_manager_)));
    CreateLoader<ServiceApplication>("test:service");
    CreateLoader<ServiceApplication>("test:service2");
  }
  void TearDown() override {
    application_manager_.reset();
    test_package_manager_->set_use_test_fetcher(false);
  }

 private:
  template<class T>
  scoped_ptr<ApplicationDelegate> CreateApplicationDelegate() {
    return scoped_ptr<ApplicationDelegate>(new T);
  }

  CFTestPackageManager* test_package_manager_;
  base::ShadowingAtExitManager at_exit_;
  base::MessageLoop loop_;
  scoped_ptr<ApplicationManager> application_manager_;
  ConnectionValidator* validator_;

  DISALLOW_COPY_AND_ASSIGN(CapabilityFilterTest);
};

class CapabilityFilter_BlockingTest : public CapabilityFilterTest {
 public:
  CapabilityFilter_BlockingTest() {}
  ~CapabilityFilter_BlockingTest() override {}

 protected:
  void RunTest() override {
    // This first application can only connect to test:service. Connections to
    // test:service2 will be blocked. It also will only be able to see the
    // "Safe" interface exposed by test:service. It will be blocked from seeing
    // "Unsafe".
    AllowedInterfaces interfaces;
    interfaces.insert(Safe::Name_);
    CapabilityFilter filter;
    filter["test:service"] = interfaces;
    RunApplication("test:untrusted", filter);

    // This second application can connect to both test:service and
    // test:service2. It can connect to both "Safe" and "Unsafe" interfaces.
    RunApplication("test:trusted", GetPermissiveCapabilityFilter());

    CapabilityFilterTest::RunTest();
  }

 private:
  // Overridden from CapabilityFilterTest:
  void SetUp() override {
    CapabilityFilterTest::SetUp();

    std::set<std::string> expectations;
    expectations.insert("E test:trusted test:service mojo::shell::Safe");
    expectations.insert("E test:trusted test:service mojo::shell::Unsafe");
    expectations.insert("E test:trusted test:service2 mojo::shell::Safe");
    expectations.insert("E test:trusted test:service2 mojo::shell::Unsafe");
    expectations.insert("E test:untrusted test:service mojo::shell::Safe");
    expectations.insert("B test:untrusted test:service mojo::shell::Unsafe");
    expectations.insert("C test:untrusted test:service2");
    InitValidator(expectations);
  }

  DISALLOW_COPY_AND_ASSIGN(CapabilityFilter_BlockingTest);
};

TEST_F(CapabilityFilter_BlockingTest, Application) {
  CreateLoader<TestApplication>("test:trusted");
  CreateLoader<TestApplication>("test:untrusted");
  RunTest();
}

TEST_F(CapabilityFilter_BlockingTest, ContentHandler) {
  RunContentHandlerTest();
}

class CapabilityFilter_WildcardsTest : public CapabilityFilterTest {
 public:
  CapabilityFilter_WildcardsTest() {}
  ~CapabilityFilter_WildcardsTest() override {}

 protected:
  void RunTest() override {
    // This application is allowed to connect to any application because of a
    // wildcard rule, and any interface exposed because of a wildcard rule in
    // the interface array.
    RunApplication("test:wildcard", GetPermissiveCapabilityFilter());

    // This application is allowed to connect to no other applications because
    // of an empty capability filter.
    RunApplication("test:blocked", CapabilityFilter());

    // This application is allowed to connect to any application because of a
    // wildcard rule but may not connect to any interfaces because of an empty
    // interface array.
    CapabilityFilter filter1;
    filter1["*"] = AllowedInterfaces();
    RunApplication("test:wildcard2", filter1);

    // This application is allowed to connect to both test:service and
    // test:service2, and may see any interface exposed by test:service but only
    // the Safe interface exposed by test:service2.
    AllowedInterfaces interfaces2;
    interfaces2.insert("*");
    CapabilityFilter filter2;
    filter2["test:service"] = interfaces2;
    AllowedInterfaces interfaces3;
    interfaces3.insert(Safe::Name_);
    filter2["test:service2"] = interfaces3;
    RunApplication("test:wildcard3", filter2);

    CapabilityFilterTest::RunTest();
  }

 private:
  // Overridden from CapabilityFilterTest:
  void SetUp() override {
    CapabilityFilterTest::SetUp();

    std::set<std::string> expectations;
    expectations.insert("E test:wildcard test:service mojo::shell::Safe");
    expectations.insert("E test:wildcard test:service mojo::shell::Unsafe");
    expectations.insert("E test:wildcard test:service2 mojo::shell::Safe");
    expectations.insert("E test:wildcard test:service2 mojo::shell::Unsafe");
    expectations.insert("C test:blocked test:service");
    expectations.insert("C test:blocked test:service2");
    expectations.insert("B test:wildcard2 test:service mojo::shell::Safe");
    expectations.insert("B test:wildcard2 test:service mojo::shell::Unsafe");
    expectations.insert("B test:wildcard2 test:service2 mojo::shell::Safe");
    expectations.insert("B test:wildcard2 test:service2 mojo::shell::Unsafe");
    expectations.insert("E test:wildcard3 test:service mojo::shell::Safe");
    expectations.insert("E test:wildcard3 test:service mojo::shell::Unsafe");
    expectations.insert("E test:wildcard3 test:service2 mojo::shell::Safe");
    expectations.insert("B test:wildcard3 test:service2 mojo::shell::Unsafe");
    InitValidator(expectations);
  }

  DISALLOW_COPY_AND_ASSIGN(CapabilityFilter_WildcardsTest);
};

TEST_F(CapabilityFilter_WildcardsTest, Application) {
  CreateLoader<TestApplication>("test:wildcard");
  CreateLoader<TestApplication>("test:blocked");
  CreateLoader<TestApplication>("test:wildcard2");
  CreateLoader<TestApplication>("test:wildcard3");
  RunTest();
}

TEST_F(CapabilityFilter_WildcardsTest, ContentHandler) {
  RunContentHandlerTest();
}

}  // namespace
}  // namespace shell
}  // namespace mojo
