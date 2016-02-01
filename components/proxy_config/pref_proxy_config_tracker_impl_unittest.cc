// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/proxy_config/pref_proxy_config_tracker_impl.h"

#include <string>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/message_loop/message_loop.h"
#include "base/test/histogram_tester.h"
#include "base/thread_task_runner_handle.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/proxy_config/proxy_config_dictionary.h"
#include "components/proxy_config/proxy_config_pref_names.h"
#include "net/proxy/proxy_info.h"
#include "net/proxy/proxy_list.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

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
  void AddObserver(net::ProxyConfigService::Observer* observer) override {
    observers_.AddObserver(observer);
  }

  void RemoveObserver(net::ProxyConfigService::Observer* observer) override {
    observers_.RemoveObserver(observer);
  }

  net::ProxyConfigService::ConfigAvailability GetLatestProxyConfig(
      net::ProxyConfig* config) override {
    *config = config_;
    return availability_;
  }

  net::ProxyConfig config_;
  ConfigAvailability availability_;
  base::ObserverList<net::ProxyConfigService::Observer, true> observers_;
};

// A mock observer for capturing callbacks.
class MockObserver : public net::ProxyConfigService::Observer {
 public:
  MOCK_METHOD2(OnProxyConfigChanged,
               void(const net::ProxyConfig&,
                    net::ProxyConfigService::ConfigAvailability));
};

class PrefProxyConfigTrackerImplTest : public testing::Test {
 protected:
  PrefProxyConfigTrackerImplTest() {
    pref_service_.reset(new TestingPrefServiceSimple());
    PrefProxyConfigTrackerImpl::RegisterPrefs(pref_service_->registry());
    fixed_config_.set_pac_url(GURL(kFixedPacUrl));
    delegate_service_ =
        new TestProxyConfigService(fixed_config_,
                                   net::ProxyConfigService::CONFIG_VALID);
    proxy_config_tracker_.reset(new PrefProxyConfigTrackerImpl(
        pref_service_.get(), base::ThreadTaskRunnerHandle::Get()));
    proxy_config_service_ =
        proxy_config_tracker_->CreateTrackingProxyConfigService(
            scoped_ptr<net::ProxyConfigService>(delegate_service_));
    // SetProxyConfigServiceImpl triggers update of initial prefs proxy
    // config by tracker to chrome proxy config service, so flush all pending
    // tasks so that tests start fresh.
    loop_.RunUntilIdle();
  }

  ~PrefProxyConfigTrackerImplTest() override {
    proxy_config_tracker_->DetachFromPrefService();
    loop_.RunUntilIdle();
    proxy_config_tracker_.reset();
    proxy_config_service_.reset();
  }

  base::MessageLoop loop_;
  scoped_ptr<TestingPrefServiceSimple> pref_service_;
  TestProxyConfigService* delegate_service_; // weak
  scoped_ptr<net::ProxyConfigService> proxy_config_service_;
  net::ProxyConfig fixed_config_;

 private:
  scoped_ptr<PrefProxyConfigTrackerImpl> proxy_config_tracker_;
};

TEST_F(PrefProxyConfigTrackerImplTest, BaseConfiguration) {
  net::ProxyConfig actual_config;
  EXPECT_EQ(net::ProxyConfigService::CONFIG_VALID,
            proxy_config_service_->GetLatestProxyConfig(&actual_config));
  EXPECT_EQ(GURL(kFixedPacUrl), actual_config.pac_url());
}

