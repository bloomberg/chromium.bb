// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_pingback_client.h"

#include <memory>
#include <string>

#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/time/time.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_data.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_page_load_timing.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_util.h"
#include "components/data_reduction_proxy/proto/client_config.pb.h"
#include "components/data_reduction_proxy/proto/pageload_metrics.pb.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace data_reduction_proxy {

namespace {

static const char kSessionKey[] = "fake-session";
static const char kFakeURL[] = "http://www.google.com/";

}  // namespace

// Controls whether a pingback is sent or not.
class TestDataReductionProxyPingbackClient
    : public DataReductionProxyPingbackClient {
 public:
  TestDataReductionProxyPingbackClient(
      net::URLRequestContextGetter* url_request_context_getter)
      : DataReductionProxyPingbackClient(url_request_context_getter),
        should_send_pingback_(false) {}

  void set_should_send_pingback(bool should_send_pingback) {
    should_send_pingback_ = should_send_pingback;
  }

 private:
  bool ShouldSendPingback() const override { return should_send_pingback_; }

  bool should_send_pingback_;
};

class DataReductionProxyPingbackClientTest : public testing::Test {
 public:
  DataReductionProxyPingbackClientTest()
      : timing_(base::Time::FromJsTime(1500),
                base::TimeDelta::FromMilliseconds(1600),
                base::TimeDelta::FromMilliseconds(1700),
                base::TimeDelta::FromMilliseconds(1800),
                base::TimeDelta::FromMilliseconds(1900)) {}

  TestDataReductionProxyPingbackClient* pingback_client() const {
    return pingback_client_.get();
  }

  void Init() {
    request_context_getter_ =
        new net::TestURLRequestContextGetter(message_loop_.task_runner());
    pingback_client_ = base::WrapUnique<TestDataReductionProxyPingbackClient>(
        new TestDataReductionProxyPingbackClient(
            request_context_getter_.get()));
  }

  void CreateAndSendPingback() {
    DataReductionProxyData request_data;
    request_data.set_session_key(kSessionKey);
    request_data.set_original_request_url(GURL(kFakeURL));
    factory()->set_remove_fetcher_on_delete(true);
    pingback_client()->SendPingback(request_data, timing_);
  }

  net::TestURLFetcherFactory* factory() { return &factory_; }

  const DataReductionProxyPageLoadTiming& timing() { return timing_; }

 private:
  base::MessageLoopForIO message_loop_;
  scoped_refptr<net::URLRequestContextGetter> request_context_getter_;
  std::unique_ptr<TestDataReductionProxyPingbackClient> pingback_client_;
  net::TestURLFetcherFactory factory_;
  DataReductionProxyPageLoadTiming timing_;
};

TEST_F(DataReductionProxyPingbackClientTest, VerifyPingbackContent) {
  Init();
  EXPECT_FALSE(factory()->GetFetcherByID(0));
  pingback_client()->set_should_send_pingback(true);
  CreateAndSendPingback();

  net::TestURLFetcher* test_fetcher = factory()->GetFetcherByID(0);
  EXPECT_EQ(test_fetcher->upload_content_type(), "application/x-protobuf");
  RecordPageloadMetricsRequest batched_request;
  batched_request.ParseFromString(test_fetcher->upload_data());
  PageloadMetrics pageload_metrics = batched_request.pageloads(0);
  EXPECT_EQ(
      timing().navigation_start,
      protobuf_parser::TimestampToTime(pageload_metrics.first_request_time()));
  EXPECT_EQ(timing().response_start,
            protobuf_parser::DurationToTimeDelta(
                pageload_metrics.time_to_first_byte()));
  EXPECT_EQ(timing().load_event_start, protobuf_parser::DurationToTimeDelta(
                                           pageload_metrics.page_load_time()));
  EXPECT_EQ(timing().first_image_paint,
            protobuf_parser::DurationToTimeDelta(
                pageload_metrics.time_to_first_image_paint()));
  EXPECT_EQ(timing().first_contentful_paint,
            protobuf_parser::DurationToTimeDelta(
                pageload_metrics.time_to_first_contentful_paint()));

  EXPECT_EQ(kSessionKey, pageload_metrics.session_key());
  EXPECT_EQ(kFakeURL, pageload_metrics.first_request_url());
  test_fetcher->delegate()->OnURLFetchComplete(test_fetcher);
  EXPECT_FALSE(factory()->GetFetcherByID(0));
}

TEST_F(DataReductionProxyPingbackClientTest, SendTwoPingbacks) {
  Init();
  EXPECT_FALSE(factory()->GetFetcherByID(0));
  pingback_client()->set_should_send_pingback(true);
  CreateAndSendPingback();
  CreateAndSendPingback();

  net::TestURLFetcher* test_fetcher = factory()->GetFetcherByID(0);
  test_fetcher->delegate()->OnURLFetchComplete(test_fetcher);
  EXPECT_TRUE(factory()->GetFetcherByID(0));
  test_fetcher = factory()->GetFetcherByID(0);
  test_fetcher->delegate()->OnURLFetchComplete(test_fetcher);
  EXPECT_FALSE(factory()->GetFetcherByID(0));
}

TEST_F(DataReductionProxyPingbackClientTest, NoPingbackSent) {
  Init();
  EXPECT_FALSE(factory()->GetFetcherByID(0));
  pingback_client()->set_should_send_pingback(false);
  CreateAndSendPingback();
  EXPECT_FALSE(factory()->GetFetcherByID(0));
}

}  // namespace data_reduction_proxy
