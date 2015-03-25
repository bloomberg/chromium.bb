// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_config_service_client.h"

#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/time/tick_clock.h"
#include "base/values.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_config_test_utils.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_configurator_test_utils.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_test_utils.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_client_config_parser.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params_test_utils.h"
#include "components/data_reduction_proxy/proto/client_config.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace data_reduction_proxy {

namespace {

class RequestOptionsPopulator {
 public:
  RequestOptionsPopulator(const base::Time& expiration_time,
                          const base::TimeDelta& increment)
      : expiration_time_(expiration_time),
        increment_(increment) {
  }

  void PopulateResponse(base::DictionaryValue* response) {
    response->SetString("sessionKey", "abcdef|1234-5678-12345678");
    response->SetString("expireTime",
                        config_parser::TimeToISO8601(expiration_time_));
    expiration_time_ += increment_;
  }

 private:
  base::Time expiration_time_;
  base::TimeDelta increment_;
};

void PopulateResponseFailure(base::DictionaryValue* response) {
}

}  // namespace

class DataReductionProxyConfigServiceClientTest : public testing::Test {
 protected:
  void SetUp() override {
    test_context_ =
        DataReductionProxyTestContext::Builder()
            .WithParamsFlags(DataReductionProxyParams::kAllowed |
                             DataReductionProxyParams::kFallbackAllowed |
                             DataReductionProxyParams::kPromoAllowed)
            .WithParamsDefinitions(TestDataReductionProxyParams::HAS_EVERYTHING)
            .WithTestConfigurator()
            .WithMockRequestOptions()
            .WithTestConfigClient()
            .Build();
    test_context_->test_config_client()->SetCustomReleaseTime(
        base::TimeTicks::UnixEpoch());
    test_context_->test_config_client()->SetNow(base::Time::UnixEpoch());
  }

  void SetDataReductionProxyEnabled(bool enabled) {
    test_context_->config()->SetStateForTest(enabled, false, false);
  }

  scoped_ptr<DataReductionProxyConfigServiceClient> BuildConfigClient() {
    scoped_ptr<DataReductionProxyParams> params(new DataReductionProxyParams(
        DataReductionProxyParams::kAllowed |
        DataReductionProxyParams::kFallbackAllowed |
        DataReductionProxyParams::kPromoAllowed));
    request_options_.reset(
        new DataReductionProxyRequestOptions(Client::UNKNOWN,
                                             test_context_->io_data()->config(),
                                             test_context_->task_runner()));
    return scoped_ptr<DataReductionProxyConfigServiceClient>(
        new DataReductionProxyConfigServiceClient(
            params.Pass(), GetBackoffPolicy(),
            request_options_.get(),
            test_context_->mutable_config_values(),
            test_context_->io_data()->config(), test_context_->task_runner()));
  }

  DataReductionProxyParams* params() {
    return test_context_->test_params();
  }

  TestDataReductionProxyConfigServiceClient* config_client() {
    return test_context_->test_config_client();
  }

  TestDataReductionProxyConfigurator* configurator() {
    return test_context_->test_configurator();
  }

  MockDataReductionProxyRequestOptions* request_options() {
    return test_context_->mock_request_options();
  }

  void RunUntilIdle() {
    test_context_->RunUntilIdle();
  }

 private:
  scoped_ptr<DataReductionProxyTestContext> test_context_;
  scoped_ptr<DataReductionProxyRequestOptions> request_options_;
};

TEST_F(DataReductionProxyConfigServiceClientTest, TestConstructStaticResponse) {
  scoped_ptr<DataReductionProxyConfigServiceClient> config_client =
      BuildConfigClient();
  std::string config_data = config_client->ConstructStaticResponse();
  ClientConfig config;
  EXPECT_TRUE(config_parser::ParseClientConfig(config_data, &config));
}

