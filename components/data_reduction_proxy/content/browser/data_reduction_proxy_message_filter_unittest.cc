// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/content/browser/data_reduction_proxy_message_filter.h"

#include "base/command_line.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_config.h"
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

  const struct {
    bool lofi_on_through_switch;
    bool data_reduction_proxy_used;
    bool auto_lofi_enabled_field_trial_group;
    bool auto_lofi_control_field_trial_group;
    bool network_quality_prohibitively_slow;
    bool expected_data_reduction_proxy;
    LoFiStatus expected_lofi_status;

  } tests[] = {
      {
       // In Enabled field trial group and the network is prohibitively slow.
       false,
       true,
       true,
       false,
       true,
       true,
       LOFI_STATUS_ACTIVE,
      },
      {
       // In Enabled field trial group but the network is not prohibitively
       // slow.
       false,
       true,
       true,
       false,
       false,
       true,
       LOFI_STATUS_INACTIVE,
      },
      {
       // In Control field trial group and the network is prohibitively slow.
       false,
       true,
       false,
       true,
       true,
       true,
       LOFI_STATUS_ACTIVE_CONTROL,
      },
      {
       // In Control field trial group but the network is not prohibitively
       // slow.
       false,
       true,
       false,
       true,
       false,
       true,
       LOFI_STATUS_INACTIVE_CONTROL,
      },
      {
       // Not a data reduction proxy server.
       false,
       false,
       true,
       false,
       true,
       false,
       LOFI_STATUS_TEMPORARILY_OFF,
      },
      {
       // Enabled through command line switch.
       true,
       true,
       true,
       false,
       true,
       true,
       LOFI_STATUS_ACTIVE_FROM_FLAGS,
      },
  };

  for (size_t i = 0; i < arraysize(tests); ++i) {
    bool is_data_reduction_proxy = false;

    if (tests[i].lofi_on_through_switch) {
      base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
          switches::kDataReductionProxyLoFi,
          switches::kDataReductionProxyLoFiValueAlwaysOn);
    }

    EXPECT_CALL(*config(), IsDataReductionProxy(testing::_, nullptr))
        .WillOnce(testing::Return(tests[i].data_reduction_proxy_used));

    EXPECT_CALL(*config(), IsNetworkQualityProhibitivelySlow(testing::_))
        .WillRepeatedly(
            testing::Return(tests[i].network_quality_prohibitively_slow));

    EXPECT_CALL(*config(), IsIncludedInLoFiEnabledFieldTrial())
        .WillRepeatedly(
            testing::Return(tests[i].auto_lofi_enabled_field_trial_group));
    EXPECT_CALL(*config(), IsIncludedInLoFiControlFieldTrial())
        .WillRepeatedly(
            testing::Return(tests[i].auto_lofi_control_field_trial_group));
    config()->UpdateLoFiStatusOnMainFrameRequest(false, nullptr);
    LoFiStatus lofi_status;
    message_filter()->OnDataReductionProxyStatus(
        proxy_server, &is_data_reduction_proxy, &lofi_status);
    EXPECT_EQ(tests[i].expected_data_reduction_proxy, is_data_reduction_proxy)
        << i;
    EXPECT_EQ(tests[i].expected_lofi_status, lofi_status) << i;
  }
}

}  // namespace data_reduction_proxy
