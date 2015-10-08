// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_usage/core/data_use_aggregator.h"

#include <stdint.h>

#include <vector>

#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "components/data_usage/core/data_use.h"
#include "net/base/network_change_notifier.h"
#include "net/base/network_delegate_impl.h"
#include "net/socket/socket_test_util.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace data_usage {

namespace {

// Override NetworkChangeNotifier to simulate connection type changes for tests.
class TestNetworkChangeNotifier : public net::NetworkChangeNotifier {
 public:
  TestNetworkChangeNotifier()
      : net::NetworkChangeNotifier(),
        connection_type_to_return_(
            net::NetworkChangeNotifier::CONNECTION_UNKNOWN) {}

  // Simulates a change of the connection type to |type|.
  void SimulateNetworkConnectionChange(ConnectionType type) {
    connection_type_to_return_ = type;
  }

 private:
  ConnectionType GetCurrentConnectionType() const override {
    return connection_type_to_return_;
  }

  // The currently simulated network connection type.
  ConnectionType connection_type_to_return_;

  DISALLOW_COPY_AND_ASSIGN(TestNetworkChangeNotifier);
};

// A network delegate that reports all received and sent network bytes to a
// DataUseAggregator.
class ReportingNetworkDelegate : public net::NetworkDelegateImpl {
 public:
  explicit ReportingNetworkDelegate(DataUseAggregator* data_use_aggregator)
      : data_use_aggregator_(data_use_aggregator), tab_id_for_requests_(-1) {}

  ~ReportingNetworkDelegate() override {}

  void set_tab_id_for_requests(int32_t tab_id_for_requests) {
    tab_id_for_requests_ = tab_id_for_requests;
  }

 private:
  void OnNetworkBytesReceived(const net::URLRequest& request,
                              int64_t bytes_received) override {
    data_use_aggregator_->ReportDataUse(request, tab_id_for_requests_,
                                        0 /* tx_bytes */, bytes_received);
  }

  void OnNetworkBytesSent(const net::URLRequest& request,
                          int64_t bytes_sent) override {
    data_use_aggregator_->ReportDataUse(request, tab_id_for_requests_,
                                        bytes_sent, 0 /* rx_bytes */);
  }

  DataUseAggregator* data_use_aggregator_;
  int32_t tab_id_for_requests_;

  DISALLOW_COPY_AND_ASSIGN(ReportingNetworkDelegate);
};

// An observer that keeps track of all the data use it observed.
class TestObserver : public DataUseAggregator::Observer {
 public:
  explicit TestObserver(DataUseAggregator* data_use_aggregator)
      : data_use_aggregator_(data_use_aggregator) {
    data_use_aggregator_->AddObserver(this);
  }

  ~TestObserver() override { data_use_aggregator_->RemoveObserver(this); }

  void OnDataUse(const std::vector<DataUse>& data_use_sequence) override {
    observed_data_use_.insert(observed_data_use_.end(),
                              data_use_sequence.begin(),
                              data_use_sequence.end());
  }

  const std::vector<DataUse>& observed_data_use() const {
    return observed_data_use_;
  }

 private:
  DataUseAggregator* data_use_aggregator_;
  std::vector<DataUse> observed_data_use_;

  DISALLOW_COPY_AND_ASSIGN(TestObserver);
};

class DataUseAggregatorTest : public testing::Test {
 public:
  DataUseAggregatorTest()
      : reporting_network_delegate_(&data_use_aggregator_),
        context_(true),
        test_observer_(&data_use_aggregator_) {
    context_.set_client_socket_factory(&mock_socket_factory_);
    context_.set_network_delegate(&reporting_network_delegate_);
    context_.Init();
  }

  ~DataUseAggregatorTest() override {}

  scoped_ptr<net::URLRequest> ExecuteRequest(
      const GURL& url,
      const GURL& first_party_for_cookies) {
    net::MockRead reads[] = {
        net::MockRead("HTTP/1.1 200 OK\r\n\r\n"), net::MockRead("hello world"),
        net::MockRead(net::SYNCHRONOUS, net::OK),
    };
    net::StaticSocketDataProvider socket(reads, arraysize(reads), nullptr, 0);
    mock_socket_factory_.AddSocketDataProvider(&socket);

    net::TestDelegate delegate;
    scoped_ptr<net::URLRequest> request =
        context_.CreateRequest(url, net::IDLE, &delegate);
    request->set_first_party_for_cookies(first_party_for_cookies);
    request->Start();
    loop_.RunUntilIdle();

    return request.Pass();
  }

