// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/shell/capability_filter_test.h"

#include <utility>

#include "base/macros.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "mojo/common/weak_binding_set.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "mojo/shell/application_loader.h"
#include "mojo/shell/package_manager.h"
#include "mojo/shell/public/cpp/connection.h"
#include "mojo/shell/public/cpp/interface_factory.h"
#include "mojo/shell/public/cpp/shell_connection.h"

namespace mojo {
namespace shell {
namespace test {

// Lives on the main thread of the test.
// Listens for services exposed/blocked and for application connections being
// closed. Quits |loop| when all expectations are met.
class ConnectionValidator : public ApplicationLoader,
                            public ShellClient,
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
  void Load(const GURL& url,
            InterfaceRequest<mojom::ShellClient> request) override {
    app_.reset(new ShellConnection(this, std::move(request)));
  }

  // Overridden from ShellClient:
  bool AcceptConnection(Connection* connection) override {
    connection->AddService<Validator>(this);
    return true;
  }

  // Overridden from InterfaceFactory<Validator>:
  void Create(Connection* connection,
              InterfaceRequest<Validator> request) override {
    validator_bindings_.AddBinding(this, std::move(request));
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
        loop_->QuitWhenIdle();
    } else {
      // This is a test failure, and will result in PrintUnexpectedExpecations()
      // being called.
      unexpected_.insert(result);
      loop_->QuitWhenIdle();
    }
  }

  scoped_ptr<ShellConnection> app_;
  std::set<std::string> expectations_;
  std::set<std::string> unexpected_;
  base::MessageLoop* loop_;
  WeakBindingSet<Validator> validator_bindings_;

  DISALLOW_COPY_AND_ASSIGN(ConnectionValidator);
};

// This class models a system service that exposes two interfaces, Safe and
// Unsafe. The interface Unsafe is not to be exposed to untrusted applications.
class ServiceApplication : public ShellClient,
                           public InterfaceFactory<Safe>,
                           public InterfaceFactory<Unsafe>,
                           public Safe,
                           public Unsafe {
 public:
  ServiceApplication() : shell_(nullptr) {}
  ~ServiceApplication() override {}

 private:
  // Overridden from ShellClient:
  void Initialize(Shell* shell, const std::string& url, uint32_t id) override {
    shell_ = shell;
    // ServiceApplications have no capability filter and can thus connect
    // directly to the validator application.
    shell_->ConnectToService("test:validator", &validator_);
  }
  bool AcceptConnection(Connection* connection) override {
    AddService<Safe>(connection);
    AddService<Unsafe>(connection);
    return true;
  }

  // Overridden from InterfaceFactory<Safe>:
  void Create(Connection* connection,
              InterfaceRequest<Safe> request) override {
    safe_bindings_.AddBinding(this, std::move(request));
  }

  // Overridden from InterfaceFactory<Unsafe>:
  void Create(Connection* connection,
              InterfaceRequest<Unsafe> request) override {
    unsafe_bindings_.AddBinding(this, std::move(request));
  }

  template <typename Interface>
  void AddService(Connection* connection) {
    validator_->AddServiceCalled(connection->GetRemoteApplicationURL(),
                                 connection->GetConnectionURL(),
                                 Interface::Name_,
                                 !connection->AddService<Interface>(this));
  }

  Shell* shell_;
  ValidatorPtr validator_;
  WeakBindingSet<Safe> safe_bindings_;
  WeakBindingSet<Unsafe> unsafe_bindings_;

  DISALLOW_COPY_AND_ASSIGN(ServiceApplication);
};

////////////////////////////////////////////////////////////////////////////////
// TestApplication:

TestApplication::TestApplication() : shell_(nullptr) {}
TestApplication::~TestApplication() {}

void TestApplication::Initialize(Shell* shell, const std::string& url,
                                 uint32_t id) {
  shell_ = shell;
  url_ = url;
}
bool TestApplication::AcceptConnection(Connection* connection) {
  // TestApplications receive their Validator via the inbound connection.
  connection->ConnectToService(&validator_);

  connection1_ = shell_->Connect("test:service");
  connection1_->SetRemoteServiceProviderConnectionErrorHandler(
      base::Bind(&TestApplication::ConnectionClosed,
                  base::Unretained(this), "test:service"));

  connection2_ = shell_->Connect("test:service2");
  connection2_->SetRemoteServiceProviderConnectionErrorHandler(
      base::Bind(&TestApplication::ConnectionClosed,
                  base::Unretained(this), "test:service2"));
  return true;
}

void TestApplication::ConnectionClosed(const std::string& service_url) {
  validator_->ConnectionClosed(url_, service_url);
}

////////////////////////////////////////////////////////////////////////////////
// TestLoader:

TestLoader::TestLoader(ShellClient* delegate) : delegate_(delegate) {}
TestLoader::~TestLoader() {}

void TestLoader::Load(const GURL& url,
                      InterfaceRequest<mojom::ShellClient> request) {
  app_.reset(new ShellConnection(delegate_.get(), std::move(request)));
}

////////////////////////////////////////////////////////////////////////////////
// CapabilityFilterTest:

CapabilityFilterTest::CapabilityFilterTest() : validator_(nullptr) {}
CapabilityFilterTest::~CapabilityFilterTest() {}

void CapabilityFilterTest::RunBlockingTest() {
  std::set<std::string> expectations;
  expectations.insert("E test:trusted test:service mojo::shell::Safe");
  expectations.insert("E test:trusted test:service mojo::shell::Unsafe");
  expectations.insert("E test:trusted test:service2 mojo::shell::Safe");
  expectations.insert("E test:trusted test:service2 mojo::shell::Unsafe");
  expectations.insert("E test:untrusted test:service mojo::shell::Safe");
  expectations.insert("B test:untrusted test:service mojo::shell::Unsafe");
  expectations.insert("C test:untrusted test:service2");
  InitValidator(expectations);

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

  RunTest();
}

void CapabilityFilterTest::RunWildcardTest() {
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
}


void CapabilityFilterTest::SetUp() {
  application_manager_.reset(
      new ApplicationManager(make_scoped_ptr(CreatePackageManager())));
  CreateLoader<ServiceApplication>("test:service");
  CreateLoader<ServiceApplication>("test:service2");
}

void CapabilityFilterTest::TearDown() {
  application_manager_.reset();
}

class ServiceProviderImpl : public ServiceProvider {
 public:
  explicit ServiceProviderImpl(
      InterfaceRequest<ServiceProvider> service_provider,
      InterfaceFactory<Validator>* factory)
      : binding_(this, std::move(service_provider)),
        factory_(factory) {}
  ~ServiceProviderImpl() override {}

