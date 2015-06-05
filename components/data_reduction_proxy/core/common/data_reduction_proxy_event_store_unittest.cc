// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/common/data_reduction_proxy_event_store.h"

#include <vector>

#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_event_creator.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params_test_utils.h"
#include "net/base/net_errors.h"
#include "net/http/http_status_code.h"
#include "net/log/net_log.h"
#include "net/log/test_net_log.h"
#include "net/log/test_net_log_entry.h"
#include "net/proxy/proxy_server.h"
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

  net::TestNetLogEntry GetLatestEntry() const {
    net::TestNetLogEntry::List entries;
    net_log_->GetEntries(&entries);
    return entries.back();
  }

  DataReductionProxyEventStore* event_store() { return event_store_.get(); }

  DataReductionProxyEventCreator* event_creator() {
    return event_creator_.get();
  }

  net::TestNetLog* net_log() { return net_log_.get(); }

  const net::BoundNetLog& bound_net_log() {
    return bound_net_log_;
  }

  size_t event_count() const { return event_store_->stored_events_.size(); }

  DataReductionProxyEventStorageDelegate::SecureProxyCheckState
  secure_proxy_check_state() const {
    return event_store_->secure_proxy_check_state_;
  }

  base::Value* last_bypass_event() const {
    return event_store_->last_bypass_event_.get();
  }

 private:
  scoped_ptr<net::TestNetLog> net_log_;
  scoped_ptr<DataReductionProxyEventStore> event_store_;
  scoped_ptr<DataReductionProxyEventCreator> event_creator_;
  net::BoundNetLog bound_net_log_;
};

TEST_F(DataReductionProxyEventStoreTest, TestAddProxyEnabledEvent) {
  EXPECT_EQ(0u, event_count());
  std::vector<net::ProxyServer> proxies_for_http;
  std::vector<net::ProxyServer> proxies_for_https;
  event_creator()->AddProxyEnabledEvent(net_log(), false, proxies_for_http,
                                        proxies_for_https);
  EXPECT_EQ(1u, event_count());
  net::TestNetLogEntry entry = GetSingleEntry();
  EXPECT_EQ(net::NetLog::TYPE_DATA_REDUCTION_PROXY_ENABLED,
            entry.type);
}

TEST_F(DataReductionProxyEventStoreTest, TestAddProxyDisabledEvent) {
  EXPECT_EQ(0u, event_count());
  event_creator()->AddProxyDisabledEvent(net_log());
  EXPECT_EQ(1u, event_count());
  net::TestNetLogEntry entry = GetSingleEntry();
  EXPECT_EQ(net::NetLog::TYPE_DATA_REDUCTION_PROXY_ENABLED,
            entry.type);
}

TEST_F(DataReductionProxyEventStoreTest, TestAddBypassActionEvent) {
  EXPECT_EQ(0u, event_count());
  EXPECT_EQ(nullptr, last_bypass_event());
  event_creator()->AddBypassActionEvent(
      bound_net_log(), BYPASS_ACTION_TYPE_BYPASS, "GET", GURL(), false,
      base::TimeDelta::FromMinutes(1));
  EXPECT_EQ(1u, event_count());
  net::TestNetLogEntry entry = GetSingleEntry();
  EXPECT_EQ(net::NetLog::TYPE_DATA_REDUCTION_PROXY_BYPASS_REQUESTED,
            entry.type);
  EXPECT_NE(nullptr, last_bypass_event());
}

TEST_F(DataReductionProxyEventStoreTest, TestAddBypassTypeEvent) {
  EXPECT_EQ(0u, event_count());
  EXPECT_EQ(nullptr, last_bypass_event());
  event_creator()->AddBypassTypeEvent(bound_net_log(), BYPASS_EVENT_TYPE_LONG,
                                      "GET", GURL(), false,
                                      base::TimeDelta::FromMinutes(1));
  EXPECT_EQ(1u, event_count());
  EXPECT_EQ(1u, net_log()->GetSize());
  net::TestNetLogEntry entry = GetSingleEntry();
  EXPECT_EQ(net::NetLog::TYPE_DATA_REDUCTION_PROXY_BYPASS_REQUESTED,
            entry.type);
  EXPECT_NE(nullptr, last_bypass_event());
}

