// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/at_exit.h"
#include "base/bind.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/stringprintf.h"
#include "mojo/application/public/cpp/application_connection.h"
#include "mojo/application/public/cpp/application_delegate.h"
#include "mojo/application/public/cpp/application_impl.h"
#include "mojo/application/public/cpp/connect.h"
#include "mojo/application/public/cpp/interface_factory.h"
#include "mojo/common/weak_binding_set.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "mojo/shell/application_loader.h"
#include "mojo/shell/application_manager.h"
#include "mojo/shell/capability_filter_unittest.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace shell {
namespace {

// Listens for services exposed/blocked and for application connections being
// closed. Quits |loop| when all expectations are met.
class ConnectionValidator {
 public:
  ConnectionValidator(const std::set<std::string>& expectations,
                      base::MessageLoop* loop)
      : expectations_(expectations),
        loop_(loop) {}
  ~ConnectionValidator() {}

  void AddServiceCalled(const std::string& app_url,
                        const std::string& service_url,
                        const std::string& name,
                        bool blocked) {
    Validate(base::StringPrintf("%s %s %s %s",
        blocked ? "B" : "E", app_url.c_str(), service_url.c_str(),
        name.c_str()));
  }

  void ConnectionClosed(const std::string& app_url,
                        const std::string& service_url) {
    Validate(base::StringPrintf("C %s %s", app_url.c_str(),
                                service_url.c_str()));
  }

  bool expectations_met() { return expectations_.empty(); }

 private:
  void Validate(const std::string& result) {
    DVLOG(1) << "Validate: " << result;
    auto i = expectations_.find(result);
    if (i != expectations_.end()) {
      expectations_.erase(i);
      if (expectations_.empty())
        loop_->Quit();
    } else {
      DVLOG(1) << "Unexpected result.";
      loop_->Quit();
    }
  }

  std::set<std::string> expectations_;
  base::MessageLoop* loop_;

  DISALLOW_COPY_AND_ASSIGN(ConnectionValidator);
};

// This class models an application who will use the shell to interact with a
// system service. The shell may limit this application's visibility of the full
// set of interfaces exposed by that service.
class TestApplication : public ApplicationDelegate,
                        public ApplicationLoader,
                        public InterfaceFactory<Client>,
                        public Client {
 public:
  TestApplication(ConnectionValidator* validator,
                  bool connect_to_test_service_2)
      : validator_(validator),
        connect_to_test_service_2_(connect_to_test_service_2) {}
  ~TestApplication() override {}

 private:
  // Overridden from ApplicationDelegate:
  bool ConfigureIncomingConnection(ApplicationConnection*) override {
    URLRequestPtr request(URLRequest::New());
    request->url = String::From("test:service");
    ApplicationConnection* connection =
        app_->ConnectToApplication(request.Pass());
    connection->SetRemoteServiceProviderConnectionErrorHandler(
        base::Bind(&TestApplication::Connection1Closed,
                   base::Unretained(this)));

    if (connect_to_test_service_2_) {
      URLRequestPtr request2(URLRequest::New());
      request2->url = String::From("test:service2");
      ApplicationConnection* connection2 =
          app_->ConnectToApplication(request2.Pass());
      connection2->SetRemoteServiceProviderConnectionErrorHandler(
          base::Bind(&TestApplication::Connection2Closed,
                     base::Unretained(this)));
    }
    return true;
  }
  bool ConfigureOutgoingConnection(ApplicationConnection* connection) override {
    connection->AddService<Client>(this);
    return true;
  }

  // Overridden from ApplicationLoader:
  void Load(const GURL& url, InterfaceRequest<Application> request) override {
    app_.reset(new ApplicationImpl(this, request.Pass()));
  }

  // Overridden from InterfaceFactory<Client>:
  void Create(ApplicationConnection* connection,
              InterfaceRequest<Client> request) override {
    client_bindings_.AddBinding(this, request.Pass());
  }

  // Overridden from Client:
  void AddServiceCalled(const String& app_url,
                        const String& service_url,
                        const String& name,
                        bool blocked) override {
    validator_->AddServiceCalled(app_url, service_url, name, blocked);
  }

  void Connection1Closed() {
    validator_->ConnectionClosed(app_->url(), "test:service");
  }

  void Connection2Closed() {
    validator_->ConnectionClosed(app_->url(), "test:service2");
  }

  ConnectionValidator* validator_;
  bool connect_to_test_service_2_;
  scoped_ptr<ApplicationImpl> app_;
  WeakBindingSet<Client> client_bindings_;

  DISALLOW_COPY_AND_ASSIGN(TestApplication);
};

// This class models a system service that exposes two interfaces, Safe and
// Unsafe. The interface Unsafe is not to be exposed to untrusted applications.
class ServiceApplication : public ApplicationDelegate,
                           public ApplicationLoader,
                           public InterfaceFactory<Safe>,
                           public InterfaceFactory<Unsafe>,
                           public Safe,
                           public Unsafe {
 public:
  ServiceApplication() {}
  ~ServiceApplication() override {}

 private:
  // Overridden from ApplicationDelegate:
  bool ConfigureIncomingConnection(ApplicationConnection* connection) override {
    connection->ConnectToService(&client_);
    AddService<Safe>(connection);
    AddService<Unsafe>(connection);
    return true;
  }

  // Overridden from ApplicationLoader:
  void Load(const GURL& url,
            InterfaceRequest<Application> application_request) override {
    app_.reset(new ApplicationImpl(this, application_request.Pass()));
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
    client_->AddServiceCalled(connection->GetRemoteApplicationURL(),
                              connection->GetConnectionURL(),
                              Interface::Name_,
                              !connection->AddService<Interface>(this));
  }

  scoped_ptr<ApplicationImpl> app_;
  ClientPtr client_;
  WeakBindingSet<Safe> safe_bindings_;
  WeakBindingSet<Unsafe> unsafe_bindings_;

  DISALLOW_COPY_AND_ASSIGN(ServiceApplication);
};

class TestApplicationManagerDelegate : public ApplicationManager::Delegate {
 public:
   TestApplicationManagerDelegate() {}
   ~TestApplicationManagerDelegate() override {}

