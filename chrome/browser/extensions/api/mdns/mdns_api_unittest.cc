// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/mdns/mdns_api.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_service_test_base.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/common/extensions/api/mdns.h"
#include "content/public/browser/browser_context.h"
#include "content/public/test/mock_render_process_host.h"
#include "extensions/browser/extension_prefs_factory.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/manifest_constants.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

namespace {

KeyedService* MDnsAPITestingFactoryFunction(content::BrowserContext* context) {
  return new MDnsAPI(context);
}

// For ExtensionService interface when it requires a path that is not used.
base::FilePath bogus_file_pathname(const std::string& name) {
  return base::FilePath(FILE_PATH_LITERAL("//foobar_nonexistent"))
      .AppendASCII(name);
}

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

class MDnsAPITest : public extensions::ExtensionServiceTestBase {
 public:
  void SetUp() override {
    extensions::ExtensionServiceTestBase::SetUp();

    // Set up browser_context().
    InitializeEmptyExtensionService();

    // A custom TestingFactoryFunction is required for an MDnsAPI to actually be
    // constructed.
    MDnsAPI::GetFactoryInstance()->SetTestingFactory(
        browser_context(),
        MDnsAPITestingFactoryFunction);

    // Create an event router and associate it with the context.
    extensions::EventRouter* event_router = new extensions::EventRouter(
        browser_context(),
        ExtensionPrefsFactory::GetInstance()->GetForBrowserContext(
            browser_context()));
    static_cast<TestExtensionSystem*>(
        ExtensionSystem::Get(browser_context()))->SetEventRouter(
            scoped_ptr<extensions::EventRouter>(event_router));

    // Do some sanity checking
    ASSERT_EQ(event_router, EventRouter::Get(browser_context()));
    ASSERT_TRUE(MDnsAPI::Get(browser_context())); // constructs MDnsAPI

    registry_ = new MockDnsSdRegistry(MDnsAPI::Get(browser_context()));
    EXPECT_CALL(*dns_sd_registry(),
                AddObserver(MDnsAPI::Get(browser_context())))
        .Times(1);
    MDnsAPI::Get(browser_context())->SetDnsSdRegistryForTesting(
        scoped_ptr<DnsSdRegistry>(registry_));

    render_process_host_.reset(
        new content::MockRenderProcessHost(browser_context()));
  }

  void TearDown() override {
    EXPECT_CALL(*dns_sd_registry(),
                RemoveObserver(MDnsAPI::Get(browser_context())))
        .Times(1);
    render_process_host_.reset();
    extensions::ExtensionServiceTestBase::TearDown();
    MDnsAPI::GetFactoryInstance()->SetTestingFactory(
        browser_context(),
        nullptr);

    registry_ = nullptr;
  }

  virtual MockDnsSdRegistry* dns_sd_registry() {
    return registry_;
  }

  // Constructs an extension according to the parameters that matter most to
  // MDnsAPI the local unit tests.
  const scoped_refptr<extensions::Extension> CreateExtension(
      std::string name,
      bool is_platform_app,
      std::string extension_id) {
    base::DictionaryValue manifest;
    manifest.SetString(extensions::manifest_keys::kVersion, "1.0.0.0");
    manifest.SetString(extensions::manifest_keys::kName, name);
    if (is_platform_app) {
      // Setting app.background.page = "background.html" is sufficient to make
      // the extension type TYPE_PLATFORM_APP.
      manifest.Set(extensions::manifest_keys::kPlatformAppBackgroundPage,
                   new base::StringValue("background.html"));
    }

    std::string error;
    return extensions::Extension::Create(
        bogus_file_pathname(name),
        extensions::Manifest::INVALID_LOCATION,
        manifest,
        Extension::NO_FLAGS,
        extension_id,
        &error);
  }

  content::RenderProcessHost* render_process_host() const {
    return render_process_host_.get();
  }

 private:
  // The registry is owned by MDnsAPI, but MDnsAPI does not have an accessor
  // for it, so use a private member.
  MockDnsSdRegistry* registry_;

  scoped_ptr<content::RenderProcessHost> render_process_host_;

};

TEST_F(MDnsAPITest, ExtensionRespectsWhitelist) {
  const std::string ext_id("mbflcebpggnecokmikipoihdbecnjfoj");
  scoped_refptr<extensions::Extension> extension =
      CreateExtension("Dinosaur networker", false, ext_id);
  ExtensionRegistry::Get(browser_context())->AddEnabled(extension);
  ASSERT_EQ(Manifest::TYPE_EXTENSION, extension.get()->GetType());

  // There is a whitelist of mdns service types extensions may access, which
  // includes "_testing._tcp.local" and exludes "_trex._tcp.local"
  {
    base::DictionaryValue filter;
    filter.SetString(kEventFilterServiceTypeKey, "_trex._tcp.local");

    ASSERT_TRUE(dns_sd_registry());
    // Test that the extension is able to listen to a non-whitelisted service
    EXPECT_CALL(*dns_sd_registry(), RegisterDnsSdListener("_trex._tcp.local"))
        .Times(0);
    EventRouter::Get(browser_context())->AddFilteredEventListener(
        api::mdns::OnServiceList::kEventName, render_process_host(), ext_id,
        filter, false);

    EXPECT_CALL(*dns_sd_registry(), UnregisterDnsSdListener("_trex._tcp.local"))
        .Times(0);
    EventRouter::Get(browser_context())->RemoveFilteredEventListener(
        api::mdns::OnServiceList::kEventName, render_process_host(), ext_id,
        filter, false);
  }
  {
    base::DictionaryValue filter;
    filter.SetString(kEventFilterServiceTypeKey, "_testing._tcp.local");

    ASSERT_TRUE(dns_sd_registry());
    // Test that the extension is able to listen to a whitelisted service
    EXPECT_CALL(*dns_sd_registry(),
                RegisterDnsSdListener("_testing._tcp.local"));
    EventRouter::Get(browser_context())->AddFilteredEventListener(
        api::mdns::OnServiceList::kEventName, render_process_host(), ext_id,
        filter, false);

    EXPECT_CALL(*dns_sd_registry(),
                UnregisterDnsSdListener("_testing._tcp.local"));
    EventRouter::Get(browser_context())->RemoveFilteredEventListener(
        api::mdns::OnServiceList::kEventName, render_process_host(),
        ext_id, filter, false);
  }
}

TEST_F(MDnsAPITest, PlatformAppsNotSubjectToWhitelist) {
  const std::string ext_id("mbflcebpggnecokmikipoihdbecnjfoj");
  scoped_refptr<extensions::Extension> extension =
      CreateExtension("Dinosaur networker", true, ext_id);
  ExtensionRegistry::Get(browser_context())->AddEnabled(extension);
  ASSERT_TRUE(extension.get()->is_platform_app());

  base::DictionaryValue filter;
  filter.SetString(kEventFilterServiceTypeKey, "_trex._tcp.local");

  ASSERT_TRUE(dns_sd_registry());
  // Test that the extension is able to listen to a non-whitelisted service
  EXPECT_CALL(*dns_sd_registry(), RegisterDnsSdListener("_trex._tcp.local"));
  EventRouter::Get(browser_context())->AddFilteredEventListener(
      api::mdns::OnServiceList::kEventName, render_process_host(), ext_id,
      filter, false);

  EXPECT_CALL(*dns_sd_registry(), UnregisterDnsSdListener("_trex._tcp.local"));
  EventRouter::Get(browser_context())->RemoveFilteredEventListener(
      api::mdns::OnServiceList::kEventName, render_process_host(), ext_id,
      filter, false);
}

} // empty namespace

} // namespace extensions
