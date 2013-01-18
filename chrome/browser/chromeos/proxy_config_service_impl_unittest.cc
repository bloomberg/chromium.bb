// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/proxy_config_service_impl.h"

#include <map>
#include <string>
#include <vector>

#include "base/format_macros.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_pref_service.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "content/public/test/test_browser_thread.h"
#include "net/proxy/proxy_config_service_common_unittest.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::BrowserThread;

namespace chromeos {

namespace {

struct Input {  // Fields of chromeos::ProxyConfigServiceImpl::ProxyConfig.
  ProxyConfigServiceImpl::ProxyConfig::Mode mode;
  const char* pac_url;
  const char* single_uri;
  const char* http_uri;
  const char* https_uri;
  const char* ftp_uri;
  const char* socks_uri;
  const char* bypass_rules;
};

// Builds an identifier for each test in an array.
#define TEST_DESC(desc) base::StringPrintf("at line %d <%s>", __LINE__, desc)

// Shortcuts to declare enums within chromeos's ProxyConfig.
#define MK_MODE(mode) ProxyConfigServiceImpl::ProxyConfig::MODE_##mode
#define MK_SCHM(scheme) net::ProxyServer::SCHEME_##scheme
#define MK_AVAIL(avail) net::ProxyConfigService::CONFIG_##avail

// Inspired from net/proxy/proxy_config_service_linux_unittest.cc.
const struct TestParams {
  // Short description to identify the test
  std::string description;

  bool is_valid;

  Input input;

  // Expected outputs from fields of net::ProxyConfig (via IO).
  bool auto_detect;
  GURL pac_url;
  net::ProxyRulesExpectation proxy_rules;
} tests[] = {
  {  // 0
    TEST_DESC("No proxying"),

    true,  // is_valid

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

    true,  // is_valid

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

    true,  // is_valid

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

    false,  // is_valid

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

    true,  // is_valid

    { // Input.
      MK_MODE(SINGLE_PROXY),  // mode
      NULL,                   // pac_url
      "www.google.com",       // single_uri
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

    true,   // is_valid

    { // Input.
      MK_MODE(SINGLE_PROXY),  // mode
      NULL,                   // pac_url
      "www.google.com:99",    // single_uri
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

    true,   // is_valid

    { // Input.
      MK_MODE(SINGLE_PROXY),       // mode
      NULL,                        // pac_url
      "http://www.google.com:99",  // single_uri
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

    true,  // is_valid

    { // Input.
      MK_MODE(PROXY_PER_SCHEME),  // mode
      NULL,                       // pac_url
      NULL,                       // single_uri
      "www.google.com:80",        // http_uri
      "www.foo.com:110",          // https_uri
      "ftp.foo.com:121",          // ftp_uri
      "socks.com:888",            // socks_uri
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

    true,  // is_valid

    { // Input.
      MK_MODE(SINGLE_PROXY),      // mode
      NULL,                       // pac_url
      "www.google.com",           // single_uri
      NULL, NULL, NULL, NULL,     // per-proto
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

template<typename TESTBASE>
class ProxyConfigServiceImplTestBase : public TESTBASE {
 protected:
  ProxyConfigServiceImplTestBase()
      : ui_thread_(BrowserThread::UI, &loop_),
        io_thread_(BrowserThread::IO, &loop_) {}

  virtual void Init(PrefServiceSimple* pref_service) {
    ASSERT_TRUE(pref_service);
    DBusThreadManager::Initialize();
    PrefProxyConfigTrackerImpl::RegisterPrefs(pref_service);
    ProxyConfigServiceImpl::RegisterPrefs(pref_service);
    proxy_config_service_.reset(new ChromeProxyConfigService(NULL));
    config_service_impl_.reset(new ProxyConfigServiceImpl(pref_service));
    config_service_impl_->SetChromeProxyConfigService(
        proxy_config_service_.get());
    // SetChromeProxyConfigService triggers update of initial prefs proxy
    // config by tracker to chrome proxy config service, so flush all pending
    // tasks so that tests start fresh.
    loop_.RunUntilIdle();
  }

  virtual void TearDown() {
    config_service_impl_->DetachFromPrefService();
    loop_.RunUntilIdle();
    config_service_impl_.reset();
    proxy_config_service_.reset();
    DBusThreadManager::Shutdown();
  }

  void SetAutomaticProxy(
      ProxyConfigServiceImpl::ProxyConfig::Mode mode,
      const char* pac_url,
      ProxyConfigServiceImpl::ProxyConfig* config,
      ProxyConfigServiceImpl::ProxyConfig::AutomaticProxy* automatic_proxy) {
    config->mode = mode;
    config->state = ProxyPrefs::CONFIG_SYSTEM;
    if (pac_url)
      automatic_proxy->pac_url = GURL(pac_url);
  }

  void SetManualProxy(
      ProxyConfigServiceImpl::ProxyConfig::Mode mode,
      const char* server_uri,
      net::ProxyServer::Scheme scheme,
      ProxyConfigServiceImpl::ProxyConfig* config,
      ProxyConfigServiceImpl::ProxyConfig::ManualProxy* manual_proxy) {
    if (!server_uri)
      return;
    config->mode = mode;
    config->state = ProxyPrefs::CONFIG_SYSTEM;
    manual_proxy->server = net::ProxyServer::FromURI(server_uri, scheme);
  }

  void InitConfigWithTestInput(
      const Input& input, ProxyConfigServiceImpl::ProxyConfig* test_config) {
    switch (input.mode) {
      case MK_MODE(DIRECT):
      case MK_MODE(AUTO_DETECT):
      case MK_MODE(PAC_SCRIPT):
        SetAutomaticProxy(input.mode, input.pac_url, test_config,
                          &test_config->automatic_proxy);
        return;
      case MK_MODE(SINGLE_PROXY):
        SetManualProxy(input.mode, input.single_uri, MK_SCHM(HTTP),
                       test_config, &test_config->single_proxy);
        break;
      case MK_MODE(PROXY_PER_SCHEME):
        SetManualProxy(input.mode, input.http_uri, MK_SCHM(HTTP),
                       test_config, &test_config->http_proxy);
        SetManualProxy(input.mode, input.https_uri, MK_SCHM(HTTPS),
                       test_config, &test_config->https_proxy);
        SetManualProxy(input.mode, input.ftp_uri, MK_SCHM(HTTP),
                       test_config, &test_config->ftp_proxy);
        SetManualProxy(input.mode, input.socks_uri, MK_SCHM(SOCKS5),
                       test_config, &test_config->socks_proxy);
        break;
    }
    if (input.bypass_rules)
      test_config->bypass_rules.ParseFromString(input.bypass_rules);
  }

  // Synchronously gets the latest proxy config.
  net::ProxyConfigService::ConfigAvailability SyncGetLatestProxyConfig(
      net::ProxyConfig* config) {
    *config = net::ProxyConfig();
    // Let message loop process all messages.
    loop_.RunUntilIdle();
    // Calls ChromeProIOGetProxyConfig (which is called from
    // ProxyConfigService::GetLatestProxyConfig), running on faked IO thread.
    return proxy_config_service_->GetLatestProxyConfig(config);
  }

  MessageLoop loop_;
  scoped_ptr<ChromeProxyConfigService> proxy_config_service_;
  scoped_ptr<ProxyConfigServiceImpl> config_service_impl_;

 private:
  // Default stub state has ethernet as the active connected network and
  // PROFILE_SHARED as profile type, which this unittest expects.
  ScopedStubCrosEnabler stub_cros_enabler_;
  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread io_thread_;
};

class ProxyConfigServiceImplTest
    : public ProxyConfigServiceImplTestBase<testing::Test> {
 protected:
  virtual void SetUp() {
    Init(&pref_service_);
  }

  TestingPrefServiceSimple pref_service_;
};

TEST_F(ProxyConfigServiceImplTest, NetworkProxy) {
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(tests); ++i) {
    SCOPED_TRACE(StringPrintf("Test[%" PRIuS "] %s", i,
                              tests[i].description.c_str()));

    ProxyConfigServiceImpl::ProxyConfig test_config;
    InitConfigWithTestInput(tests[i].input, &test_config);
    config_service_impl_->SetTesting(&test_config);

    net::ProxyConfig config;
    EXPECT_EQ(MK_AVAIL(VALID), SyncGetLatestProxyConfig(&config));

    EXPECT_EQ(tests[i].auto_detect, config.auto_detect());
    EXPECT_EQ(tests[i].pac_url, config.pac_url());
    EXPECT_TRUE(tests[i].proxy_rules.Matches(config.proxy_rules()));
  }
}

TEST_F(ProxyConfigServiceImplTest, ModifyFromUI) {
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(tests); ++i) {
    SCOPED_TRACE(StringPrintf("Test[%" PRIuS "] %s", i,
                              tests[i].description.c_str()));

    // Init with direct.
    ProxyConfigServiceImpl::ProxyConfig test_config;
    SetAutomaticProxy(MK_MODE(DIRECT), NULL, &test_config,
                      &test_config.automatic_proxy);
    config_service_impl_->SetTesting(&test_config);

    // Set config to tests[i].input via UI.
    net::ProxyBypassRules bypass_rules;
    const Input& input = tests[i].input;
    switch (input.mode) {
      case MK_MODE(DIRECT) :
        config_service_impl_->UISetProxyConfigToDirect();
        break;
      case MK_MODE(AUTO_DETECT) :
        config_service_impl_->UISetProxyConfigToAutoDetect();
        break;
      case MK_MODE(PAC_SCRIPT) :
        config_service_impl_->UISetProxyConfigToPACScript(GURL(input.pac_url));
        break;
      case MK_MODE(SINGLE_PROXY) :
        config_service_impl_->UISetProxyConfigToSingleProxy(
            net::ProxyServer::FromURI(input.single_uri, MK_SCHM(HTTP)));
        if (input.bypass_rules) {
          bypass_rules.ParseFromString(input.bypass_rules);
          config_service_impl_->UISetProxyConfigBypassRules(bypass_rules);
        }
        break;
      case MK_MODE(PROXY_PER_SCHEME) :
        if (input.http_uri) {
          config_service_impl_->UISetProxyConfigToProxyPerScheme("http",
                  net::ProxyServer::FromURI(input.http_uri, MK_SCHM(HTTP)));
        }
        if (input.https_uri) {
          config_service_impl_->UISetProxyConfigToProxyPerScheme("https",
              net::ProxyServer::FromURI(input.https_uri, MK_SCHM(HTTPS)));
        }
        if (input.ftp_uri) {
          config_service_impl_->UISetProxyConfigToProxyPerScheme("ftp",
              net::ProxyServer::FromURI(input.ftp_uri, MK_SCHM(HTTP)));
        }
        if (input.socks_uri) {
          config_service_impl_->UISetProxyConfigToProxyPerScheme("socks",
              net::ProxyServer::FromURI(input.socks_uri, MK_SCHM(SOCKS5)));
        }
        if (input.bypass_rules) {
          bypass_rules.ParseFromString(input.bypass_rules);
          config_service_impl_->UISetProxyConfigBypassRules(bypass_rules);
        }
        break;
    }

    // Retrieve config from IO thread.
    net::ProxyConfig io_config;
    EXPECT_EQ(MK_AVAIL(VALID), SyncGetLatestProxyConfig(&io_config));
    EXPECT_EQ(tests[i].auto_detect, io_config.auto_detect());
    EXPECT_EQ(tests[i].pac_url, io_config.pac_url());
    EXPECT_TRUE(tests[i].proxy_rules.Matches(io_config.proxy_rules()));

    // Retrieve config from UI thread.
    ProxyConfigServiceImpl::ProxyConfig ui_config;
    config_service_impl_->UIGetProxyConfig(&ui_config);
    EXPECT_EQ(input.mode, ui_config.mode);
    if (tests[i].is_valid) {
      if (input.pac_url)
        EXPECT_EQ(GURL(input.pac_url), ui_config.automatic_proxy.pac_url);
      const net::ProxyRulesExpectation& proxy_rules = tests[i].proxy_rules;
      if (input.single_uri)
        EXPECT_EQ(proxy_rules.single_proxy,
                  ui_config.single_proxy.server.ToURI());
      if (input.http_uri)
        EXPECT_EQ(proxy_rules.proxy_for_http,
                  ui_config.http_proxy.server.ToURI());
      if (input.https_uri)
        EXPECT_EQ(proxy_rules.proxy_for_https,
                  ui_config.https_proxy.server.ToURI());
      if (input.ftp_uri)
        EXPECT_EQ(proxy_rules.proxy_for_ftp,
                  ui_config.ftp_proxy.server.ToURI());
      if (input.socks_uri) {
        EXPECT_EQ(proxy_rules.fallback_proxy,
                  ui_config.socks_proxy.server.ToURI());
      }
      if (input.bypass_rules)
        EXPECT_TRUE(bypass_rules.Equals(ui_config.bypass_rules));
    }
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

    SCOPED_TRACE(StringPrintf(
        "Test[%" PRIuS "] managed=[%s], recommended=[%s], network=[%s]", i,
        managed_params.description.c_str(),
        recommended_params.description.c_str(),
        network_params.description.c_str()));

    ProxyConfigServiceImpl::ProxyConfig managed_config;
    InitConfigWithTestInput(managed_params.input, &managed_config);
    ProxyConfigServiceImpl::ProxyConfig recommended_config;
    InitConfigWithTestInput(recommended_params.input, &recommended_config);
    ProxyConfigServiceImpl::ProxyConfig network_config;
    InitConfigWithTestInput(network_params.input, &network_config);

    // Managed proxy pref should take effect over recommended proxy and
    // non-existent network proxy.
    config_service_impl_->SetTesting(NULL);
    pref_service_.SetManagedPref(prefs::kProxy,
                                 managed_config.ToPrefProxyConfig());
    pref_service_.SetRecommendedPref(prefs::kProxy,
                                     recommended_config.ToPrefProxyConfig());
    net::ProxyConfig actual_config;
    EXPECT_EQ(MK_AVAIL(VALID), SyncGetLatestProxyConfig(&actual_config));
    EXPECT_EQ(managed_params.auto_detect, actual_config.auto_detect());
    EXPECT_EQ(managed_params.pac_url, actual_config.pac_url());
    EXPECT_TRUE(managed_params.proxy_rules.Matches(
        actual_config.proxy_rules()));

    // Recommended proxy pref should take effect when managed proxy pref is
    // removed.
    pref_service_.RemoveManagedPref(prefs::kProxy);
    EXPECT_EQ(MK_AVAIL(VALID), SyncGetLatestProxyConfig(&actual_config));
    EXPECT_EQ(recommended_params.auto_detect, actual_config.auto_detect());
    EXPECT_EQ(recommended_params.pac_url, actual_config.pac_url());
    EXPECT_TRUE(recommended_params.proxy_rules.Matches(
        actual_config.proxy_rules()));

    // Network proxy should take take effect over recommended proxy pref.
    config_service_impl_->SetTesting(&network_config);
    EXPECT_EQ(MK_AVAIL(VALID), SyncGetLatestProxyConfig(&actual_config));
    EXPECT_EQ(network_params.auto_detect, actual_config.auto_detect());
    EXPECT_EQ(network_params.pac_url, actual_config.pac_url());
    EXPECT_TRUE(network_params.proxy_rules.Matches(
        actual_config.proxy_rules()));

    // Managed proxy pref should take effect over network proxy.
    pref_service_.SetManagedPref(prefs::kProxy,
                                 managed_config.ToPrefProxyConfig());
    EXPECT_EQ(MK_AVAIL(VALID), SyncGetLatestProxyConfig(&actual_config));
    EXPECT_EQ(managed_params.auto_detect, actual_config.auto_detect());
    EXPECT_EQ(managed_params.pac_url, actual_config.pac_url());
    EXPECT_TRUE(managed_params.proxy_rules.Matches(
        actual_config.proxy_rules()));

    // Network proxy should take effect over recommended proxy pref when managed
    // proxy pref is removed.
    pref_service_.RemoveManagedPref(prefs::kProxy);
    EXPECT_EQ(MK_AVAIL(VALID), SyncGetLatestProxyConfig(&actual_config));
    EXPECT_EQ(network_params.auto_detect, actual_config.auto_detect());
    EXPECT_EQ(network_params.pac_url, actual_config.pac_url());
    EXPECT_TRUE(network_params.proxy_rules.Matches(
        actual_config.proxy_rules()));

    // Removing recommended proxy pref should have no effect on network proxy.
    pref_service_.RemoveRecommendedPref(prefs::kProxy);
    EXPECT_EQ(MK_AVAIL(VALID), SyncGetLatestProxyConfig(&actual_config));
    EXPECT_EQ(network_params.auto_detect, actual_config.auto_detect());
    EXPECT_EQ(network_params.pac_url, actual_config.pac_url());
    EXPECT_TRUE(network_params.proxy_rules.Matches(
        actual_config.proxy_rules()));
  }
}

}  // namespace

}  // namespace chromeos
