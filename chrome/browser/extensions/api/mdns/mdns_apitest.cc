// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/command_line.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/api/mdns/mdns_api.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/media/router/test/mock_dns_sd_registry.h"
#include "chrome/common/extensions/api/mdns.h"
#include "content/public/test/browser_test.h"
#include "extensions/common/switches.h"
#include "extensions/test/result_catcher.h"
#include "testing/gmock/include/gmock/gmock.h"

using ::testing::_;
using ::testing::A;
using media_router::DnsSdRegistry;

namespace api = extensions::api;

namespace {

class MDnsAPITest : public extensions::ExtensionApiTest {
 public:
  MDnsAPITest() {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    extensions::ExtensionApiTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(
        extensions::switches::kWhitelistedExtensionID,
        "ddchlicdkolnonkihahngkmmmjnjlkkf");
  }

  void SetUpTestDnsSdRegistry() {
    extensions::MDnsAPI* api = extensions::MDnsAPI::Get(profile());
    dns_sd_registry_ = std::make_unique<media_router::MockDnsSdRegistry>(api);
    EXPECT_CALL(*dns_sd_registry_, AddObserver(api))
        .Times(1);
    api->SetDnsSdRegistryForTesting(dns_sd_registry_.get());
  }

 protected:
  std::unique_ptr<media_router::MockDnsSdRegistry> dns_sd_registry_;
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
              RemoveObserver(A<DnsSdRegistry::DnsSdObserver*>()))
      .Times(1);

  EXPECT_TRUE(RunExtensionSubtest("mdns/api", "register_listener.html"))
      << message_;

  extensions::ResultCatcher catcher;
  // Dispatch 3 events, one of which should not be sent to the test extension.
  DnsSdRegistry::DnsSdServiceList services;

  media_router::DnsSdService service;
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
#define MAYBE_ForceDiscovery DISABLED_ForceDiscovery
#else
#define MAYBE_ForceDiscovery ForceDiscovery
#endif
// Test loading extension, registering an MDNS listener and dispatching events.
IN_PROC_BROWSER_TEST_F(MDnsAPITest, MAYBE_ForceDiscovery) {
  const std::string& service_type = "_googlecast._tcp.local";
  SetUpTestDnsSdRegistry();

  EXPECT_CALL(*dns_sd_registry_, RegisterDnsSdListener(service_type)).Times(1);
  EXPECT_CALL(*dns_sd_registry_, UnregisterDnsSdListener(service_type))
      .Times(1);
  EXPECT_CALL(*dns_sd_registry_, ResetAndDiscover()).Times(1);
  EXPECT_CALL(*dns_sd_registry_,
              RemoveObserver(A<DnsSdRegistry::DnsSdObserver*>()))
      .Times(1);

  EXPECT_TRUE(RunExtensionSubtest("mdns/api", "force_discovery.html"))
      << message_;

  extensions::ResultCatcher catcher;
  DnsSdRegistry::DnsSdServiceList services;

  media_router::DnsSdService service;
  service.service_name = service_type;
  services.push_back(service);

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
              RemoveObserver(A<DnsSdRegistry::DnsSdObserver*>()))
      .Times(1);

  EXPECT_TRUE(RunExtensionSubtest("mdns/api",
                                  "register_multiple_listeners.html"))
      << message_;

  extensions::ResultCatcher catcher;
  DnsSdRegistry::DnsSdServiceList services;

  media_router::DnsSdService service;
  service.service_name = service_type;
  services.push_back(service);

  dns_sd_registry_->DispatchMDnsEvent(service_type, services);
  dns_sd_registry_->DispatchMDnsEvent(test_service_type, services);
  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
}

// TODO(justinlin): Win Dbg has a workaround that makes RunExtensionSubtest
// always return true without actually running the test. Remove when fixed.
#if defined(OS_WIN) && !defined(NDEBUG)
#define MAYBE_RegisterTooManyListeners DISABLED_RegisterTooManyListeners
#else
#define MAYBE_RegisterTooManyListeners RegisterTooManyListeners
#endif
// Test loading extension and registering multiple listeners.
IN_PROC_BROWSER_TEST_F(MDnsAPITest, MAYBE_RegisterTooManyListeners) {
  SetUpTestDnsSdRegistry();

  EXPECT_CALL(*dns_sd_registry_, RegisterDnsSdListener(_)).Times(10);
  EXPECT_CALL(*dns_sd_registry_, UnregisterDnsSdListener(_)).Times(10);
  EXPECT_CALL(*dns_sd_registry_,
              RemoveObserver(A<DnsSdRegistry::DnsSdObserver*>()))
      .Times(1);

  EXPECT_TRUE(RunPlatformAppTest("mdns/api-packaged-apps"))
      << message_;
}

// TODO(justinlin): Win Dbg has a workaround that makes RunExtensionSubtest
// always return true without actually running the test. Remove when fixed.
#if defined(OS_WIN) && !defined(NDEBUG)
#define MAYBE_MaxServiceInstancesPerEventConst \
  DISABLED_MaxServiceInstancesPerEventConst
#else
#define MAYBE_MaxServiceInstancesPerEventConst MaxServiceInstancesPerEventConst
#endif
// Test loading extension and registering multiple listeners.
IN_PROC_BROWSER_TEST_F(MDnsAPITest, MAYBE_MaxServiceInstancesPerEventConst) {
  EXPECT_TRUE(RunExtensionSubtest("mdns/api",
                                  "get_max_service_instances.html"));
}
