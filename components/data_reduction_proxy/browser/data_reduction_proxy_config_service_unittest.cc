// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/browser/data_reduction_proxy_config_service.h"

#include <string>

#include "base/bind.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop.h"
#include "base/test/test_simple_task_runner.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::Mock;

namespace {

// Test system proxy rules.
static const char kSystemProxyRules[] = "http=http://system.com:80,direct://";

// Test data reduction proxy rules.
static const char kDataReductionProxyRules[] =
    "http=https://foo.com:443,http://bar.com:80,direct://";

// Test data reduction proxy rules when in restricted mode.
static const char kDataReductionProxyRestrictedRules[] =
    "http=http://bar.com:80,direct://";

}  // namespace

namespace data_reduction_proxy {

class TestProxyConfigService : public net::ProxyConfigService {
 public:
  TestProxyConfigService()
      : availability_(net::ProxyConfigService::CONFIG_VALID) {
    config_.proxy_rules().ParseFromString(kSystemProxyRules);
  }

  void SetProxyConfig(const net::ProxyConfig config,
                      ConfigAvailability availability) {
    config_ = config;
    availability_ = availability;
    FOR_EACH_OBSERVER(net::ProxyConfigService::Observer, observers_,
                      OnProxyConfigChanged(config, availability));
  }

  virtual void AddObserver(
      net::ProxyConfigService::Observer* observer) OVERRIDE {
    observers_.AddObserver(observer);
  }

  virtual void RemoveObserver(
      net::ProxyConfigService::Observer* observer) OVERRIDE {
    observers_.RemoveObserver(observer);
  }

  virtual ConfigAvailability GetLatestProxyConfig(
      net::ProxyConfig* config) OVERRIDE {
    *config = config_;
    return availability_;
  }

 private:
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


class DataReductionProxyConfigServiceTest : public testing::Test {
 public:
  virtual void SetUp() {
    observer_.reset(new MockObserver());
    base_service_ = new TestProxyConfigService();
    scoped_ptr<net::ProxyConfigService> base_service(base_service_);
    config_service_.reset(
        new DataReductionProxyConfigService(base_service.Pass()));
  }

  void EnableDataReductionProxy(bool data_reduction_proxy_enabled) {
    config_service_->enabled_ = data_reduction_proxy_enabled;
    config_service_->config_.proxy_rules().ParseFromString(
        kDataReductionProxyRules);
  }

  scoped_ptr<net::ProxyConfigService::Observer> observer_;

  // Holds a weak pointer to the base service. Ownership is passed to
  // |config_service_|.
  TestProxyConfigService* base_service_;

