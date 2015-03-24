// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_config_service_client.h"

#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_config_test_utils.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_test_utils.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_client_config_parser.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params_test_utils.h"
#include "components/data_reduction_proxy/proto/client_config.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace data_reduction_proxy {

class DataReductionProxyConfigClientTest : public testing::Test {
 protected:
  void SetUp() override {
    test_context_ =
        DataReductionProxyTestContext::Builder()
            .WithParamsFlags(DataReductionProxyParams::kAllowed |
                             DataReductionProxyParams::kFallbackAllowed |
                             DataReductionProxyParams::kPromoAllowed)
            .WithParamsDefinitions(TestDataReductionProxyParams::HAS_EVERYTHING)
            .Build();
  }

  scoped_ptr<DataReductionProxyConfigServiceClient> BuildConfigClient() {
    return make_scoped_ptr(new DataReductionProxyConfigServiceClient(
        test_context_->config()->test_params(),
        test_context_->io_data()->request_options()));
  }

 private:
  scoped_ptr<DataReductionProxyTestContext> test_context_;
};

TEST_F(DataReductionProxyConfigClientTest, TestConstructStaticResponse) {
  scoped_ptr<DataReductionProxyConfigServiceClient> config_client =
      BuildConfigClient();
  std::string config_data = config_client->ConstructStaticResponse();
  ClientConfig config;
  EXPECT_TRUE(config_parser::ParseClientConfig(config_data, &config));
}

}  // namespace data_reduction_proxy
