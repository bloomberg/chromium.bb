// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/pref_proxy_config_service.h"

#include "base/command_line.h"
#include "base/file_path.h"
#include "chrome/browser/net/chrome_url_request_context.h"
#include "chrome/browser/prefs/pref_service_mock_builder.h"
#include "chrome/browser/prefs/proxy_config_dictionary.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/testing_pref_service.h"
#include "net/proxy/proxy_config_service_common_unittest.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Mock;

namespace {

const char kFixedPacUrl[] = "http://chromium.org/fixed_pac_url";

// Testing proxy config service that allows us to fire notifications at will.
class TestProxyConfigService : public net::ProxyConfigService {
 public:
  TestProxyConfigService(const net::ProxyConfig& config,
                         ConfigAvailability availability)
      : config_(config),
        availability_(availability) {}

  void SetProxyConfig(const net::ProxyConfig config,
                      ConfigAvailability availability) {
    config_ = config;
    availability_ = availability;
    FOR_EACH_OBSERVER(net::ProxyConfigService::Observer, observers_,
                      OnProxyConfigChanged(config, availability));
  }

 private:
  virtual void AddObserver(net::ProxyConfigService::Observer* observer) {
    observers_.AddObserver(observer);
  }

  virtual void RemoveObserver(net::ProxyConfigService::Observer* observer) {
    observers_.RemoveObserver(observer);
  }

  virtual net::ProxyConfigService::ConfigAvailability GetLatestProxyConfig(
      net::ProxyConfig* config) {
    *config = config_;
    return availability_;
  }

  net::ProxyConfig config_;
  ConfigAvailability availability_;
  ObserverList<net::ProxyConfigService::Observer, true> observers_;
};

// A mock observer for capturing callbacks.
class MockObserver : public net::ProxyConfigService::Observer {
 public:
  MOCK_METHOD2(OnProxyConfigChanged,
               void(const net::ProxyConfig&,
                    net::ProxyConfigService::ConfigAvailability));
};

template<typename TESTBASE>
class PrefProxyConfigServiceTestBase : public TESTBASE {
 protected:
  PrefProxyConfigServiceTestBase()
      : ui_thread_(BrowserThread::UI, &loop_),
        io_thread_(BrowserThread::IO, &loop_) {}

  virtual void Init(PrefService* pref_service) {
    ASSERT_TRUE(pref_service);
    PrefProxyConfigService::RegisterPrefs(pref_service);
    fixed_config_.set_pac_url(GURL(kFixedPacUrl));
    delegate_service_ =
        new TestProxyConfigService(fixed_config_,
                                   net::ProxyConfigService::CONFIG_VALID);
    proxy_config_tracker_ = new PrefProxyConfigTracker(pref_service);
    proxy_config_service_.reset(
        new PrefProxyConfigService(proxy_config_tracker_.get(),
                                   delegate_service_));
  }

  virtual void TearDown() {
    proxy_config_tracker_->DetachFromPrefService();
    loop_.RunAllPending();
    proxy_config_service_.reset();
  }

  MessageLoop loop_;
  TestProxyConfigService* delegate_service_; // weak
  scoped_ptr<PrefProxyConfigService> proxy_config_service_;
  net::ProxyConfig fixed_config_;

