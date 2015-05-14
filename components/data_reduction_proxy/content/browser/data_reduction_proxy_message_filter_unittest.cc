// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/content/browser/data_reduction_proxy_message_filter.h"

#include "base/command_line.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_config_test_utils.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_test_utils.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params_test_utils.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_switches.h"
#include "net/base/host_port_pair.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace data_reduction_proxy {

class DataReductionProxyMessageFilterTest : public testing::Test {
 public:
  void SetUp() override {
    test_context_ =
        DataReductionProxyTestContext::Builder()
            .WithParamsFlags(DataReductionProxyParams::kAllowed)
            .WithParamsDefinitions(TestDataReductionProxyParams::HAS_EVERYTHING)
            .WithMockConfig()
            .Build();
    message_filter_ = new DataReductionProxyMessageFilter(
        test_context_->settings());
  }

 protected:
  DataReductionProxyMessageFilter* message_filter() const {
    return message_filter_.get();
  }

  MockDataReductionProxyConfig* config() const {
    return test_context_->mock_config();
  }

 private:
  base::MessageLoopForIO message_loop_;
  scoped_ptr<DataReductionProxyTestContext> test_context_;
  scoped_refptr<DataReductionProxyMessageFilter> message_filter_;
};

TEST_F(DataReductionProxyMessageFilterTest, TestOnDataReductionProxyStatus) {
  net::HostPortPair proxy_server =
      net::HostPortPair::FromString("www.google.com:443");

  const bool enable_lofi_on_switch[] = {false, true};

  const struct {
    bool is_data_reduction_proxy;
    bool auto_lofi_enabled_field_trial_group;
    bool auto_lofi_control_field_trial_group;
    bool is_network_bad;
    bool expected_is_data_reduction_proxy;
    AutoLoFiStatus expected_auto_lofi_status;

  } tests[] = {
      {
       // In Enabled field trial group and the network is bad.
       true,
       true,
       false,
       true,
       true,
       AUTO_LOFI_STATUS_ON,
      },
      {
       // In Enabled field trial group but the network is not bad.
       true,
       true,
       false,
       false,
       true,
       AUTO_LOFI_STATUS_DISABLED,
      },
      {
       // In Control field trial group and the network is bad.
       true,
       false,
       true,
       true,
       true,
       AUTO_LOFI_STATUS_OFF,
      },
      {
       // In Control field trial group but the network is not bad.
       true,
       false,
       true,
       false,
       true,
       AUTO_LOFI_STATUS_DISABLED,
      },
      {
       // Not a data reduction proxy server.
       false,
       true,
       false,
       true,
       false,
       AUTO_LOFI_STATUS_DISABLED,
      },
  };

  for (size_t i = 0; i < arraysize(enable_lofi_on_switch); ++i) {
    if (enable_lofi_on_switch[i]) {
      // Enabling LoFi on switch should have no effect on Auto LoFi.
      base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
      command_line->AppendSwitch(
          data_reduction_proxy::switches::kEnableDataReductionProxyLoFi);
    }

    for (size_t j = 0; j < arraysize(tests); ++j) {
      bool is_data_reduction_proxy = false;

      EXPECT_CALL(*config(), IsDataReductionProxy(testing::_, nullptr))
          .WillOnce(testing::Return(tests[j].is_data_reduction_proxy));
      EXPECT_CALL(*config(), IsNetworkBad())
          .WillRepeatedly(testing::Return(tests[j].is_network_bad));
      EXPECT_CALL(*config(), IsIncludedInLoFiEnabledFieldTrial())
          .WillRepeatedly(
              testing::Return(tests[j].auto_lofi_enabled_field_trial_group));
      EXPECT_CALL(*config(), IsIncludedInLoFiControlFieldTrial())
          .WillRepeatedly(
              testing::Return(tests[j].auto_lofi_control_field_trial_group));
      enum AutoLoFiStatus auto_lofi_status;
      message_filter()->OnDataReductionProxyStatus(
          proxy_server, &is_data_reduction_proxy, &auto_lofi_status);
      EXPECT_EQ(is_data_reduction_proxy,
                tests[j].expected_is_data_reduction_proxy)
          << i << j;
      EXPECT_EQ(auto_lofi_status, tests[j].expected_auto_lofi_status) << i << j;
    }
  }
}

}  // namespace data_reduction_proxy