TEST_F(PrefProxyConfigTrackerImplTest, DynamicPrefOverrides) {
  pref_service_->SetManagedPref(proxy_config::prefs::kProxy,
                                ProxyConfigDictionary::CreateFixedServers(
                                    "http://example.com:3128", std::string()));
  loop_.RunUntilIdle();

  net::ProxyConfig actual_config;
  EXPECT_EQ(net::ProxyConfigService::CONFIG_VALID,
            proxy_config_service_->GetLatestProxyConfig(&actual_config));
  EXPECT_FALSE(actual_config.auto_detect());
  EXPECT_EQ(net::ProxyConfig::ProxyRules::TYPE_SINGLE_PROXY,
            actual_config.proxy_rules().type);
  EXPECT_EQ(actual_config.proxy_rules().single_proxies.Get(),
            net::ProxyServer::FromURI("http://example.com:3128",
                                      net::ProxyServer::SCHEME_HTTP));

  pref_service_->SetManagedPref(proxy_config::prefs::kProxy,
                                ProxyConfigDictionary::CreateAutoDetect());
  loop_.RunUntilIdle();

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

TEST_F(PrefProxyConfigTrackerImplTest, Observers) {
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
  loop_.RunUntilIdle();
  Mock::VerifyAndClearExpectations(&observer);

  // Override configuration, this should trigger a notification.
  net::ProxyConfig pref_config;
  pref_config.set_pac_url(GURL(kFixedPacUrl));

  EXPECT_CALL(observer, OnProxyConfigChanged(ProxyConfigMatches(pref_config),
                                             CONFIG_VALID)).Times(1);
  pref_service_->SetManagedPref(
      proxy_config::prefs::kProxy,
      ProxyConfigDictionary::CreatePacScript(kFixedPacUrl, false));
  loop_.RunUntilIdle();
  Mock::VerifyAndClearExpectations(&observer);

  // Since there are pref overrides, delegate changes should be ignored.
  net::ProxyConfig config3;
  config3.proxy_rules().ParseFromString("http=config3:80");
  EXPECT_CALL(observer, OnProxyConfigChanged(_, _)).Times(0);
  fixed_config_.set_auto_detect(true);
  delegate_service_->SetProxyConfig(config3, CONFIG_VALID);
  loop_.RunUntilIdle();
  Mock::VerifyAndClearExpectations(&observer);

  // Clear the override should switch back to the fixed configuration.
  EXPECT_CALL(observer, OnProxyConfigChanged(ProxyConfigMatches(config3),
                                             CONFIG_VALID)).Times(1);
  pref_service_->RemoveManagedPref(proxy_config::prefs::kProxy);
  loop_.RunUntilIdle();
  Mock::VerifyAndClearExpectations(&observer);

  // Delegate service notifications should show up again.
  net::ProxyConfig config4;
  config4.proxy_rules().ParseFromString("socks:config4");
  EXPECT_CALL(observer, OnProxyConfigChanged(ProxyConfigMatches(config4),
                                             CONFIG_VALID)).Times(1);
  delegate_service_->SetProxyConfig(config4, CONFIG_VALID);
  loop_.RunUntilIdle();
  Mock::VerifyAndClearExpectations(&observer);

  proxy_config_service_->RemoveObserver(&observer);
}

TEST_F(PrefProxyConfigTrackerImplTest, Fallback) {
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
  pref_service_->SetRecommendedPref(proxy_config::prefs::kProxy,
                                    ProxyConfigDictionary::CreateAutoDetect());
  loop_.RunUntilIdle();
  Mock::VerifyAndClearExpectations(&observer);
  EXPECT_EQ(CONFIG_VALID,
            proxy_config_service_->GetLatestProxyConfig(&actual_config));
  EXPECT_TRUE(actual_config.Equals(recommended_config));

  // Override in user prefs.
  EXPECT_CALL(observer,
              OnProxyConfigChanged(ProxyConfigMatches(user_config),
                                   CONFIG_VALID)).Times(1);
  pref_service_->SetManagedPref(
      proxy_config::prefs::kProxy,
      ProxyConfigDictionary::CreatePacScript(kFixedPacUrl, false));
  loop_.RunUntilIdle();
  Mock::VerifyAndClearExpectations(&observer);
  EXPECT_EQ(CONFIG_VALID,
            proxy_config_service_->GetLatestProxyConfig(&actual_config));
  EXPECT_TRUE(actual_config.Equals(user_config));

  // Go back to recommended pref.
  EXPECT_CALL(observer,
              OnProxyConfigChanged(ProxyConfigMatches(recommended_config),
                                   CONFIG_VALID)).Times(1);
  pref_service_->RemoveManagedPref(proxy_config::prefs::kProxy);
  loop_.RunUntilIdle();
  Mock::VerifyAndClearExpectations(&observer);
  EXPECT_EQ(CONFIG_VALID,
            proxy_config_service_->GetLatestProxyConfig(&actual_config));
  EXPECT_TRUE(actual_config.Equals(recommended_config));

  proxy_config_service_->RemoveObserver(&observer);
}

TEST_F(PrefProxyConfigTrackerImplTest, ExplicitSystemSettings) {
  pref_service_->SetRecommendedPref(proxy_config::prefs::kProxy,
                                    ProxyConfigDictionary::CreateAutoDetect());
  pref_service_->SetUserPref(proxy_config::prefs::kProxy,
                             ProxyConfigDictionary::CreateSystem());
  loop_.RunUntilIdle();

  // Test if we actually use the system setting, which is |kFixedPacUrl|.
  net::ProxyConfig actual_config;
  EXPECT_EQ(net::ProxyConfigService::CONFIG_VALID,
            proxy_config_service_->GetLatestProxyConfig(&actual_config));
  EXPECT_EQ(GURL(kFixedPacUrl), actual_config.pac_url());
}

void CheckResolvedProxyMatches(net::ProxyConfig* config,
                               const GURL& url,
                               const std::string& result_string) {
  net::ProxyInfo expected_result;
  expected_result.UseNamedProxy(result_string);

  net::ProxyInfo result;
  config->proxy_rules().Apply(url, &result);

  EXPECT_TRUE(expected_result.proxy_list().Equals(result.proxy_list()))
      << "expected: " << expected_result.proxy_list().ToPacString()
      << "\nactual: " << result.proxy_list().ToPacString();
}

TEST_F(PrefProxyConfigTrackerImplTest, ExcludeGooglezipDataReductionProxies) {
  const std::string kDataReductionProxies =
      "https://proxy.googlezip.net:443,compress.googlezip.net,"
      "https://proxy-dev.googlezip.net:443,proxy-dev.googlezip.net,"
      "quic://proxy.googlezip.net";
  const int kNumDataReductionProxies = 5;

  struct {
    std::string initial_proxy_rules;
    const char* http_proxy_info;
    const char* https_proxy_info;
    const char* ftp_proxy_info;
    int expected_num_removed_proxies;
  } test_cases[] = {
      {"http=foopyhttp," + kDataReductionProxies +
           ",direct://;https=foopyhttps," + kDataReductionProxies +
           ",direct://;ftp=foopyftp," + kDataReductionProxies + ",direct://",
       "foopyhttp;direct://",
       "foopyhttps;direct://",
       "foopyftp;direct://",
       kNumDataReductionProxies * 3},

      {"foopy," + kDataReductionProxies + ",direct://",
       "foopy;direct://",
       "foopy;direct://",
       "foopy;direct://",
       kNumDataReductionProxies},

      {"http=" + kDataReductionProxies + ";https=" + kDataReductionProxies +
           ";ftp=" + kDataReductionProxies,
       "direct://",
       "direct://",
       "direct://",
       kNumDataReductionProxies * 3},

      {"http=" + kDataReductionProxies + ",foopy,direct://",
       "foopy;direct://",
       "direct://",
       "direct://",
       kNumDataReductionProxies},

      {"foopy,direct://",
       "foopy;direct://",
       "foopy;direct://",
       "foopy;direct://",
       0},

      {"direct://",
       "direct://",
       "direct://",
       "direct://",
       0},
  };

  // Test setting the proxy from a user pref.
  for (const auto& test : test_cases) {
    base::HistogramTester histogram_tester;
    pref_service_->SetUserPref(proxy_config::prefs::kProxy,
                               ProxyConfigDictionary::CreateFixedServers(
                                   test.initial_proxy_rules, std::string()));
    loop_.RunUntilIdle();

    net::ProxyConfig config;
    EXPECT_EQ(net::ProxyConfigService::CONFIG_VALID,
              proxy_config_service_->GetLatestProxyConfig(&config));
    histogram_tester.ExpectUniqueSample(
        "Net.PrefProxyConfig.GooglezipProxyRemovalCount",
        test.expected_num_removed_proxies, 1);

    CheckResolvedProxyMatches(&config, GURL("http://google.com"),
                              test.http_proxy_info);
    CheckResolvedProxyMatches(&config, GURL("https://google.com"),
                              test.https_proxy_info);
    CheckResolvedProxyMatches(&config, GURL("ftp://google.com"),
                              test.ftp_proxy_info);
  }
}

}  // namespace
