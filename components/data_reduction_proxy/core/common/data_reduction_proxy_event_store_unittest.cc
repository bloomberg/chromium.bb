// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/common/data_reduction_proxy_event_store.h"

#include "base/bind.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/test_simple_task_runner.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params_test_utils.h"
#include "net/base/capturing_net_log.h"
#include "net/base/net_log.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace data_reduction_proxy {

class DataReductionProxyEventStoreTest : public testing::Test {
 public:
  DataReductionProxyEventStoreTest()
      : task_runner_(scoped_refptr<base::TestSimpleTaskRunner>(
                         new base::TestSimpleTaskRunner())),
        net_log_(new net::CapturingNetLog()) {
    bound_net_log_ = net::BoundNetLog::Make(
        net_log_.get(), net::NetLog::SOURCE_DATA_REDUCTION_PROXY);
  }

  void SetUp() override {
    proxy_.reset(new DataReductionProxyEventStore(task_runner_));
  }

  net::CapturingNetLog::CapturedEntry GetSingleEntry() const {
    net::CapturingNetLog::CapturedEntryList entries;
    net_log_->GetEntries(&entries);
    EXPECT_EQ(1u, entries.size());
    return entries[0];
  }

  DataReductionProxyEventStore* proxy() {
    return proxy_.get();
  }

  base::TestSimpleTaskRunner* task_runner() {
    return task_runner_.get();
  }

  net::CapturingNetLog* net_log() {
    return net_log_.get();
  }

  const net::BoundNetLog& bound_net_log() {
    return bound_net_log_;
  }

 private:
  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
  scoped_ptr<net::CapturingNetLog> net_log_;
  scoped_ptr<DataReductionProxyEventStore> proxy_;
  net::BoundNetLog bound_net_log_;
};

TEST_F(DataReductionProxyEventStoreTest, TestAddProxyEnabledEvent) {
  EXPECT_EQ(0u, proxy()->stored_events_.size());
  proxy()->AddProxyEnabledEvent(
                                net_log(), false, false,
      TestDataReductionProxyParams::DefaultOrigin(),
      TestDataReductionProxyParams::DefaultFallbackOrigin(),
      TestDataReductionProxyParams::DefaultSSLOrigin());
  task_runner()->RunPendingTasks();
  EXPECT_EQ(1u, proxy()->stored_events_.size());
  net::CapturingNetLog::CapturedEntry entry = GetSingleEntry();
  EXPECT_EQ(net::NetLog::TYPE_DATA_REDUCTION_PROXY_ENABLED,
            entry.type);
}

TEST_F(DataReductionProxyEventStoreTest, TestAddProxyDisabledEvent) {
  EXPECT_EQ(0u, proxy()->stored_events_.size());
  proxy()->AddProxyDisabledEvent(net_log());
  task_runner()->RunPendingTasks();
  EXPECT_EQ(1u, proxy()->stored_events_.size());
  net::CapturingNetLog::CapturedEntry entry = GetSingleEntry();
  EXPECT_EQ(net::NetLog::TYPE_DATA_REDUCTION_PROXY_ENABLED,
            entry.type);
}

TEST_F(DataReductionProxyEventStoreTest, TestAddBypassActionEvent) {
  EXPECT_EQ(0u, proxy()->stored_events_.size());
  EXPECT_EQ(nullptr, proxy()->last_bypass_event_.get());
  proxy()->AddBypassActionEvent(bound_net_log(), "bypass", GURL(),
                                base::TimeDelta::FromMinutes(1));
  task_runner()->RunPendingTasks();
  EXPECT_EQ(1u, proxy()->stored_events_.size());
  net::CapturingNetLog::CapturedEntry entry = GetSingleEntry();
  EXPECT_EQ(net::NetLog::TYPE_DATA_REDUCTION_PROXY_BYPASS_REQUESTED,
            entry.type);
  EXPECT_NE(nullptr, proxy()->last_bypass_event_.get());
}

TEST_F(DataReductionProxyEventStoreTest, TestAddBypassTypeEvent) {
  EXPECT_EQ(0u, proxy()->stored_events_.size());
  EXPECT_EQ(nullptr, proxy()->last_bypass_event_.get());
  proxy()->AddBypassTypeEvent(bound_net_log(), BYPASS_EVENT_TYPE_LONG, GURL(),
                              base::TimeDelta::FromMinutes(1));
  task_runner()->RunPendingTasks();
  EXPECT_EQ(1u, proxy()->stored_events_.size());
  EXPECT_EQ(1u, net_log()->GetSize());
  net::CapturingNetLog::CapturedEntry entry = GetSingleEntry();
  EXPECT_EQ(net::NetLog::TYPE_DATA_REDUCTION_PROXY_BYPASS_REQUESTED,
            entry.type);
  EXPECT_NE(nullptr, proxy()->last_bypass_event_.get());
}

TEST_F(DataReductionProxyEventStoreTest, TestBeginSecureProxyCheck) {
  EXPECT_EQ(0u, proxy()->stored_events_.size());
  EXPECT_EQ(CHECK_UNKNOWN, proxy()->secure_proxy_check_state_);
  proxy()->BeginSecureProxyCheck(bound_net_log(), GURL());
  task_runner()->RunPendingTasks();
  EXPECT_EQ(1u, proxy()->stored_events_.size());
  EXPECT_EQ(1u, net_log()->GetSize());
  net::CapturingNetLog::CapturedEntry entry = GetSingleEntry();
  EXPECT_EQ(net::NetLog::TYPE_DATA_REDUCTION_PROXY_CANARY_REQUEST,
            entry.type);
  EXPECT_EQ(CHECK_PENDING, proxy()->secure_proxy_check_state_);
}

TEST_F(DataReductionProxyEventStoreTest, TestEndSecureProxyCheck) {
  EXPECT_EQ(0u, proxy()->stored_events_.size());
  EXPECT_EQ(CHECK_UNKNOWN, proxy()->secure_proxy_check_state_);
  proxy()->EndSecureProxyCheck(bound_net_log(), 0);
  task_runner()->RunPendingTasks();
  EXPECT_EQ(1u, proxy()->stored_events_.size());
  EXPECT_EQ(1u, net_log()->GetSize());
  net::CapturingNetLog::CapturedEntry entry = GetSingleEntry();
  EXPECT_EQ(net::NetLog::TYPE_DATA_REDUCTION_PROXY_CANARY_REQUEST,
            entry.type);
  EXPECT_EQ(CHECK_SUCCESS, proxy()->secure_proxy_check_state_);
}

}  // namespace data_reduction_proxy
