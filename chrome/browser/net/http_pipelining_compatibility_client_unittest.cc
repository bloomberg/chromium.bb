// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/http_pipelining_compatibility_client.h"

#include <map>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/message_loop.h"
#include "base/metrics/histogram.h"
#include "base/stl_util.h"
#include "base/stringprintf.h"
#include "content/test/test_browser_thread.h"
#include "net/base/net_errors.h"
#include "net/base/test_completion_callback.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/url_request/url_request_test_util.h"
#include "net/test/test_server.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chrome_browser_net {

namespace {

static const char* const kHistogramNames[] = {
  "NetConnectivity.Pipeline.Depth",
  "NetConnectivity.Pipeline.AllHTTP11",
  "NetConnectivity.Pipeline.Success",
  "NetConnectivity.Pipeline.0.NetworkError",
  "NetConnectivity.Pipeline.0.ResponseCode",
  "NetConnectivity.Pipeline.0.Status",
  "NetConnectivity.Pipeline.1.NetworkError",
  "NetConnectivity.Pipeline.1.ResponseCode",
  "NetConnectivity.Pipeline.1.Status",
  "NetConnectivity.Pipeline.2.NetworkError",
  "NetConnectivity.Pipeline.2.ResponseCode",
  "NetConnectivity.Pipeline.2.Status",
};

enum HistogramField {
  FIELD_DEPTH,
  FIELD_HTTP_1_1,
  FIELD_NETWORK_ERROR,
  FIELD_RESPONSE_CODE,
  FIELD_STATUS,
  FIELD_SUCCESS,
};

using content::BrowserThread;

class HttpPipeliningCompatibilityClientTest : public testing::Test {
 public:
  HttpPipeliningCompatibilityClientTest()
      : test_server_(
          net::TestServer::TYPE_HTTP,
          net::TestServer::kLocalhost,
          FilePath(FILE_PATH_LITERAL("chrome/test/data/http_pipelining"))),
        io_thread_(BrowserThread::IO, &message_loop_) {
  }

 protected:
  virtual void SetUp() OVERRIDE {
    ASSERT_TRUE(test_server_.Start());
    context_ = new TestURLRequestContextGetter(
        BrowserThread::GetMessageLoopProxyForThread(BrowserThread::IO));
    context_->AddRef();

    for (size_t i = 0; i < arraysize(kHistogramNames); ++i) {
      const char* name = kHistogramNames[i];
      base::Histogram::SampleSet sample = GetHistogram(name);
      if (sample.TotalCount() > 0) {
        original_samples_[name] = sample;
      }
    }
  }

  virtual void TearDown() OVERRIDE {
    BrowserThread::ReleaseSoon(BrowserThread::IO, FROM_HERE, context_);
    message_loop_.RunAllPending();
  }

  void RunTest(
      std::vector<HttpPipeliningCompatibilityClient::RequestInfo> requests,
      bool collect_stats) {
    HttpPipeliningCompatibilityClient client;
    net::TestCompletionCallback callback;
    client.Start(test_server_.GetURL("").spec(),
                 requests, collect_stats, callback.callback(),
                 context_->GetURLRequestContext());
    callback.WaitForResult();
  }

  void ExpectHistogramCount(int expected_count, int expected_value,
                            HistogramField field) {
    const char* name;

    switch (field) {
      case FIELD_DEPTH:
        name = "NetConnectivity.Pipeline.Depth";
        break;

      case FIELD_HTTP_1_1:
        name = "NetConnectivity.Pipeline.AllHTTP11";
        break;

      case FIELD_SUCCESS:
        name = "NetConnectivity.Pipeline.Success";
        break;

      default:
        FAIL() << "Unexpected field: " << field;
    }

    base::Histogram::SampleSet sample = GetHistogram(name);
    if (ContainsKey(original_samples_, name)) {
      sample.Subtract(original_samples_[name]);
    }

    EXPECT_EQ(expected_count, sample.TotalCount()) << name;
    if (expected_count > 0) {
      EXPECT_EQ(expected_count, sample.counts(expected_value)) << name;
    }
  }

  void ExpectRequestHistogramCount(int expected_count, int expected_value,
                                   int request_id, HistogramField field) {
    const char* field_str = "";
    switch (field) {
      case FIELD_STATUS:
        field_str = "Status";
        break;

      case FIELD_NETWORK_ERROR:
        field_str = "NetworkError";
        break;

      case FIELD_RESPONSE_CODE:
        field_str = "ResponseCode";
        break;

      default:
        FAIL() << "Unexpected field: " << field;
    }

    std::string name = base::StringPrintf("NetConnectivity.Pipeline.%d.%s",
                                          request_id, field_str);
    base::Histogram::SampleSet sample = GetHistogram(name.c_str());
    if (ContainsKey(original_samples_, name)) {
      sample.Subtract(original_samples_[name]);
    }

    EXPECT_EQ(expected_count, sample.TotalCount()) << name;
    if (expected_count > 0) {
      EXPECT_EQ(expected_count, sample.counts(expected_value)) << name;
    }
  }

