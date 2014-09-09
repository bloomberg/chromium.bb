// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "chrome/browser/extensions/api/mdns/mdns_api.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/common/extensions/api/mdns.h"
#include "extensions/common/switches.h"
#include "extensions/test/result_catcher.h"
#include "testing/gmock/include/gmock/gmock.h"

using extensions::DnsSdRegistry;
using ::testing::A;
using ::testing::_;

namespace api = extensions::api;

namespace {

class MockDnsSdRegistry : public DnsSdRegistry {
 public:
  explicit MockDnsSdRegistry(extensions::MDnsAPI* api) : api_(api) {}
  virtual ~MockDnsSdRegistry() {}

  MOCK_METHOD1(AddObserver, void(DnsSdObserver* observer));
  MOCK_METHOD1(RemoveObserver, void(DnsSdObserver* observer));
  MOCK_METHOD1(RegisterDnsSdListener, void(std::string service_type));
  MOCK_METHOD1(UnregisterDnsSdListener, void(std::string service_type));

  void DispatchMDnsEvent(const std::string& service_type,
                         const DnsSdServiceList& services) {
    api_->OnDnsSdEvent(service_type, services);
  }

 private:
  extensions::DnsSdRegistry::DnsSdObserver* api_;
};

class MDnsAPITest : public ExtensionApiTest {
 public:
  MDnsAPITest() {}

  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    ExtensionApiTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(
        extensions::switches::kWhitelistedExtensionID,
        "ddchlicdkolnonkihahngkmmmjnjlkkf");
  }

  void SetUpTestDnsSdRegistry() {
    extensions::MDnsAPI* api = extensions::MDnsAPI::Get(profile());
    dns_sd_registry_ = new MockDnsSdRegistry(api);
    // Transfers ownership of the registry instance.
    api->SetDnsSdRegistryForTesting(
        make_scoped_ptr<DnsSdRegistry>(dns_sd_registry_).Pass());
  }

 protected:
  MockDnsSdRegistry* dns_sd_registry_;
};

}  // namespace

// TODO(justinlin): Win Dbg has a workaround that makes RunExtensionSubtest
// always return true without actually running the test. Remove when fixed.
#if defined(OS_WIN) && !defined(NDEBUG)
#define MAYBE_RegisterListener DISABLED_RegisterListener
#else
#define MAYBE_RegisterListener RegisterListener
#endif
// Test loading extension, registering an MDNS listener and dispatching events.
IN_PROC_BROWSER_TEST_F(MDnsAPITest, MAYBE_RegisterListener) {
  const std::string& service_type = "_googlecast._tcp.local";
  SetUpTestDnsSdRegistry();

  EXPECT_CALL(*dns_sd_registry_, RegisterDnsSdListener(service_type))
      .Times(1);
  EXPECT_CALL(*dns_sd_registry_, UnregisterDnsSdListener(service_type))
      .Times(1);
  EXPECT_CALL(*dns_sd_registry_,
              RemoveObserver(A<extensions::DnsSdRegistry::DnsSdObserver*>()))
      .Times(1);

  EXPECT_TRUE(RunExtensionSubtest("mdns/api", "register_listener.html"))
      << message_;

  extensions::ResultCatcher catcher;
  // Dispatch 3 events, one of which should not be sent to the test extension.
  DnsSdRegistry::DnsSdServiceList services;

  extensions::DnsSdService service;
  service.service_name = service_type;
  services.push_back(service);

  dns_sd_registry_->DispatchMDnsEvent(service_type, services);
  dns_sd_registry_->DispatchMDnsEvent("_uninteresting._tcp.local", services);
  dns_sd_registry_->DispatchMDnsEvent(service_type, services);
  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
}

// TODO(justinlin): Win Dbg has a workaround that makes RunExtensionSubtest
// always return true without actually running the test. Remove when fixed.
#if defined(OS_WIN) && !defined(NDEBUG)
#define MAYBE_RegisterMultipleListeners DISABLED_RegisterMultipleListeners
#else
#define MAYBE_RegisterMultipleListeners RegisterMultipleListeners
#endif
// Test loading extension and registering multiple listeners.
IN_PROC_BROWSER_TEST_F(MDnsAPITest, MAYBE_RegisterMultipleListeners) {
  const std::string& service_type = "_googlecast._tcp.local";
  const std::string& test_service_type = "_testing._tcp.local";
  SetUpTestDnsSdRegistry();

  EXPECT_CALL(*dns_sd_registry_, RegisterDnsSdListener(service_type))
      .Times(1);
  EXPECT_CALL(*dns_sd_registry_, UnregisterDnsSdListener(service_type))
      .Times(1);
  EXPECT_CALL(*dns_sd_registry_, RegisterDnsSdListener(test_service_type))
      .Times(1);
  EXPECT_CALL(*dns_sd_registry_, UnregisterDnsSdListener(test_service_type))
      .Times(1);
  EXPECT_CALL(*dns_sd_registry_,
              RemoveObserver(A<extensions::DnsSdRegistry::DnsSdObserver*>()))
      .Times(1);

  EXPECT_TRUE(RunExtensionSubtest("mdns/api",
                                  "register_multiple_listeners.html"))
      << message_;

  extensions::ResultCatcher catcher;
  DnsSdRegistry::DnsSdServiceList services;

  extensions::DnsSdService service;
  service.service_name = service_type;
  services.push_back(service);

  dns_sd_registry_->DispatchMDnsEvent(service_type, services);
  dns_sd_registry_->DispatchMDnsEvent(test_service_type, services);
  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
}
