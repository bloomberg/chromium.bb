// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_usage/core/data_use_aggregator.h"

#include <stdint.h>

#include <map>
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

  ConnectionType GetCurrentConnectionType() const override {
    return connection_type_to_return_;
  }

 private:
  // The currently simulated network connection type.
  ConnectionType connection_type_to_return_;

  DISALLOW_COPY_AND_ASSIGN(TestNetworkChangeNotifier);
};

// A network delegate that reports all received and sent network bytes to a
// DataUseAggregator.
class ReportingNetworkDelegate : public net::NetworkDelegateImpl {
 public:
  // The simulated context for the data usage of a net::URLRequest.
  struct DataUseContext {
    DataUseContext()
        : tab_id(-1),
          connection_type(net::NetworkChangeNotifier::CONNECTION_UNKNOWN) {}

    DataUseContext(int32_t tab_id,
                   net::NetworkChangeNotifier::ConnectionType connection_type)
        : tab_id(tab_id), connection_type(connection_type) {}

    int32_t tab_id;
    net::NetworkChangeNotifier::ConnectionType connection_type;
  };

  typedef std::map<const net::URLRequest*, DataUseContext> DataUseContextMap;

  ReportingNetworkDelegate(
      DataUseAggregator* data_use_aggregator,
      TestNetworkChangeNotifier* test_network_change_notifier)
      : data_use_aggregator_(data_use_aggregator),
        test_network_change_notifier_(test_network_change_notifier) {}

  ~ReportingNetworkDelegate() override {}

  void set_data_use_context_map(const DataUseContextMap& data_use_context_map) {
    data_use_context_map_ = data_use_context_map;
  }

 private:
  DataUseContext UpdateDataUseContext(const net::URLRequest& request) {
    DataUseContextMap::const_iterator data_use_context_it =
        data_use_context_map_.find(&request);
    DataUseContext data_use_context =
        data_use_context_it == data_use_context_map_.end()
            ? DataUseContext()
            : data_use_context_it->second;

    if (test_network_change_notifier_->GetCurrentConnectionType() !=
        data_use_context.connection_type) {
      test_network_change_notifier_->SimulateNetworkConnectionChange(
          data_use_context.connection_type);
    }
    return data_use_context;
  }

  void OnNetworkBytesReceived(const net::URLRequest& request,
                              int64_t bytes_received) override {
    DataUseContext data_use_context = UpdateDataUseContext(request);
    data_use_aggregator_->ReportDataUse(request, data_use_context.tab_id,
                                        0 /* tx_bytes */, bytes_received);
  }

  void OnNetworkBytesSent(const net::URLRequest& request,
                          int64_t bytes_sent) override {
    DataUseContext data_use_context = UpdateDataUseContext(request);
    data_use_aggregator_->ReportDataUse(request, data_use_context.tab_id,
                                        bytes_sent, 0 /* rx_bytes */);
  }

  DataUseAggregator* data_use_aggregator_;
  TestNetworkChangeNotifier* test_network_change_notifier_;
  DataUseContextMap data_use_context_map_;

  DISALLOW_COPY_AND_ASSIGN(ReportingNetworkDelegate);
};

// An observer that keeps track of all the data use it observed.
class TestObserver : public DataUseAggregator::Observer {
 public:
  explicit TestObserver(DataUseAggregator* data_use_aggregator)
      : data_use_aggregator_(data_use_aggregator),
        on_data_use_called_count_(0) {
    data_use_aggregator_->AddObserver(this);
  }

  ~TestObserver() override { data_use_aggregator_->RemoveObserver(this); }

  void OnDataUse(const std::vector<DataUse>& data_use_sequence) override {
    ++on_data_use_called_count_;
    observed_data_use_.insert(observed_data_use_.end(),
                              data_use_sequence.begin(),
                              data_use_sequence.end());
  }

  const std::vector<DataUse>& observed_data_use() const {
    return observed_data_use_;
  }

  int on_data_use_called_count() const { return on_data_use_called_count_; }