  MessageLoopForIO message_loop_;
  net::TestServer test_server_;
  TestURLRequestContextGetter* context_;
  content::TestBrowserThread io_thread_;

 private:
  base::Histogram::SampleSet GetHistogram(const char* name) {
    base::Histogram::SampleSet sample;
    base::Histogram* current_histogram = NULL;
    base::Histogram* cached_histogram = NULL;
    base::StatisticsRecorder::FindHistogram(name, &current_histogram);
    if (ContainsKey(histograms_, name)) {
      cached_histogram = histograms_[name];
    }

    // This is to work around the CACHE_HISTOGRAM_* macros caching the last used
    // histogram by name. So, even though we throw out the StatisticsRecorder
    // between tests, the CACHE_HISTOGRAM_* might still write into the old
    // Histogram if it has the same name as the last run. We keep a cache of the
    // last used Histogram and then update the cache if it's different than the
    // current Histogram.
    if (cached_histogram && current_histogram) {
      cached_histogram->SnapshotSample(&sample);
      if (cached_histogram != current_histogram) {
        base::Histogram::SampleSet current_sample;
        current_histogram->SnapshotSample(&current_sample);
        sample.Add(current_sample);
        histograms_[name] = current_histogram;
      }
    } else if (current_histogram) {
      current_histogram->SnapshotSample(&sample);
      histograms_[name] = current_histogram;
    } else if (cached_histogram) {
      cached_histogram->SnapshotSample(&sample);
    }
    return sample;
  }