TEST_F(DataReductionProxyConfigServiceClientTest, SuccessfulLoop) {
  RequestOptionsPopulator populator(
      base::Time::UnixEpoch() + base::TimeDelta::FromDays(1),
      base::TimeDelta::FromDays(1));
  SetDataReductionProxyEnabled(true);
  EXPECT_TRUE(configurator()->origin().empty());
  EXPECT_TRUE(configurator()->fallback_origin().empty());
  EXPECT_TRUE(configurator()->ssl_origin().empty());
  EXPECT_CALL(*request_options(), PopulateConfigResponse(testing::_))
      .Times(2)
      .WillRepeatedly(
          testing::Invoke(&populator,
                          &RequestOptionsPopulator::PopulateResponse));
  RunUntilIdle();
  EXPECT_EQ(base::TimeDelta::FromDays(1), config_client()->GetDelay());
  EXPECT_EQ(params()->origin().ToURI(), configurator()->origin());
  EXPECT_EQ(params()->fallback_origin().ToURI(),
            configurator()->fallback_origin());
  EXPECT_TRUE(configurator()->ssl_origin().empty());
  config_client()->SetNow(base::Time::UnixEpoch() + base::TimeDelta::FromDays(1)
                          + base::TimeDelta::FromSeconds(5));
  config_client()->RetrieveConfig();
  EXPECT_EQ(base::TimeDelta::FromDays(1) - base::TimeDelta::FromSeconds(5),
            config_client()->GetDelay());
  EXPECT_EQ(params()->origin().ToURI(), configurator()->origin());
  EXPECT_EQ(params()->fallback_origin().ToURI(),
            configurator()->fallback_origin());
  EXPECT_TRUE(configurator()->ssl_origin().empty());
}

TEST_F(DataReductionProxyConfigServiceClientTest, SuccessfulLoopShortDuration) {
  RequestOptionsPopulator populator(
      base::Time::UnixEpoch() + base::TimeDelta::FromSeconds(1),
      base::TimeDelta::FromSeconds(1));
  SetDataReductionProxyEnabled(true);
  EXPECT_TRUE(configurator()->origin().empty());
  EXPECT_TRUE(configurator()->fallback_origin().empty());
  EXPECT_TRUE(configurator()->ssl_origin().empty());
  EXPECT_CALL(*request_options(), PopulateConfigResponse(testing::_))
      .Times(1)
      .WillOnce(testing::Invoke(&populator,
                                &RequestOptionsPopulator::PopulateResponse));
  RunUntilIdle();
  EXPECT_EQ(base::TimeDelta::FromSeconds(10), config_client()->GetDelay());
  EXPECT_EQ(params()->origin().ToURI(), configurator()->origin());
  EXPECT_EQ(params()->fallback_origin().ToURI(),
            configurator()->fallback_origin());
  EXPECT_TRUE(configurator()->ssl_origin().empty());
}

TEST_F(DataReductionProxyConfigServiceClientTest, EnsureBackoff) {
  SetDataReductionProxyEnabled(true);
  EXPECT_TRUE(configurator()->origin().empty());
  EXPECT_TRUE(configurator()->fallback_origin().empty());
  EXPECT_TRUE(configurator()->ssl_origin().empty());
  EXPECT_CALL(*request_options(), PopulateConfigResponse(testing::_))
      .Times(2)
      .WillRepeatedly(testing::Invoke(&PopulateResponseFailure));
  config_client()->RetrieveConfig();
  EXPECT_TRUE(configurator()->origin().empty());
  EXPECT_TRUE(configurator()->fallback_origin().empty());
  EXPECT_TRUE(configurator()->ssl_origin().empty());
  EXPECT_EQ(base::TimeDelta::FromSeconds(20), config_client()->GetDelay());
  config_client()->RetrieveConfig();
  EXPECT_TRUE(configurator()->origin().empty());
  EXPECT_TRUE(configurator()->fallback_origin().empty());
  EXPECT_TRUE(configurator()->ssl_origin().empty());
  EXPECT_EQ(base::TimeDelta::FromSeconds(40), config_client()->GetDelay());
}

TEST_F(DataReductionProxyConfigServiceClientTest, ConfigDisabled) {
  RequestOptionsPopulator populator(
      base::Time::UnixEpoch() + base::TimeDelta::FromDays(1),
      base::TimeDelta::FromDays(1));
  SetDataReductionProxyEnabled(false);
  EXPECT_TRUE(configurator()->origin().empty());
  EXPECT_TRUE(configurator()->fallback_origin().empty());
  EXPECT_TRUE(configurator()->ssl_origin().empty());
  EXPECT_CALL(*request_options(), PopulateConfigResponse(testing::_))
      .Times(1)
      .WillOnce(testing::Invoke(&populator,
                                &RequestOptionsPopulator::PopulateResponse));
  RunUntilIdle();
  EXPECT_TRUE(configurator()->origin().empty());
  EXPECT_TRUE(configurator()->fallback_origin().empty());
  EXPECT_TRUE(configurator()->ssl_origin().empty());
  EXPECT_EQ(base::TimeDelta::FromDays(1), config_client()->GetDelay());
}

}  // namespace data_reduction_proxy