 private:
  DataUseAggregator* data_use_aggregator_;
  std::vector<DataUse> observed_data_use_;
  int on_data_use_called_count_;

  DISALLOW_COPY_AND_ASSIGN(TestObserver);
};

class DataUseAggregatorTest : public testing::Test {
 public:
  DataUseAggregatorTest()
      : reporting_network_delegate_(&data_use_aggregator_,
                                    &test_network_change_notifier_),
        context_(true),
        test_observer_(&data_use_aggregator_) {
    context_.set_client_socket_factory(&mock_socket_factory_);
    context_.set_network_delegate(&reporting_network_delegate_);
    context_.Init();
  }

  ~DataUseAggregatorTest() override {}

  scoped_ptr<net::URLRequest> ExecuteRequest(
      const GURL& url,
      const GURL& first_party_for_cookies,
      int32_t tab_id,
      net::NetworkChangeNotifier::ConnectionType connection_type) {
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

    ReportingNetworkDelegate::DataUseContextMap data_use_context_map;
    data_use_context_map[request.get()] =
        ReportingNetworkDelegate::DataUseContext(tab_id, connection_type);
    reporting_network_delegate_.set_data_use_context_map(data_use_context_map);

    request->Start();
    loop_.RunUntilIdle();

    return request.Pass();
  }

  void SimulateNetworkConnectionChange(
      net::NetworkChangeNotifier::ConnectionType connection_type) {
    test_network_change_notifier_.SimulateNetworkConnectionChange(
        connection_type);
  }

  ReportingNetworkDelegate* reporting_network_delegate() {
    return &reporting_network_delegate_;
  }

  DataUseAggregator* data_use_aggregator() { return &data_use_aggregator_; }

  net::MockClientSocketFactory* mock_socket_factory() {
    return &mock_socket_factory_;
  }

  net::TestURLRequestContext* context() { return &context_; }

  TestObserver* test_observer() { return &test_observer_; }

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
  const int32_t kFooTabId = 10;
  const net::NetworkChangeNotifier::ConnectionType kFooConnectionType =
      net::NetworkChangeNotifier::CONNECTION_2G;
  scoped_ptr<net::URLRequest> foo_request =
      ExecuteRequest(GURL("http://foo.com"), GURL("http://foofirstparty.com"),
                     kFooTabId, kFooConnectionType);

  const int32_t kBarTabId = 20;
  const net::NetworkChangeNotifier::ConnectionType kBarConnectionType =
      net::NetworkChangeNotifier::CONNECTION_WIFI;
  scoped_ptr<net::URLRequest> bar_request =
      ExecuteRequest(GURL("http://bar.com"), GURL("http://barfirstparty.com"),
                     kBarTabId, kBarConnectionType);

  auto data_use_it = test_observer()->observed_data_use().begin();