 private:
  // Overridden from ApplicationManager::Delegate:
  GURL ResolveMappings(const GURL& url) override {
    return url;
  }
  GURL ResolveMojoURL(const GURL& url) override {
    return url;
  }
  bool CreateFetcher(const GURL& url,
                     const Fetcher::FetchCallback& loader_callback) override {
    return false;
  }

  DISALLOW_COPY_AND_ASSIGN(TestApplicationManagerDelegate);
};

class CapabilityFilterTest : public testing::Test {
 public:
   CapabilityFilterTest() {}
   ~CapabilityFilterTest() override {}

 protected:
  void RunApplication(const std::string& url, CapabilityFilterPtr filter) {
    ServiceProviderPtr services;
    URLRequestPtr request(URLRequest::New());
    request->url = String::From(url);
    application_manager_->ConnectToApplication(
        nullptr, request.Pass(), std::string(), GURL(), GetProxy(&services),
        nullptr, filter.Pass(), base::MessageLoop::QuitWhenIdleClosure());
  }

  base::MessageLoop* loop() { return &loop_;  }
  ApplicationManager* application_manager() {
    return application_manager_.get();
  }

 private:
  // Overridden from testing::Test:
  void SetUp() override {
    application_manager_.reset(new ApplicationManager(&test_delegate_));
  }
  void TearDown() override {
    application_manager_.reset();
  }

  base::ShadowingAtExitManager at_exit_;
  TestApplicationManagerDelegate test_delegate_;
  base::MessageLoop loop_;
  scoped_ptr<ApplicationManager> application_manager_;

