// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/proxy_config_service_impl.h"

#include <vector>

#include "base/format_macros.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/prefs/testing_pref_service.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/chromeos/settings/device_settings_service.h"
#include "chrome/browser/chromeos/ui_proxy_config.h"
#include "chrome/common/pref_names.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/shill_profile_client.h"
#include "chromeos/dbus/shill_service_client.h"
#include "chromeos/network/network_handler.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "content/public/test/test_browser_thread.h"
#include "net/proxy/proxy_config_service_common_unittest.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

using content::BrowserThread;

namespace chromeos {

namespace {

struct Input {
  UIProxyConfig::Mode mode;
  std::string pac_url;
  std::string server;
  std::string bypass_rules;
};

// Builds an identifier for each test in an array.
#define TEST_DESC(desc) base::StringPrintf("at line %d <%s>", __LINE__, desc)

// Shortcuts to declare enums within chromeos's ProxyConfig.
#define MK_MODE(mode) UIProxyConfig::MODE_##mode

// Inspired from net/proxy/proxy_config_service_linux_unittest.cc.
const struct TestParams {
  // Short description to identify the test
  std::string description;

  Input input;

  // Expected outputs from fields of net::ProxyConfig (via IO).
  bool auto_detect;
  GURL pac_url;
  net::ProxyRulesExpectation proxy_rules;
} tests[] = {
  {  // 0
    TEST_DESC("No proxying"),

    { // Input.
      MK_MODE(DIRECT),  // mode
    },

    // Expected result.
    false,                                   // auto_detect
    GURL(),                                  // pac_url
    net::ProxyRulesExpectation::Empty(),     // proxy_rules
  },

  {  // 1
    TEST_DESC("Auto detect"),

    { // Input.
      MK_MODE(AUTO_DETECT),  // mode
    },

    // Expected result.
    true,                                    // auto_detect
    GURL(),                                  // pac_url
    net::ProxyRulesExpectation::Empty(),     // proxy_rules
  },

  {  // 2
    TEST_DESC("Valid PAC URL"),

    { // Input.
      MK_MODE(PAC_SCRIPT),     // mode
      "http://wpad/wpad.dat",  // pac_url
    },

    // Expected result.
    false,                                   // auto_detect
    GURL("http://wpad/wpad.dat"),            // pac_url
    net::ProxyRulesExpectation::Empty(),     // proxy_rules
  },

  {  // 3
    TEST_DESC("Invalid PAC URL"),

    { // Input.
      MK_MODE(PAC_SCRIPT),  // mode
      "wpad.dat",           // pac_url
    },

    // Expected result.
    false,                                   // auto_detect
    GURL(),                                  // pac_url
    net::ProxyRulesExpectation::Empty(),     // proxy_rules
  },

  {  // 4
    TEST_DESC("Single-host in proxy list"),

    { // Input.
      MK_MODE(SINGLE_PROXY),  // mode
      "",                     // pac_url
      "www.google.com",       // server
    },

    // Expected result.
    false,                                   // auto_detect
    GURL(),                                  // pac_url
    net::ProxyRulesExpectation::Single(      // proxy_rules
        "www.google.com:80",                 // single proxy
        "<local>"),                          // bypass rules
  },

  {  // 5
    TEST_DESC("Single-host, different port"),

    { // Input.
      MK_MODE(SINGLE_PROXY),  // mode
      "",                     // pac_url
      "www.google.com:99",    // server
    },

    // Expected result.
    false,                                   // auto_detect
    GURL(),                                  // pac_url
    net::ProxyRulesExpectation::Single(      // proxy_rules
        "www.google.com:99",                 // single
        "<local>"),                          // bypass rules
  },

  {  // 6
    TEST_DESC("Tolerate a scheme"),

    { // Input.
      MK_MODE(SINGLE_PROXY),       // mode
      "",                          // pac_url
      "http://www.google.com:99",  // server
    },

    // Expected result.
    false,                                   // auto_detect
    GURL(),                                  // pac_url
    net::ProxyRulesExpectation::Single(      // proxy_rules
        "www.google.com:99",                 // single proxy
        "<local>"),                          // bypass rules
  },

  {  // 7
    TEST_DESC("Per-scheme proxy rules"),

    { // Input.
      MK_MODE(PROXY_PER_SCHEME),  // mode
      "",                         // pac_url
      "http=www.google.com:80;https=https://www.foo.com:110;"
      "ftp=ftp.foo.com:121;socks=socks5://socks.com:888",  // server
    },

    // Expected result.
    false,                          // auto_detect
    GURL(),                         // pac_url
    net::ProxyRulesExpectation::PerSchemeWithSocks(  // proxy_rules
        "www.google.com:80",        // http
        "https://www.foo.com:110",  // https
        "ftp.foo.com:121",          // ftp
        "socks5://socks.com:888",   // fallback proxy
        "<local>"),                 // bypass rules
  },

  {  // 8
    TEST_DESC("Bypass rules"),

    { // Input.
      MK_MODE(SINGLE_PROXY),      // mode
      "",                         // pac_url
      "www.google.com",           // server
      "*.google.com, *foo.com:99, 1.2.3.4:22, 127.0.0.1/8",  // bypass_rules
    },

    // Expected result.
    false,                          // auto_detect
    GURL(),                         // pac_url
    net::ProxyRulesExpectation::Single(  // proxy_rules
        "www.google.com:80",             // single proxy
        // bypass_rules
        "*.google.com,*foo.com:99,1.2.3.4:22,127.0.0.1/8,<local>"),
  },
};  // tests

const char* kUserProfilePath = "user_profile";

}  // namespace

class ProxyConfigServiceImplTest : public testing::Test {
 protected:
  ProxyConfigServiceImplTest()
      : ui_thread_(BrowserThread::UI, &loop_),
        io_thread_(BrowserThread::IO, &loop_) {}