TEST_F(DataReductionProxyEventStoreTest, TestAddProxyFallbackEvent) {
  EXPECT_EQ(0u, event_count());
  event_creator()->AddProxyFallbackEvent(net_log(), "bad_proxy",
                                         net::ERR_PROXY_CONNECTION_FAILED);
  EXPECT_EQ(1u, event_count());
  net::TestNetLogEntry entry = GetSingleEntry();
  EXPECT_EQ(net::NetLog::TYPE_DATA_REDUCTION_PROXY_FALLBACK, entry.type);
}

TEST_F(DataReductionProxyEventStoreTest, TestBeginSecureProxyCheck) {
  EXPECT_EQ(0u, event_count());
  EXPECT_EQ(DataReductionProxyEventStorageDelegate::CHECK_UNKNOWN,
            secure_proxy_check_state());
  event_creator()->BeginSecureProxyCheck(bound_net_log(), GURL());
  EXPECT_EQ(1u, event_count());
  EXPECT_EQ(1u, net_log()->GetSize());
  net::TestNetLogEntry entry = GetSingleEntry();
  EXPECT_EQ(net::NetLog::TYPE_DATA_REDUCTION_PROXY_CANARY_REQUEST,
            entry.type);
  EXPECT_EQ(DataReductionProxyEventStorageDelegate::CHECK_PENDING,
            secure_proxy_check_state());
}

TEST_F(DataReductionProxyEventStoreTest, TestEndSecureProxyCheck) {
  EXPECT_EQ(0u, event_count());
  EXPECT_EQ(DataReductionProxyEventStorageDelegate::CHECK_UNKNOWN,
            secure_proxy_check_state());
  event_creator()->EndSecureProxyCheck(bound_net_log(), 0, net::HTTP_OK, true);
  EXPECT_EQ(1u, event_count());
  EXPECT_EQ(1u, net_log()->GetSize());
  net::TestNetLogEntry entry = GetSingleEntry();
  EXPECT_EQ(net::NetLog::TYPE_DATA_REDUCTION_PROXY_CANARY_REQUEST,
            entry.type);
  EXPECT_EQ(DataReductionProxyEventStorageDelegate::CHECK_SUCCESS,
            secure_proxy_check_state());
}

TEST_F(DataReductionProxyEventStoreTest, TestEndSecureProxyCheckFailed) {
  const struct {
    const char* test_case;
    int net_error;
    int http_response_code;
    bool secure_proxy_check_succeeded;
    bool expected_proxy_check_succeeded;
  } tests[] = {
      {
       "Succeeded", net::OK, net::HTTP_OK, true, true,
      },
      {
       "Net failure", net::ERR_CONNECTION_RESET, -1, false, false,
      },
      {
       "HTTP failure", net::OK, net::HTTP_NOT_FOUND, false, false,
      },
      {
       "Bad content", net::OK, net::HTTP_OK, false, false,
      },
  };
  size_t expected_event_count = 0;
  EXPECT_EQ(expected_event_count, net_log()->GetSize());
  EXPECT_EQ(expected_event_count++, event_count());
  EXPECT_EQ(DataReductionProxyEventStorageDelegate::CHECK_UNKNOWN,
            secure_proxy_check_state());
  for (const auto& test : tests) {
    event_creator()->EndSecureProxyCheck(bound_net_log(), test.net_error,
                                         test.http_response_code,
                                         test.secure_proxy_check_succeeded);
    EXPECT_EQ(expected_event_count, net_log()->GetSize()) << test.test_case;
    EXPECT_EQ(expected_event_count++, event_count()) << test.test_case;
    net::TestNetLogEntry entry = GetLatestEntry();
    EXPECT_EQ(net::NetLog::TYPE_DATA_REDUCTION_PROXY_CANARY_REQUEST, entry.type)
        << test.test_case;
    EXPECT_EQ(test.secure_proxy_check_succeeded
                  ? DataReductionProxyEventStorageDelegate::CHECK_SUCCESS
                  : DataReductionProxyEventStorageDelegate::CHECK_FAILED,
              secure_proxy_check_state())
        << test.test_case;
  }
}

}  // namespace data_reduction_proxy