 private:
  scoped_refptr<PrefProxyConfigTracker> proxy_config_tracker_;
  BrowserThread ui_thread_;
  BrowserThread io_thread_;
};

class PrefProxyConfigServiceTest
    : public PrefProxyConfigServiceTestBase<testing::Test> {
 protected:
  virtual void SetUp() {
    pref_service_.reset(new TestingPrefService());
    Init(pref_service_.get());
  }

  scoped_ptr<TestingPrefService> pref_service_;
};

TEST_F(PrefProxyConfigServiceTest, BaseConfiguration) {
  net::ProxyConfig actual_config;
  EXPECT_EQ(net::ProxyConfigService::CONFIG_VALID,
            proxy_config_service_->GetLatestProxyConfig(&actual_config));
  EXPECT_EQ(GURL(kFixedPacUrl), actual_config.pac_url());
}

TEST_F(PrefProxyConfigServiceTest, DynamicPrefOverrides) {
  pref_service_->SetManagedPref(
      prefs::kProxy,
      ProxyConfigDictionary::CreateFixedServers("http://example.com:3128", ""));
  loop_.RunAllPending();

  net::ProxyConfig actual_config;
  EXPECT_EQ(net::ProxyConfigService::CONFIG_VALID,
            proxy_config_service_->GetLatestProxyConfig(&actual_config));
  EXPECT_FALSE(actual_config.auto_detect());
  EXPECT_EQ(net::ProxyConfig::ProxyRules::TYPE_SINGLE_PROXY,
            actual_config.proxy_rules().type);
  EXPECT_EQ(actual_config.proxy_rules().single_proxy,
            net::ProxyServer::FromURI("http://example.com:3128",
                                      net::ProxyServer::SCHEME_HTTP));

  pref_service_->SetManagedPref(prefs::kProxy,
                                ProxyConfigDictionary::CreateAutoDetect());
  loop_.RunAllPending();

  EXPECT_EQ(net::ProxyConfigService::CONFIG_VALID,
            proxy_config_service_->GetLatestProxyConfig(&actual_config));
  EXPECT_TRUE(actual_config.auto_detect());
}

// Compares proxy configurations, but allows different identifiers.
MATCHER_P(ProxyConfigMatches, config, "") {
  net::ProxyConfig reference(config);
  reference.set_id(arg.id());
  return reference.Equals(arg);
}

TEST_F(PrefProxyConfigServiceTest, Observers) {
  const net::ProxyConfigService::ConfigAvailability CONFIG_VALID =
      net::ProxyConfigService::CONFIG_VALID;
  MockObserver observer;
  proxy_config_service_->AddObserver(&observer);

  // Firing the observers in the delegate should trigger a notification.
  net::ProxyConfig config2;
  config2.set_auto_detect(true);
  EXPECT_CALL(observer, OnProxyConfigChanged(ProxyConfigMatches(config2),
                                             CONFIG_VALID)).Times(1);
  delegate_service_->SetProxyConfig(config2, CONFIG_VALID);
  loop_.RunAllPending();
  Mock::VerifyAndClearExpectations(&observer);

  // Override configuration, this should trigger a notification.
  net::ProxyConfig pref_config;
  pref_config.set_pac_url(GURL(kFixedPacUrl));

  EXPECT_CALL(observer, OnProxyConfigChanged(ProxyConfigMatches(pref_config),
                                             CONFIG_VALID)).Times(1);
  pref_service_->SetManagedPref(
      prefs::kProxy,
      ProxyConfigDictionary::CreatePacScript(kFixedPacUrl));
  loop_.RunAllPending();
  Mock::VerifyAndClearExpectations(&observer);

  // Since there are pref overrides, delegate changes should be ignored.
  net::ProxyConfig config3;
  config3.proxy_rules().ParseFromString("http=config3:80");
  EXPECT_CALL(observer, OnProxyConfigChanged(_, _)).Times(0);
  fixed_config_.set_auto_detect(true);
  delegate_service_->SetProxyConfig(config3, CONFIG_VALID);
  loop_.RunAllPending();
  Mock::VerifyAndClearExpectations(&observer);

  // Clear the override should switch back to the fixed configuration.
  EXPECT_CALL(observer, OnProxyConfigChanged(ProxyConfigMatches(config3),
                                             CONFIG_VALID)).Times(1);
  pref_service_->RemoveManagedPref(prefs::kProxy);
  loop_.RunAllPending();
  Mock::VerifyAndClearExpectations(&observer);

  // Delegate service notifications should show up again.
  net::ProxyConfig config4;
  config4.proxy_rules().ParseFromString("socks:config4");
  EXPECT_CALL(observer, OnProxyConfigChanged(ProxyConfigMatches(config4),
                                             CONFIG_VALID)).Times(1);
  delegate_service_->SetProxyConfig(config4, CONFIG_VALID);
  loop_.RunAllPending();
  Mock::VerifyAndClearExpectations(&observer);

  proxy_config_service_->RemoveObserver(&observer);
}

TEST_F(PrefProxyConfigServiceTest, Fallback) {
  const net::ProxyConfigService::ConfigAvailability CONFIG_VALID =
      net::ProxyConfigService::CONFIG_VALID;
  MockObserver observer;
  net::ProxyConfig actual_config;
  delegate_service_->SetProxyConfig(net::ProxyConfig::CreateDirect(),
                                    net::ProxyConfigService::CONFIG_UNSET);
  proxy_config_service_->AddObserver(&observer);

  // Prepare test data.
  net::ProxyConfig recommended_config = net::ProxyConfig::CreateAutoDetect();
  net::ProxyConfig user_config =
      net::ProxyConfig::CreateFromCustomPacURL(GURL(kFixedPacUrl));

  // Set a recommended pref.
  EXPECT_CALL(observer,
              OnProxyConfigChanged(ProxyConfigMatches(recommended_config),
                                   CONFIG_VALID)).Times(1);
  pref_service_->SetRecommendedPref(
      prefs::kProxy,
      ProxyConfigDictionary::CreateAutoDetect());
  loop_.RunAllPending();
  Mock::VerifyAndClearExpectations(&observer);
  EXPECT_EQ(CONFIG_VALID,
            proxy_config_service_->GetLatestProxyConfig(&actual_config));
  EXPECT_TRUE(actual_config.Equals(recommended_config));

  // Override in user prefs.
  EXPECT_CALL(observer,
              OnProxyConfigChanged(ProxyConfigMatches(user_config),
                                   CONFIG_VALID)).Times(1);
  pref_service_->SetManagedPref(
      prefs::kProxy,
      ProxyConfigDictionary::CreatePacScript(kFixedPacUrl));
  loop_.RunAllPending();
  Mock::VerifyAndClearExpectations(&observer);
  EXPECT_EQ(CONFIG_VALID,
            proxy_config_service_->GetLatestProxyConfig(&actual_config));
  EXPECT_TRUE(actual_config.Equals(user_config));

  // Go back to recommended pref.
  EXPECT_CALL(observer,
              OnProxyConfigChanged(ProxyConfigMatches(recommended_config),
                                   CONFIG_VALID)).Times(1);
  pref_service_->RemoveManagedPref(prefs::kProxy);
  loop_.RunAllPending();
  Mock::VerifyAndClearExpectations(&observer);
  EXPECT_EQ(CONFIG_VALID,
            proxy_config_service_->GetLatestProxyConfig(&actual_config));
  EXPECT_TRUE(actual_config.Equals(recommended_config));

  proxy_config_service_->RemoveObserver(&observer);
}

// Test parameter object for testing command line proxy configuration.
struct CommandLineTestParams {
  // Explicit assignment operator, so testing::TestWithParam works with MSVC.
  CommandLineTestParams& operator=(const CommandLineTestParams& other) {
    description = other.description;
    for (unsigned int i = 0; i < arraysize(switches); i++)
      switches[i] = other.switches[i];
    is_null = other.is_null;
    auto_detect = other.auto_detect;
    pac_url = other.pac_url;
    proxy_rules = other.proxy_rules;
    return *this;
  }