 private:
  // ServiceProvider method.
  void ConnectToService(const mojo::String& service_name,
                        ScopedMessagePipeHandle client_handle) override {
    if (service_name == Validator::Name_) {
      factory_->Create(nullptr,
                       MakeRequest<Validator>(std::move(client_handle)));
    }
  }

  Binding<ServiceProvider> binding_;
  InterfaceFactory<Validator>* factory_;

  DISALLOW_COPY_AND_ASSIGN(ServiceProviderImpl);
};

void CapabilityFilterTest::RunApplication(const std::string& url,
                                          const CapabilityFilter& filter) {
  ServiceProviderPtr services;

  // We expose Validator to the test application via ConnectToApplication
  // because we don't allow the test application to connect to test:validator.
  // Adding it to the CapabilityFilter would interfere with the test.
  ServiceProviderPtr exposed_services;
  new ServiceProviderImpl(GetProxy(&exposed_services), validator_);
  scoped_ptr<ConnectToApplicationParams> params(
      new ConnectToApplicationParams);
  params->SetTarget(Identity(GURL(url), std::string(), filter));
  params->set_services(GetProxy(&services));
  params->set_exposed_services(std::move(exposed_services));
  params->set_on_application_end(base::MessageLoop::QuitWhenIdleClosure());
  application_manager_->ConnectToApplication(std::move(params));
}

void CapabilityFilterTest::InitValidator(
    const std::set<std::string>& expectations) {
  validator_ = new ConnectionValidator(expectations, &loop_);
  application_manager()->SetLoaderForURL(make_scoped_ptr(validator_),
                                          GURL("test:validator"));
}

void CapabilityFilterTest::RunTest() {
  loop()->Run();
  EXPECT_TRUE(validator_->expectations_met());
  if (!validator_->expectations_met())
    validator_->PrintUnmetExpectations();
}

}  // namespace test
}  // namespace shell
}  // namespace mojo