  virtual void SetUp() {
    DBusThreadManager::InitializeWithStub();
    NetworkHandler::Initialize();

    SetUpNetwork();

    PrefProxyConfigTrackerImpl::RegisterPrefs(pref_service_.registry());

    // Create a ProxyConfigServiceImpl like for the system request context.
    config_service_impl_.reset(
        new ProxyConfigServiceImpl(NULL,  // no profile prefs
                                   &pref_service_));
    proxy_config_service_ =
        config_service_impl_->CreateTrackingProxyConfigService(
            scoped_ptr<net::ProxyConfigService>());

    // CreateTrackingProxyConfigService triggers update of initial prefs proxy
    // config by tracker to chrome proxy config service, so flush all pending
    // tasks so that tests start fresh.
    loop_.RunUntilIdle();
  }

  void SetUpNetwork() {
    ShillProfileClient::TestInterface* profile_test =
        DBusThreadManager::Get()->GetShillProfileClient()->GetTestInterface();
    ShillServiceClient::TestInterface* service_test =
        DBusThreadManager::Get()->GetShillServiceClient()->GetTestInterface();

    // Process any pending notifications before clearing services.
    loop_.RunUntilIdle();
    service_test->ClearServices();

    // Sends a notification about the added profile.
    profile_test->AddProfile(kUserProfilePath, "user_hash");

    service_test->AddService("/service/stub_wifi2",
                             "stub_wifi2" /* guid */,
                             "wifi2_PSK",
                             shill::kTypeWifi, shill::kStateOnline,
                             true /* visible */);
    profile_test->AddService(kUserProfilePath, "/service/stub_wifi2");

    loop_.RunUntilIdle();
  }

  virtual void TearDown() {
    config_service_impl_->DetachFromPrefService();
    loop_.RunUntilIdle();
    config_service_impl_.reset();
    proxy_config_service_.reset();
    NetworkHandler::Shutdown();
    DBusThreadManager::Shutdown();
  }

