// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/common/data_reduction_proxy_event_store.h"

#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_event_creator.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params_test_utils.h"
#include "net/http/http_status_code.h"
#include "net/log/net_log.h"
#include "net/log/test_net_log.h"
#include "net/log/test_net_log_entry.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace data_reduction_proxy {

class DataReductionProxyEventStoreTest : public testing::Test {
 public:
  DataReductionProxyEventStoreTest() : net_log_(new net::TestNetLog()) {
    bound_net_log_ = net::BoundNetLog::Make(
        net_log_.get(), net::NetLog::SOURCE_DATA_REDUCTION_PROXY);
  }

  void SetUp() override {
    event_store_.reset(new DataReductionProxyEventStore());
    event_creator_.reset(
        new DataReductionProxyEventCreator(event_store_.get()));
  }

  net::TestNetLogEntry GetSingleEntry() const {
    net::TestNetLogEntry::List entries;
    net_log_->GetEntries(&entries);
    EXPECT_EQ(1u, entries.size());
    return entries[0];
  }

  DataReductionProxyEventStore* event_store() { return event_store_.get(); }

  DataReductionProxyEventCreator* event_creator() {
    return event_creator_.get();
  }

  net::TestNetLog* net_log() { return net_log_.get(); }

  const net::BoundNetLog& bound_net_log() {
    return bound_net_log_;
  }

 private:
  scoped_ptr<net::TestNetLog> net_log_;
  scoped_ptr<DataReductionProxyEventStore> event_store_;
  scoped_ptr<DataReductionProxyEventCreator> event_creator_;
  net::BoundNetLog bound_net_log_;
};

TEST_F(DataReductionProxyEventStoreTest, TestAddProxyEnabledEvent) {
  EXPECT_EQ(0u, event_store()->stored_events_.size());
  event_creator()->AddProxyEnabledEvent(
      net_log(), false, false, TestDataReductionProxyParams::DefaultOrigin(),
      TestDataReductionProxyParams::DefaultFallbackOrigin(),
      TestDataReductionProxyParams::DefaultSSLOrigin());
  EXPECT_EQ(1u, event_store()->stored_events_.size());
  net::TestNetLogEntry entry = GetSingleEntry();
  EXPECT_EQ(net::NetLog::TYPE_DATA_REDUCTION_PROXY_ENABLED,
            entry.type);
}

TEST_F(DataReductionProxyEventStoreTest, TestAddProxyDisabledEvent) {
  EXPECT_EQ(0u, event_store()->stored_events_.size());
  event_creator()->AddProxyDisabledEvent(net_log());
  EXPECT_EQ(1u, event_store()->stored_events_.size());
  net::TestNetLogEntry entry = GetSingleEntry();
  EXPECT_EQ(net::NetLog::TYPE_DATA_REDUCTION_PROXY_ENABLED,
            entry.type);
}

TEST_F(DataReductionProxyEventStoreTest, TestAddBypassActionEvent) {
  EXPECT_EQ(0u, event_store()->stored_events_.size());
  EXPECT_EQ(nullptr, event_store()->last_bypass_event_.get());
  event_creator()->AddBypassActionEvent(
      bound_net_log(), BYPASS_ACTION_TYPE_BYPASS, "GET", GURL(), false,
      base::TimeDelta::FromMinutes(1));
  EXPECT_EQ(1u, event_store()->stored_events_.size());
  net::TestNetLogEntry entry = GetSingleEntry();
  EXPECT_EQ(net::NetLog::TYPE_DATA_REDUCTION_PROXY_BYPASS_REQUESTED,
            entry.type);
  EXPECT_NE(nullptr, event_store()->last_bypass_event_.get());
}

TEST_F(DataReductionProxyEventStoreTest, TestAddBypassTypeEvent) {
  EXPECT_EQ(0u, event_store()->stored_events_.size());
  EXPECT_EQ(nullptr, event_store()->last_bypass_event_.get());
  event_creator()->AddBypassTypeEvent(bound_net_log(), BYPASS_EVENT_TYPE_LONG,
                                      "GET", GURL(), false,
                                      base::TimeDelta::FromMinutes(1));
  EXPECT_EQ(1u, event_store()->stored_events_.size());
  EXPECT_EQ(1u, net_log()->GetSize());
  net::TestNetLogEntry entry = GetSingleEntry();
  EXPECT_EQ(net::NetLog::TYPE_DATA_REDUCTION_PROXY_BYPASS_REQUESTED,
            entry.type);
  EXPECT_NE(nullptr, event_store()->last_bypass_event_.get());
}

TEST_F(DataReductionProxyEventStoreTest, TestBeginSecureProxyCheck) {
  EXPECT_EQ(0u, event_store()->stored_events_.size());
  EXPECT_EQ(DataReductionProxyEventStorageDelegate::CHECK_UNKNOWN,
            event_store()->secure_proxy_check_state_);
  event_creator()->BeginSecureProxyCheck(bound_net_log(), GURL());
  EXPECT_EQ(1u, event_store()->stored_events_.size());
  EXPECT_EQ(1u, net_log()->GetSize());
  net::TestNetLogEntry entry = GetSingleEntry();
  EXPECT_EQ(net::NetLog::TYPE_DATA_REDUCTION_PROXY_CANARY_REQUEST,
            entry.type);
  EXPECT_EQ(DataReductionProxyEventStorageDelegate::CHECK_PENDING,
            event_store()->secure_proxy_check_state_);
}

TEST_F(DataReductionProxyEventStoreTest, TestEndSecureProxyCheck) {
  EXPECT_EQ(0u, event_store()->stored_events_.size());
  EXPECT_EQ(DataReductionProxyEventStorageDelegate::CHECK_UNKNOWN,
            event_store()->secure_proxy_check_state_);
  event_creator()->EndSecureProxyCheck(bound_net_log(), 0, net::HTTP_OK, true);
  EXPECT_EQ(1u, event_store()->stored_events_.size());
  EXPECT_EQ(1u, net_log()->GetSize());
  net::TestNetLogEntry entry = GetSingleEntry();
  EXPECT_EQ(net::NetLog::TYPE_DATA_REDUCTION_PROXY_CANARY_REQUEST,
            entry.type);
  EXPECT_EQ(DataReductionProxyEventStorageDelegate::CHECK_SUCCESS,
            event_store()->secure_proxy_check_state_);
}

}  // namespace data_reduction_proxy