  static std::map<std::string, base::Histogram*> histograms_;
  std::map<std::string, base::Histogram::SampleSet> samples_;
  std::map<std::string, base::Histogram::SampleSet> original_samples_;
  base::StatisticsRecorder recorder_;
};

// static
std::map<std::string, base::Histogram*>
    HttpPipeliningCompatibilityClientTest::histograms_;

TEST_F(HttpPipeliningCompatibilityClientTest, Success) {
  HttpPipeliningCompatibilityClient::RequestInfo info;
  info.filename = "files/alphabet.txt";
  info.expected_response = "abcdefghijklmnopqrstuvwxyz";
  std::vector<HttpPipeliningCompatibilityClient::RequestInfo> requests;
  requests.push_back(info);

  RunTest(requests, false);

  ExpectHistogramCount(1, true, FIELD_SUCCESS);
  ExpectHistogramCount(0, 0, FIELD_DEPTH);
  ExpectHistogramCount(0, 0, FIELD_HTTP_1_1);
  ExpectRequestHistogramCount(
      1, HttpPipeliningCompatibilityClient::SUCCESS, 0, FIELD_STATUS);
  ExpectRequestHistogramCount(0, 0, 0, FIELD_NETWORK_ERROR);
  ExpectRequestHistogramCount(1, 200, 0, FIELD_RESPONSE_CODE);
}

TEST_F(HttpPipeliningCompatibilityClientTest, TooSmall) {
  HttpPipeliningCompatibilityClient::RequestInfo info;
  info.filename = "files/alphabet.txt";
  info.expected_response = "abcdefghijklmnopqrstuvwxyz26";
  std::vector<HttpPipeliningCompatibilityClient::RequestInfo> requests;
  requests.push_back(info);

  RunTest(requests, false);

  ExpectHistogramCount(1, false, FIELD_SUCCESS);
  ExpectRequestHistogramCount(
      1, HttpPipeliningCompatibilityClient::TOO_SMALL, 0, FIELD_STATUS);
  ExpectRequestHistogramCount(0, 0, 0, FIELD_NETWORK_ERROR);
  ExpectRequestHistogramCount(1, 200, 0, FIELD_RESPONSE_CODE);
}

TEST_F(HttpPipeliningCompatibilityClientTest, TooLarge) {
  HttpPipeliningCompatibilityClient::RequestInfo info;
  info.filename = "files/alphabet.txt";
  info.expected_response = "abc";
  std::vector<HttpPipeliningCompatibilityClient::RequestInfo> requests;
  requests.push_back(info);

  RunTest(requests, false);

  ExpectHistogramCount(1, false, FIELD_SUCCESS);
  ExpectRequestHistogramCount(
      1, HttpPipeliningCompatibilityClient::TOO_LARGE, 0, FIELD_STATUS);
  ExpectRequestHistogramCount(0, 0, 0, FIELD_NETWORK_ERROR);
  ExpectRequestHistogramCount(1, 200, 0, FIELD_RESPONSE_CODE);
}

TEST_F(HttpPipeliningCompatibilityClientTest, Mismatch) {
  HttpPipeliningCompatibilityClient::RequestInfo info;
  info.filename = "files/alphabet.txt";
  info.expected_response = "zyxwvutsrqponmlkjihgfedcba";
  std::vector<HttpPipeliningCompatibilityClient::RequestInfo> requests;
  requests.push_back(info);

  RunTest(requests, false);

  ExpectHistogramCount(1, false, FIELD_SUCCESS);
  ExpectRequestHistogramCount(
      1, HttpPipeliningCompatibilityClient::CONTENT_MISMATCH, 0, FIELD_STATUS);
  ExpectRequestHistogramCount(0, 0, 0, FIELD_NETWORK_ERROR);
  ExpectRequestHistogramCount(1, 200, 0, FIELD_RESPONSE_CODE);
}

TEST_F(HttpPipeliningCompatibilityClientTest, Redirect) {
  HttpPipeliningCompatibilityClient::RequestInfo info;
  info.filename = "server-redirect?http://foo.bar/asdf";
  info.expected_response = "shouldn't matter";
  std::vector<HttpPipeliningCompatibilityClient::RequestInfo> requests;
  requests.push_back(info);

  RunTest(requests, false);

  ExpectHistogramCount(1, false, FIELD_SUCCESS);
  ExpectRequestHistogramCount(
      1, HttpPipeliningCompatibilityClient::REDIRECTED, 0, FIELD_STATUS);
  ExpectRequestHistogramCount(0, 0, 0, FIELD_NETWORK_ERROR);
  ExpectRequestHistogramCount(0, 0, 0, FIELD_RESPONSE_CODE);
}

TEST_F(HttpPipeliningCompatibilityClientTest, AuthRequired) {
  HttpPipeliningCompatibilityClient::RequestInfo info;
  info.filename = "auth-basic";
  info.expected_response = "shouldn't matter";
  std::vector<HttpPipeliningCompatibilityClient::RequestInfo> requests;
  requests.push_back(info);

  RunTest(requests, false);

  ExpectHistogramCount(1, false, FIELD_SUCCESS);
  ExpectRequestHistogramCount(
      1, HttpPipeliningCompatibilityClient::BAD_RESPONSE_CODE, 0, FIELD_STATUS);
  ExpectRequestHistogramCount(0, 0, 0, FIELD_NETWORK_ERROR);
  ExpectRequestHistogramCount(1, 401, 0, FIELD_RESPONSE_CODE);
}

TEST_F(HttpPipeliningCompatibilityClientTest, NoContent) {
  HttpPipeliningCompatibilityClient::RequestInfo info;
  info.filename = "nocontent";
  info.expected_response = "shouldn't matter";
  std::vector<HttpPipeliningCompatibilityClient::RequestInfo> requests;
  requests.push_back(info);

  RunTest(requests, false);

  ExpectHistogramCount(1, false, FIELD_SUCCESS);
  ExpectRequestHistogramCount(
      1, HttpPipeliningCompatibilityClient::BAD_RESPONSE_CODE, 0, FIELD_STATUS);
  ExpectRequestHistogramCount(0, 0, 0, FIELD_NETWORK_ERROR);
  ExpectRequestHistogramCount(1, 204, 0, FIELD_RESPONSE_CODE);
}

TEST_F(HttpPipeliningCompatibilityClientTest, CloseSocket) {
  HttpPipeliningCompatibilityClient::RequestInfo info;
  info.filename = "close-socket";
  info.expected_response = "shouldn't matter";
  std::vector<HttpPipeliningCompatibilityClient::RequestInfo> requests;
  requests.push_back(info);

  RunTest(requests, false);

  ExpectHistogramCount(1, false, FIELD_SUCCESS);
  ExpectRequestHistogramCount(
      1, HttpPipeliningCompatibilityClient::NETWORK_ERROR, 0, FIELD_STATUS);
  ExpectRequestHistogramCount(
      1, -net::ERR_EMPTY_RESPONSE, 0, FIELD_NETWORK_ERROR);
  ExpectRequestHistogramCount(0, 0, 0, FIELD_RESPONSE_CODE);
}

TEST_F(HttpPipeliningCompatibilityClientTest, OldHttpVersion) {
  HttpPipeliningCompatibilityClient::RequestInfo info;
  info.filename = "http-1.0";
  info.expected_response = "abcdefghijklmnopqrstuvwxyz";
  std::vector<HttpPipeliningCompatibilityClient::RequestInfo> requests;
  requests.push_back(info);

  RunTest(requests, false);

  ExpectHistogramCount(1, false, FIELD_SUCCESS);
  ExpectRequestHistogramCount(
      1, HttpPipeliningCompatibilityClient::BAD_HTTP_VERSION, 0, FIELD_STATUS);
  ExpectRequestHistogramCount(0, 0, 0, FIELD_NETWORK_ERROR);
  ExpectRequestHistogramCount(1, 200, 0, FIELD_RESPONSE_CODE);
}

TEST_F(HttpPipeliningCompatibilityClientTest, MultipleRequests) {
  std::vector<HttpPipeliningCompatibilityClient::RequestInfo> requests;

  HttpPipeliningCompatibilityClient::RequestInfo info1;
  info1.filename = "files/alphabet.txt";
  info1.expected_response = "abcdefghijklmnopqrstuvwxyz";
  requests.push_back(info1);

  HttpPipeliningCompatibilityClient::RequestInfo info2;
  info2.filename = "close-socket";
  info2.expected_response = "shouldn't matter";
  requests.push_back(info2);

  HttpPipeliningCompatibilityClient::RequestInfo info3;
  info3.filename = "auth-basic";
  info3.expected_response = "shouldn't matter";
  requests.push_back(info3);

  RunTest(requests, true);

  ExpectHistogramCount(1, false, FIELD_SUCCESS);

  ExpectRequestHistogramCount(
      1, HttpPipeliningCompatibilityClient::SUCCESS, 0, FIELD_STATUS);
  ExpectRequestHistogramCount(0, 0, 0, FIELD_NETWORK_ERROR);
  ExpectRequestHistogramCount(1, 200, 0, FIELD_RESPONSE_CODE);

  ExpectRequestHistogramCount(
      1, HttpPipeliningCompatibilityClient::NETWORK_ERROR, 1, FIELD_STATUS);
  ExpectRequestHistogramCount(
      1, -net::ERR_PIPELINE_EVICTION, 1, FIELD_NETWORK_ERROR);
  ExpectRequestHistogramCount(0, 0, 1, FIELD_RESPONSE_CODE);

  ExpectRequestHistogramCount(
      1, HttpPipeliningCompatibilityClient::NETWORK_ERROR, 2, FIELD_STATUS);
  ExpectRequestHistogramCount(
      1, -net::ERR_PIPELINE_EVICTION, 2, FIELD_NETWORK_ERROR);
  ExpectRequestHistogramCount(0, 0, 2, FIELD_RESPONSE_CODE);

  ExpectRequestHistogramCount(
      1, HttpPipeliningCompatibilityClient::NETWORK_ERROR, 3, FIELD_STATUS);
  ExpectRequestHistogramCount(
      1, -net::ERR_PIPELINE_EVICTION, 3, FIELD_NETWORK_ERROR);
  ExpectRequestHistogramCount(0, 0, 3, FIELD_RESPONSE_CODE);
}

TEST_F(HttpPipeliningCompatibilityClientTest, StatsOk) {
  EXPECT_EQ(HttpPipeliningCompatibilityClient::SUCCESS,
            internal::ProcessStatsResponse(
                "max_pipeline_depth:3,were_all_requests_http_1_1:0"));
  ExpectHistogramCount(1, 3, FIELD_DEPTH);
  ExpectHistogramCount(1, 0, FIELD_HTTP_1_1);
}

TEST_F(HttpPipeliningCompatibilityClientTest, StatsIndifferentToOrder) {
  EXPECT_EQ(HttpPipeliningCompatibilityClient::SUCCESS,
            internal::ProcessStatsResponse(
                "were_all_requests_http_1_1:1,max_pipeline_depth:2"));
  ExpectHistogramCount(1, 2, FIELD_DEPTH);
  ExpectHistogramCount(1, 1, FIELD_HTTP_1_1);
}

TEST_F(HttpPipeliningCompatibilityClientTest, StatsBadField) {
  EXPECT_EQ(HttpPipeliningCompatibilityClient::CORRUPT_STATS,
            internal::ProcessStatsResponse(
                "foo:3,were_all_requests_http_1_1:1"));
  ExpectHistogramCount(0, 0, FIELD_DEPTH);
  ExpectHistogramCount(0, 0, FIELD_HTTP_1_1);
}

TEST_F(HttpPipeliningCompatibilityClientTest, StatsTooShort) {
  EXPECT_EQ(HttpPipeliningCompatibilityClient::CORRUPT_STATS,
            internal::ProcessStatsResponse("were_all_requests_http_1_1:1"));
  ExpectHistogramCount(0, 0, FIELD_DEPTH);
  ExpectHistogramCount(0, 0, FIELD_HTTP_1_1);
}

}  // anonymous namespace

}  // namespace chrome_browser_net