  // First, the |foo_request| data use should have happened.
  int64_t observed_foo_tx_bytes = 0, observed_foo_rx_bytes = 0;
  while (data_use_it != test_observer()->observed_data_use().end() &&
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
  while (data_use_it != test_observer()->observed_data_use().end()) {
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

TEST_F(DataUseAggregatorTest, ReportCombinedDataUse) {
  // Set up the |foo_request|.
  net::MockRead foo_reads[] = {
      net::MockRead(net::SYNCHRONOUS, "HTTP/1.1 200 OK\r\n\r\n"),
      net::MockRead(net::SYNCHRONOUS, "hello world"),
      net::MockRead(net::SYNCHRONOUS, net::OK),
  };
  net::StaticSocketDataProvider foo_socket(foo_reads, arraysize(foo_reads),
                                           nullptr, 0);
  mock_socket_factory()->AddSocketDataProvider(&foo_socket);

  net::TestDelegate foo_delegate;
  scoped_ptr<net::URLRequest> foo_request = context()->CreateRequest(
      GURL("http://foo.com"), net::IDLE, &foo_delegate);
  foo_request->set_first_party_for_cookies(GURL("http://foofirstparty.com"));

  // Set up the |bar_request|.
  net::MockRead bar_reads[] = {
      net::MockRead(net::SYNCHRONOUS, "HTTP/1.1 200 OK\r\n\r\n"),
      net::MockRead(net::SYNCHRONOUS, "hello world"),
      net::MockRead(net::SYNCHRONOUS, net::OK),
  };
  net::StaticSocketDataProvider bar_socket(bar_reads, arraysize(bar_reads),
                                           nullptr, 0);
  mock_socket_factory()->AddSocketDataProvider(&bar_socket);

  net::TestDelegate bar_delegate;
  scoped_ptr<net::URLRequest> bar_request = context()->CreateRequest(
      GURL("http://bar.com"), net::IDLE, &bar_delegate);
  bar_request->set_first_party_for_cookies(GURL("http://barfirstparty.com"));

  // Set up the network delegate to assign tab IDs and connection types for each
  // request.
  const int32_t kFooTabId = 10;
  const net::NetworkChangeNotifier::ConnectionType kFooConnectionType =
      net::NetworkChangeNotifier::CONNECTION_2G;
  const int32_t kBarTabId = 20;
  const net::NetworkChangeNotifier::ConnectionType kBarConnectionType =
      net::NetworkChangeNotifier::CONNECTION_WIFI;

  ReportingNetworkDelegate::DataUseContextMap data_use_context_map;
  data_use_context_map[foo_request.get()] =
      ReportingNetworkDelegate::DataUseContext(kFooTabId, kFooConnectionType);
  data_use_context_map[bar_request.get()] =
      ReportingNetworkDelegate::DataUseContext(kBarTabId, kBarConnectionType);
  reporting_network_delegate()->set_data_use_context_map(data_use_context_map);

  // Run the requests.
  foo_request->Start();
  bar_request->Start();
  base::MessageLoop::current()->RunUntilIdle();

  // The observer should have been notified once with a DataUse element for each
  // request.
  EXPECT_EQ(1, test_observer()->on_data_use_called_count());
  EXPECT_EQ(static_cast<size_t>(2),
            test_observer()->observed_data_use().size());

  // All of the |foo_request| DataUse should have been combined into a single
  // DataUse element.
  const DataUse& foo_data_use = test_observer()->observed_data_use().front();
  EXPECT_EQ(GURL("http://foo.com"), foo_data_use.url);
  EXPECT_EQ(foo_request->request_time(), foo_data_use.request_time);
  EXPECT_EQ(GURL("http://foofirstparty.com"),
            foo_data_use.first_party_for_cookies);
  EXPECT_EQ(kFooTabId, foo_data_use.tab_id);
  EXPECT_EQ(kFooConnectionType, foo_data_use.connection_type);
  EXPECT_EQ(foo_request->GetTotalSentBytes(), foo_data_use.tx_bytes);
  EXPECT_EQ(foo_request->GetTotalReceivedBytes(), foo_data_use.rx_bytes);

  // All of the |bar_request| DataUse should have been combined into a single
  // DataUse element.
  const DataUse& bar_data_use = test_observer()->observed_data_use().back();
  EXPECT_EQ(GURL("http://bar.com"), bar_data_use.url);
  EXPECT_EQ(bar_request->request_time(), bar_data_use.request_time);
  EXPECT_EQ(GURL("http://barfirstparty.com"),
            bar_data_use.first_party_for_cookies);
  EXPECT_EQ(kBarTabId, bar_data_use.tab_id);
  EXPECT_EQ(kBarConnectionType, bar_data_use.connection_type);
  EXPECT_EQ(bar_request->GetTotalSentBytes(), bar_data_use.tx_bytes);
  EXPECT_EQ(bar_request->GetTotalReceivedBytes(), bar_data_use.rx_bytes);
}

TEST_F(DataUseAggregatorTest, ReportOffTheRecordDataUse) {
  // Off the record data use should not be reported to observers.
  data_use_aggregator()->ReportOffTheRecordDataUse(1000, 1000);
  base::MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(static_cast<size_t>(0),
            test_observer()->observed_data_use().size());
}

}  // namespace

}  // namespace data_usage