  scoped_ptr<DataReductionProxyConfigService> config_service_;
};

// Compares proxy configurations, but allows different identifiers.
MATCHER_P(ProxyConfigMatches, config, "") {
  net::ProxyConfig reference(config);
  reference.set_id(arg.id());
  return reference.Equals(arg);
}

TEST_F(DataReductionProxyConfigServiceTest, GetLatestProxyConfigEnabled) {
  // Set up the |config_service_| as though Enable had been previously called
  // and check that |GetLatestProxyConfigEnabled| return rules for the data
  // reduction proxy.
  EnableDataReductionProxy(true);
  net::ProxyConfig::ProxyRules expected_rules;
  expected_rules.ParseFromString(kDataReductionProxyRules);
  net::ProxyConfig latest_config;
  EXPECT_EQ(net::ProxyConfigService::CONFIG_VALID,
            config_service_->GetLatestProxyConfig(&latest_config));
  ASSERT_TRUE(latest_config.proxy_rules().Equals(expected_rules));
}

TEST_F(DataReductionProxyConfigServiceTest, GetLatestProxyConfigDisabledValid) {
  // Set up the |config_service_| with the data reduction proxy disabled and
  // check that the underlying system config is returned.
  EnableDataReductionProxy(false);
  net::ProxyConfig::ProxyRules expected_rules;
  expected_rules.ParseFromString(kSystemProxyRules);
  net::ProxyConfig latest_config;
  EXPECT_EQ(net::ProxyConfigService::CONFIG_VALID,
            config_service_->GetLatestProxyConfig(&latest_config));
  ASSERT_TRUE(latest_config.proxy_rules().Equals(expected_rules));
}

TEST_F(DataReductionProxyConfigServiceTest, GetLatestProxyConfigDisabledUnset) {
  // Set up the |config_service_| with the data reduction proxy disabled and
  // check that direct is returned if the the underlying system config is unset.
  EnableDataReductionProxy(false);
  base_service_->SetProxyConfig(net::ProxyConfig(),
                                net::ProxyConfigService::CONFIG_UNSET);
  net::ProxyConfig latest_config;
  EXPECT_EQ(net::ProxyConfigService::CONFIG_VALID,
            config_service_->GetLatestProxyConfig(&latest_config));
  ASSERT_TRUE(latest_config.Equals(net::ProxyConfig()));
}

TEST_F(DataReductionProxyConfigServiceTest, UpdateProxyConfig) {
  MockObserver observer;
  base::MessageLoopForUI loop;
  config_service_->AddObserver(&observer);
  // Firing the observers in the delegate should trigger a notification.
  net::ProxyConfig config2;
  config2.set_auto_detect(true);
  EXPECT_CALL(observer, OnProxyConfigChanged(
      ProxyConfigMatches(config2),
      net::ProxyConfigService::CONFIG_VALID)).Times(1);
  base_service_->SetProxyConfig(config2, net::ProxyConfigService::CONFIG_VALID);
  loop.RunUntilIdle();
  Mock::VerifyAndClearExpectations(&observer);

  // Enable the data reduction proxy, which should trigger a notification.
  net::ProxyConfig system_config;
  system_config.proxy_rules().ParseFromString(kSystemProxyRules);
  base_service_->SetProxyConfig(system_config,
                                net::ProxyConfigService::CONFIG_VALID);
  net::ProxyConfig data_reduction_proxy_config;
  data_reduction_proxy_config.proxy_rules().ParseFromString(
      kDataReductionProxyRules);

  EXPECT_CALL(observer, OnProxyConfigChanged(
      ProxyConfigMatches(data_reduction_proxy_config),
      net::ProxyConfigService::CONFIG_VALID)).Times(1);
  config_service_->UpdateProxyConfig(true, data_reduction_proxy_config);
  loop.RunUntilIdle();
  Mock::VerifyAndClearExpectations(&observer);


  // Disable the data reduction proxy, which should trigger a notification.
  base_service_->SetProxyConfig(system_config,
                                net::ProxyConfigService::CONFIG_VALID);
  EXPECT_CALL(observer, OnProxyConfigChanged(
                  ProxyConfigMatches(system_config),
                  net::ProxyConfigService::CONFIG_VALID)).Times(1);
  config_service_->UpdateProxyConfig(false, data_reduction_proxy_config);
  loop.RunUntilIdle();
  Mock::VerifyAndClearExpectations(&observer);

  config_service_->RemoveObserver(&observer);
}

TEST_F(DataReductionProxyConfigServiceTest, TrackerEnable) {
  MockObserver observer;
  //base::MessageLoopForUI loop;
  config_service_->AddObserver(&observer);
  scoped_refptr<base::TestSimpleTaskRunner> task_runner_(
      new base::TestSimpleTaskRunner());
  DataReductionProxyConfigTracker tracker(
      base::Bind(&data_reduction_proxy::DataReductionProxyConfigService::
                     UpdateProxyConfig,
                 base::Unretained(config_service_.get())),
      task_runner_.get());
  net::ProxyConfig expected_config;
  expected_config.proxy_rules().ParseFromString(kDataReductionProxyRules);
  EXPECT_CALL(observer, OnProxyConfigChanged(
                  ProxyConfigMatches(expected_config),
                  net::ProxyConfigService::CONFIG_VALID)).Times(1);
  tracker.Enable(false,
                 false,
                 "https://foo.com:443",
                 "http://bar.com:80",
                 "");
  task_runner_->RunUntilIdle();
  Mock::VerifyAndClearExpectations(&observer);

  config_service_->RemoveObserver(&observer);
}

TEST_F(DataReductionProxyConfigServiceTest, TrackerEnableRestricted) {
  MockObserver observer;
  //base::MessageLoopForUI loop;
  config_service_->AddObserver(&observer);
  scoped_refptr<base::TestSimpleTaskRunner> task_runner_(
      new base::TestSimpleTaskRunner());
  DataReductionProxyConfigTracker tracker(
      base::Bind(&data_reduction_proxy::DataReductionProxyConfigService::
                     UpdateProxyConfig,
                 base::Unretained(config_service_.get())),
      task_runner_.get());
  net::ProxyConfig expected_config;
  expected_config.proxy_rules().ParseFromString(
      kDataReductionProxyRestrictedRules);
  EXPECT_CALL(observer, OnProxyConfigChanged(
                  ProxyConfigMatches(expected_config),
                  net::ProxyConfigService::CONFIG_VALID)).Times(1);
  tracker.Enable(true,
                 false,
                 "https://foo.com:443",
                 "http://bar.com:80",
                 "");
  task_runner_->RunUntilIdle();
  Mock::VerifyAndClearExpectations(&observer);

  config_service_->RemoveObserver(&observer);
}

TEST_F(DataReductionProxyConfigServiceTest, TrackerDisable) {
  MockObserver observer;
  //base::MessageLoopForUI loop;
  config_service_->AddObserver(&observer);
  scoped_refptr<base::TestSimpleTaskRunner> task_runner_(
      new base::TestSimpleTaskRunner());
  DataReductionProxyConfigTracker tracker(
      base::Bind(&data_reduction_proxy::DataReductionProxyConfigService::
                     UpdateProxyConfig,
                 base::Unretained(config_service_.get())),
      task_runner_.get());
  net::ProxyConfig expected_config;
  expected_config.proxy_rules().ParseFromString(kSystemProxyRules);
  EXPECT_CALL(observer, OnProxyConfigChanged(
                  ProxyConfigMatches(expected_config),
                  net::ProxyConfigService::CONFIG_VALID)).Times(1);
  tracker.Disable();
  task_runner_->RunUntilIdle();
  //loop.RunUntilIdle();
  Mock::VerifyAndClearExpectations(&observer);

  config_service_->RemoveObserver(&observer);
}


TEST_F(DataReductionProxyConfigServiceTest, TrackerBypassList) {
  base::MessageLoopForUI loop;
  scoped_refptr<base::TestSimpleTaskRunner> task_runner_(
      new base::TestSimpleTaskRunner());
  DataReductionProxyConfigTracker tracker(
      base::Bind(&data_reduction_proxy::DataReductionProxyConfigService::
                     UpdateProxyConfig,
                 base::Unretained(config_service_.get())),
      task_runner_.get());
  tracker.AddHostPatternToBypass("http://www.google.com");
  tracker.AddHostPatternToBypass("fefe:13::abc/33");
  tracker.AddURLPatternToBypass("foo.org/images/*");
  tracker.AddURLPatternToBypass("http://foo.com/*");
  tracker.AddURLPatternToBypass("http://baz.com:22/bar/*");
  tracker.AddURLPatternToBypass("http://*bat.com/bar/*");

  std::string expected[] = {
    "http://www.google.com",
    "fefe:13::abc/33",
    "foo.org",
    "http://foo.com",
    "http://baz.com:22",
    "http://*bat.com"
  };

  ASSERT_EQ(tracker.bypass_rules_.size(), 6u);
  int i = 0;
  for (std::vector<std::string>::iterator it = tracker.bypass_rules_.begin();
       it != tracker.bypass_rules_.end(); ++it) {
    EXPECT_EQ(expected[i++], *it);
  }
}

}  // namespace data_reduction_proxy