  void InitConfigWithTestInput(const Input& input,
                               base::DictionaryValue* result) {
    base::DictionaryValue* new_config = NULL;
    switch (input.mode) {
      case MK_MODE(DIRECT):
        new_config = ProxyConfigDictionary::CreateDirect();
        break;
      case MK_MODE(AUTO_DETECT):
        new_config = ProxyConfigDictionary::CreateAutoDetect();
        break;
      case MK_MODE(PAC_SCRIPT):
        new_config =
            ProxyConfigDictionary::CreatePacScript(input.pac_url, false);
        break;
      case MK_MODE(SINGLE_PROXY):
      case MK_MODE(PROXY_PER_SCHEME):
        new_config =
          ProxyConfigDictionary::CreateFixedServers(input.server,
                                                    input.bypass_rules);
        break;
    }
    result->Swap(new_config);
    delete new_config;
  }

  void SetConfig(base::DictionaryValue* pref_proxy_config_dict) {
    std::string proxy_config;
    if (pref_proxy_config_dict)
      base::JSONWriter::Write(pref_proxy_config_dict, &proxy_config);

    NetworkStateHandler* network_state_handler =
        NetworkHandler::Get()->network_state_handler();
    const NetworkState* network = network_state_handler->DefaultNetwork();
    ASSERT_TRUE(network);
    DBusThreadManager::Get()->GetShillServiceClient()->GetTestInterface()->
        SetServiceProperty(network->path(),
                           shill::kProxyConfigProperty,
                           base::StringValue(proxy_config));
  }

  // Synchronously gets the latest proxy config.
  void SyncGetLatestProxyConfig(net::ProxyConfig* config) {
    *config = net::ProxyConfig();
    // Let message loop process all messages. This will run
    // ChromeProxyConfigService::UpdateProxyConfig, which is posted on IO from
    // PrefProxyConfigTrackerImpl::OnProxyConfigChanged.
    loop_.RunUntilIdle();
    net::ProxyConfigService::ConfigAvailability availability =
        proxy_config_service_->GetLatestProxyConfig(config);

    EXPECT_EQ(net::ProxyConfigService::CONFIG_VALID, availability);
  }

  base::MessageLoop loop_;
  scoped_ptr<net::ProxyConfigService> proxy_config_service_;
  scoped_ptr<ProxyConfigServiceImpl> config_service_impl_;
  TestingPrefServiceSimple pref_service_;

 private:
  ScopedTestDeviceSettingsService test_device_settings_service_;
  ScopedTestCrosSettings test_cros_settings_;
  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread io_thread_;
};

TEST_F(ProxyConfigServiceImplTest, NetworkProxy) {
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(tests); ++i) {
    SCOPED_TRACE(base::StringPrintf("Test[%" PRIuS "] %s", i,
                                    tests[i].description.c_str()));

    base::DictionaryValue test_config;
    InitConfigWithTestInput(tests[i].input, &test_config);
    SetConfig(&test_config);

    net::ProxyConfig config;
    SyncGetLatestProxyConfig(&config);

    EXPECT_EQ(tests[i].auto_detect, config.auto_detect());
    EXPECT_EQ(tests[i].pac_url, config.pac_url());
    EXPECT_TRUE(tests[i].proxy_rules.Matches(config.proxy_rules()));
  }
}