  void SimulateNetworkConnectionChange(
      net::NetworkChangeNotifier::ConnectionType connection_type) {
    test_network_change_notifier_.SimulateNetworkConnectionChange(
        connection_type);
  }

  void set_tab_id_for_requests(int32_t tab_id_for_requests) {
    reporting_network_delegate_.set_tab_id_for_requests(tab_id_for_requests);
  }

  DataUseAggregator* data_use_aggregator() { return &data_use_aggregator_; }

  const std::vector<DataUse>& observed_data_use() const {
    return test_observer_.observed_data_use();
  }

 private:
  base::MessageLoopForIO loop_;
  TestNetworkChangeNotifier test_network_change_notifier_;

  DataUseAggregator data_use_aggregator_;
  net::MockClientSocketFactory mock_socket_factory_;
  ReportingNetworkDelegate reporting_network_delegate_;
  net::TestURLRequestContext context_;
  TestObserver test_observer_;

  DISALLOW_COPY_AND_ASSIGN(DataUseAggregatorTest);
};

TEST_F(DataUseAggregatorTest, ReportDataUse) {
  const net::NetworkChangeNotifier::ConnectionType kFooConnectionType =
      net::NetworkChangeNotifier::CONNECTION_2G;
  SimulateNetworkConnectionChange(kFooConnectionType);
  const int32_t kFooTabId = 10;
  set_tab_id_for_requests(kFooTabId);
  const scoped_ptr<net::URLRequest> foo_request =
      ExecuteRequest(GURL("http://foo.com"), GURL("http://foofirstparty.com"));

  const net::NetworkChangeNotifier::ConnectionType kBarConnectionType =
      net::NetworkChangeNotifier::CONNECTION_WIFI;
  SimulateNetworkConnectionChange(kBarConnectionType);
  const int32_t kBarTabId = 20;
  set_tab_id_for_requests(kBarTabId);
  scoped_ptr<net::URLRequest> bar_request =
      ExecuteRequest(GURL("http://bar.com"), GURL("http://barfirstparty.com"));

  auto data_use_it = observed_data_use().begin();

  // First, the |foo_request| data use should have happened.
  int64_t observed_foo_tx_bytes = 0, observed_foo_rx_bytes = 0;
  while (data_use_it != observed_data_use().end() &&
         data_use_it->url == GURL("http://foo.com")) {
    EXPECT_EQ(foo_request->request_time(), data_use_it->request_time);
    EXPECT_EQ(GURL("http://foofirstparty.com"),
              data_use_it->first_party_for_cookies);
    EXPECT_EQ(kFooTabId, data_use_it->tab_id);
    EXPECT_EQ(kFooConnectionType, data_use_it->connection_type);

    observed_foo_tx_bytes += data_use_it->tx_bytes;
    observed_foo_rx_bytes += data_use_it->rx_bytes;
    ++data_use_it;
  }
  EXPECT_EQ(foo_request->GetTotalSentBytes(), observed_foo_tx_bytes);
  EXPECT_EQ(foo_request->GetTotalReceivedBytes(), observed_foo_rx_bytes);

  // Then, the |bar_request| data use should have happened.
  int64_t observed_bar_tx_bytes = 0, observed_bar_rx_bytes = 0;
  while (data_use_it != observed_data_use().end()) {
    EXPECT_EQ(GURL("http://bar.com"), data_use_it->url);
    EXPECT_EQ(bar_request->request_time(), data_use_it->request_time);
    EXPECT_EQ(GURL("http://barfirstparty.com"),
              data_use_it->first_party_for_cookies);
    EXPECT_EQ(kBarTabId, data_use_it->tab_id);
    EXPECT_EQ(kBarConnectionType, data_use_it->connection_type);

    observed_bar_tx_bytes += data_use_it->tx_bytes;
    observed_bar_rx_bytes += data_use_it->rx_bytes;
    ++data_use_it;
  }
  EXPECT_EQ(bar_request->GetTotalSentBytes(), observed_bar_tx_bytes);
  EXPECT_EQ(bar_request->GetTotalReceivedBytes(), observed_bar_rx_bytes);
}

TEST_F(DataUseAggregatorTest, ReportOffTheRecordDataUse) {
  // Off the record data use should not be reported to observers.
  data_use_aggregator()->ReportOffTheRecordDataUse(1000, 1000);
  base::MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(static_cast<size_t>(0), observed_data_use().size());
}

}  // namespace

}  // namespace data_usage