  // Short description to identify the test.
  const char* description;

  // The command line to build a ProxyConfig from.
  struct SwitchValue {
    const char* name;
    const char* value;
  } switches[2];

  // Expected outputs (fields of the ProxyConfig).
  bool is_null;
  bool auto_detect;
  GURL pac_url;
  net::ProxyRulesExpectation proxy_rules;
};

void PrintTo(const CommandLineTestParams& params, std::ostream* os) {
  *os << params.description;
}

class PrefProxyConfigServiceCommandLineTest
    : public PrefProxyConfigServiceTestBase<
          testing::TestWithParam<CommandLineTestParams> > {
 protected:
  PrefProxyConfigServiceCommandLineTest()
      : command_line_(CommandLine::NO_PROGRAM) {}

  virtual void SetUp() {
    for (size_t i = 0; i < arraysize(GetParam().switches); i++) {
      const char* name = GetParam().switches[i].name;
      const char* value = GetParam().switches[i].value;
      if (name && value)
        command_line_.AppendSwitchASCII(name, value);
      else if (name)
        command_line_.AppendSwitch(name);
    }
    pref_service_.reset(
        PrefServiceMockBuilder().WithCommandLine(&command_line_).Create());
    Init(pref_service_.get());
  }

 private:
  CommandLine command_line_;
  scoped_ptr<PrefService> pref_service_;
};

TEST_P(PrefProxyConfigServiceCommandLineTest, CommandLine) {
  net::ProxyConfig config;
  EXPECT_EQ(net::ProxyConfigService::CONFIG_VALID,
            proxy_config_service_->GetLatestProxyConfig(&config));

  if (GetParam().is_null) {
    EXPECT_EQ(GURL(kFixedPacUrl), config.pac_url());
  } else {
    EXPECT_NE(GURL(kFixedPacUrl), config.pac_url());
    EXPECT_EQ(GetParam().auto_detect, config.auto_detect());
    EXPECT_EQ(GetParam().pac_url, config.pac_url());
    EXPECT_TRUE(GetParam().proxy_rules.Matches(config.proxy_rules()));
  }
}

static const CommandLineTestParams kCommandLineTestParams[] = {
  {
    "Empty command line",
    // Input
    { },
    // Expected result
    true,                                               // is_null
    false,                                              // auto_detect
    GURL(),                                             // pac_url
    net::ProxyRulesExpectation::Empty(),
  },
  {
    "No proxy",
    // Input
    {
      { switches::kNoProxyServer, NULL },
    },
    // Expected result
    false,                                              // is_null
    false,                                              // auto_detect
    GURL(),                                             // pac_url
    net::ProxyRulesExpectation::Empty(),
  },
  {
    "No proxy with extra parameters.",
    // Input
    {
      { switches::kNoProxyServer, NULL },
      { switches::kProxyServer, "http://proxy:8888" },
    },
    // Expected result
    false,                                              // is_null
    false,                                              // auto_detect
    GURL(),                                             // pac_url
    net::ProxyRulesExpectation::Empty(),
  },
  {
    "Single proxy.",
    // Input
    {
      { switches::kProxyServer, "http://proxy:8888" },
    },
    // Expected result
    false,                                              // is_null
    false,                                              // auto_detect
    GURL(),                                             // pac_url
    net::ProxyRulesExpectation::Single(
        "proxy:8888",  // single proxy
        ""),           // bypass rules
  },
  {
    "Per scheme proxy.",
    // Input
    {
      { switches::kProxyServer, "http=httpproxy:8888;ftp=ftpproxy:8889" },
    },
    // Expected result
    false,                                              // is_null
    false,                                              // auto_detect
    GURL(),                                             // pac_url
    net::ProxyRulesExpectation::PerScheme(
        "httpproxy:8888",  // http
        "",                // https
        "ftpproxy:8889",   // ftp
        ""),               // bypass rules
  },
  {
    "Per scheme proxy with bypass URLs.",
    // Input
    {
      { switches::kProxyServer, "http=httpproxy:8888;ftp=ftpproxy:8889" },
      { switches::kProxyBypassList,
        ".google.com, foo.com:99, 1.2.3.4:22, 127.0.0.1/8" },
    },
    // Expected result
    false,                                              // is_null
    false,                                              // auto_detect
    GURL(),                                             // pac_url
    net::ProxyRulesExpectation::PerScheme(
        "httpproxy:8888",  // http
        "",                // https
        "ftpproxy:8889",   // ftp
        "*.google.com,foo.com:99,1.2.3.4:22,127.0.0.1/8"),
  },
  {
    "Pac URL",
    // Input
    {
      { switches::kProxyPacUrl, "http://wpad/wpad.dat" },
    },
    // Expected result
    false,                                              // is_null
    false,                                              // auto_detect
    GURL("http://wpad/wpad.dat"),                       // pac_url
    net::ProxyRulesExpectation::Empty(),
  },
  {
    "Autodetect",
    // Input
    {
      { switches::kProxyAutoDetect, NULL },
    },
    // Expected result
    false,                                              // is_null
    true,                                               // auto_detect
    GURL(),                                             // pac_url
    net::ProxyRulesExpectation::Empty(),
  },
};

INSTANTIATE_TEST_CASE_P(
    PrefProxyConfigServiceCommandLineTestInstance,
    PrefProxyConfigServiceCommandLineTest,
    testing::ValuesIn(kCommandLineTestParams));

}  // namespace