TEST_F(ProxyConfigServiceImplTest, DynamicPrefsOverride) {
  // Groupings of 3 test inputs to use for managed, recommended and network
  // proxies respectively.  Only valid and non-direct test inputs are used.
  const size_t proxies[][3] = {
    { 1, 2, 4, },
    { 1, 4, 2, },
    { 4, 2, 1, },
    { 2, 1, 4, },
    { 2, 4, 5, },
    { 2, 5, 4, },
    { 5, 4, 2, },
    { 4, 2, 5, },
    { 4, 5, 6, },
    { 4, 6, 5, },
    { 6, 5, 4, },
    { 5, 4, 6, },
    { 5, 6, 7, },
    { 5, 7, 6, },
    { 7, 6, 5, },
    { 6, 5, 7, },
    { 6, 7, 8, },
    { 6, 8, 7, },
    { 8, 7, 6, },
    { 7, 6, 8, },
  };
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(proxies); ++i) {
    const TestParams& managed_params = tests[proxies[i][0]];
    const TestParams& recommended_params = tests[proxies[i][1]];
    const TestParams& network_params = tests[proxies[i][2]];

    SCOPED_TRACE(base::StringPrintf(
        "Test[%" PRIuS "] managed=[%s], recommended=[%s], network=[%s]", i,
        managed_params.description.c_str(),
        recommended_params.description.c_str(),
        network_params.description.c_str()));

    base::DictionaryValue managed_config;
    InitConfigWithTestInput(managed_params.input, &managed_config);
    base::DictionaryValue recommended_config;
    InitConfigWithTestInput(recommended_params.input, &recommended_config);
    base::DictionaryValue network_config;
    InitConfigWithTestInput(network_params.input, &network_config);

    // Managed proxy pref should take effect over recommended proxy and
    // non-existent network proxy.
    SetConfig(NULL);
    pref_service_.SetManagedPref(prefs::kProxy, managed_config.DeepCopy());
    pref_service_.SetRecommendedPref(prefs::kProxy,
                                     recommended_config.DeepCopy());
    net::ProxyConfig actual_config;
    SyncGetLatestProxyConfig(&actual_config);
    EXPECT_EQ(managed_params.auto_detect, actual_config.auto_detect());
    EXPECT_EQ(managed_params.pac_url, actual_config.pac_url());
    EXPECT_TRUE(managed_params.proxy_rules.Matches(
        actual_config.proxy_rules()));

    // Recommended proxy pref should take effect when managed proxy pref is
    // removed.
    pref_service_.RemoveManagedPref(prefs::kProxy);
    SyncGetLatestProxyConfig(&actual_config);
    EXPECT_EQ(recommended_params.auto_detect, actual_config.auto_detect());
    EXPECT_EQ(recommended_params.pac_url, actual_config.pac_url());
    EXPECT_TRUE(recommended_params.proxy_rules.Matches(
        actual_config.proxy_rules()));

    // Network proxy should take take effect over recommended proxy pref.
    SetConfig(&network_config);
    SyncGetLatestProxyConfig(&actual_config);
    EXPECT_EQ(network_params.auto_detect, actual_config.auto_detect());
    EXPECT_EQ(network_params.pac_url, actual_config.pac_url());
    EXPECT_TRUE(network_params.proxy_rules.Matches(
        actual_config.proxy_rules()));

    // Managed proxy pref should take effect over network proxy.
    pref_service_.SetManagedPref(prefs::kProxy, managed_config.DeepCopy());
    SyncGetLatestProxyConfig(&actual_config);
    EXPECT_EQ(managed_params.auto_detect, actual_config.auto_detect());
    EXPECT_EQ(managed_params.pac_url, actual_config.pac_url());
    EXPECT_TRUE(managed_params.proxy_rules.Matches(
        actual_config.proxy_rules()));

    // Network proxy should take effect over recommended proxy pref when managed
    // proxy pref is removed.
    pref_service_.RemoveManagedPref(prefs::kProxy);
    SyncGetLatestProxyConfig(&actual_config);
    EXPECT_EQ(network_params.auto_detect, actual_config.auto_detect());
    EXPECT_EQ(network_params.pac_url, actual_config.pac_url());
    EXPECT_TRUE(network_params.proxy_rules.Matches(
        actual_config.proxy_rules()));

    // Removing recommended proxy pref should have no effect on network proxy.
    pref_service_.RemoveRecommendedPref(prefs::kProxy);
    SyncGetLatestProxyConfig(&actual_config);
    EXPECT_EQ(network_params.auto_detect, actual_config.auto_detect());
    EXPECT_EQ(network_params.pac_url, actual_config.pac_url());
    EXPECT_TRUE(network_params.proxy_rules.Matches(
        actual_config.proxy_rules()));
  }
}

}  // namespace chromeos