  DISALLOW_COPY_AND_ASSIGN(CapabilityFilterTest);
};

TEST_F(CapabilityFilterTest, Blocking) {
  std::set<std::string> expectations;
  expectations.insert("E test:trusted test:service mojo::shell::Safe");
  expectations.insert("E test:trusted test:service mojo::shell::Unsafe");
  expectations.insert("E test:trusted test:service2 mojo::shell::Safe");
  expectations.insert("E test:trusted test:service2 mojo::shell::Unsafe");
  expectations.insert("E test:untrusted test:service mojo::shell::Safe");
  expectations.insert("B test:untrusted test:service mojo::shell::Unsafe");
  expectations.insert("C test:untrusted test:service2");

  ConnectionValidator validator(expectations, loop());
  application_manager()->SetLoaderForURL(
      make_scoped_ptr(new TestApplication(&validator, true)),
      GURL("test:trusted"));
  application_manager()->SetLoaderForURL(
      make_scoped_ptr(new TestApplication(&validator, true)),
      GURL("test:untrusted"));
  application_manager()->SetLoaderForURL(
      make_scoped_ptr(new ServiceApplication), GURL("test:service"));
  application_manager()->SetLoaderForURL(
      make_scoped_ptr(new ServiceApplication), GURL("test:service2"));

  Array<String> interfaces(Array<String>::New(1));
  interfaces[0] = String::From(std::string(Safe::Name_));
  CapabilityFilterPtr filter(CapabilityFilter::New());
  filter->filter.insert("test:service", interfaces.Pass());

  // This first application can only connect to test:service. Connections to
  // test:service2 will be blocked. It also will only be able to see the "Safe"
  // interface exposed by test:service. It will be blocked from seeing "Unsafe".
  RunApplication("test:untrusted", filter.Pass());

  // This second application can connect to both test:service and test:service2.
  // It can connect to both "Safe" and "Unsafe" interfaces.
  RunApplication("test:trusted", nullptr);

  loop()->Run();

  EXPECT_TRUE(validator.expectations_met());
}

TEST_F(CapabilityFilterTest, Wildcards) {
  std::set<std::string> expectations;
  expectations.insert("E test:wildcard test:service mojo::shell::Safe");
  expectations.insert("E test:wildcard test:service mojo::shell::Unsafe");
  expectations.insert("C test:blocked test:service");
  expectations.insert("B test:wildcard2 test:service mojo::shell::Safe");
  expectations.insert("B test:wildcard2 test:service mojo::shell::Unsafe");
  expectations.insert("B test:wildcard2 test:service2 mojo::shell::Safe");
  expectations.insert("B test:wildcard2 test:service2 mojo::shell::Unsafe");
  expectations.insert("E test:wildcard3 test:service mojo::shell::Safe");
  expectations.insert("E test:wildcard3 test:service mojo::shell::Unsafe");
  expectations.insert("E test:wildcard3 test:service2 mojo::shell::Safe");
  expectations.insert("B test:wildcard3 test:service2 mojo::shell::Unsafe");

  ConnectionValidator validator(expectations, loop());
  application_manager()->SetLoaderForURL(
      make_scoped_ptr(new TestApplication(&validator, false)),
      GURL("test:wildcard"));
  application_manager()->SetLoaderForURL(
      make_scoped_ptr(new TestApplication(&validator, false)),
      GURL("test:blocked"));
  application_manager()->SetLoaderForURL(
      make_scoped_ptr(new TestApplication(&validator, true)),
      GURL("test:wildcard2"));
  application_manager()->SetLoaderForURL(
      make_scoped_ptr(new TestApplication(&validator, true)),
      GURL("test:wildcard3"));
  application_manager()->SetLoaderForURL(
      make_scoped_ptr(new ServiceApplication), GURL("test:service"));
  application_manager()->SetLoaderForURL(
      make_scoped_ptr(new ServiceApplication), GURL("test:service2"));

  // This application is allowed to connect to any application because of a
  // wildcard rule, and any interface exposed because of a wildcard rule in
  // the interface array.
  CapabilityFilterPtr filter1(CapabilityFilter::New());
  Array<String> interfaces(Array<String>::New(1));
  interfaces[0] = "*";
  filter1->filter.insert("*", interfaces.Pass());
  RunApplication("test:wildcard", filter1.Pass());

  // This application is allowed to connect to no other applications because of
  // an empty capability filter.
  RunApplication("test:blocked", CapabilityFilter::New());

  // This application is allowed to connect to any application because of a
  // wildcard rule but may not connect to any interfaces because of an empty
  // interface array.
  CapabilityFilterPtr filter2(CapabilityFilter::New());
  filter2->filter.insert("*", Array<String>::New(0));
  RunApplication("test:wildcard2", filter2.Pass());

  // This application is allowed to connect to both test:service and
  // test:service2, and may see any interface exposed by test:service but only
  // the Safe interface exposed by test:service2.
  CapabilityFilterPtr filter3(CapabilityFilter::New());
  Array<String> interfaces1(Array<String>::New(1));
  interfaces1[0] = "*";
  filter3->filter.insert("test:service", interfaces1.Pass());
  Array<String> interfaces2(Array<String>::New(1));
  interfaces2[0] = String::From(std::string(Safe::Name_));
  filter3->filter.insert("test:service2", interfaces2.Pass());
  RunApplication("test:wildcard3", filter3.Pass());

  loop()->Run();

  EXPECT_TRUE(validator.expectations_met());
}

}  // namespace
}  // namespace shell
}  // namespace mojo
