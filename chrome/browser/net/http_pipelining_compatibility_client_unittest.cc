// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/http_pipelining_compatibility_client.h"

#include <map>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/metrics/histogram.h"
#include "base/metrics/histogram_samples.h"
#include "base/metrics/statistics_recorder.h"
#include "base/stl_util.h"
#include "base/stringprintf.h"
#include "content/public/test/test_browser_thread.h"
#include "net/base/net_errors.h"
#include "net/base/test_completion_callback.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/url_request/url_request_test_util.h"
#include "net/test/test_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::HistogramBase;
using base::HistogramSamples;

namespace chrome_browser_net {

namespace {

static const char* const kHistogramNames[] = {
  "NetConnectivity.Pipeline.CanarySuccess",
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
  FIELD_CANARY,
  FIELD_DEPTH,
  FIELD_HTTP_1_1,
  FIELD_NETWORK_ERROR,
  FIELD_RESPONSE_CODE,
  FIELD_STATUS,
  FIELD_SUCCESS,
};

class MockFactory : public internal::PipelineTestRequest::Factory {
 public:
  MOCK_METHOD6(NewRequest, internal::PipelineTestRequest*(
      int, const std::string&, const RequestInfo&,
      internal::PipelineTestRequest::Delegate*, net::URLRequestContext*,
      internal::PipelineTestRequest::Type));
};

class MockRequest : public internal::PipelineTestRequest {
 public:
  MOCK_METHOD0(Start, void());
};

using content::BrowserThread;
using testing::_;
using testing::Field;
using testing::Invoke;
using testing::Return;
using testing::StrEq;

class HttpPipeliningCompatibilityClientTest : public testing::Test {
 public:
  HttpPipeliningCompatibilityClientTest()
      : test_server_(net::TestServer::TYPE_HTTP,
                     net::TestServer::kLocalhost,
                     base::FilePath(FILE_PATH_LITERAL(
                         "chrome/test/data/http_pipelining"))),
        io_thread_(BrowserThread::IO, &message_loop_) {
  }

 protected:
  virtual void SetUp() OVERRIDE {
    // Start up a histogram recorder.
    // TODO(rtenneti): Leaks StatisticsRecorder and will update suppressions.
    base::StatisticsRecorder::Initialize();
    ASSERT_TRUE(test_server_.Start());
    context_ = new net::TestURLRequestContextGetter(
        BrowserThread::GetMessageLoopProxyForThread(BrowserThread::IO));
    context_->AddRef();

    for (size_t i = 0; i < arraysize(kHistogramNames); ++i) {
      const char* name = kHistogramNames[i];
      scoped_ptr<HistogramSamples> samples = GetHistogram(name);
      if (samples.get() && samples->TotalCount() > 0) {
        original_samples_[name] = samples.release();
      }
    }
  }

  virtual void TearDown() OVERRIDE {
    BrowserThread::ReleaseSoon(BrowserThread::IO, FROM_HERE, context_);
    message_loop_.RunUntilIdle();
    STLDeleteValues(&original_samples_);
  }

  void RunTest(
      std::vector<RequestInfo> requests,
      HttpPipeliningCompatibilityClient::Options options) {
    HttpPipeliningCompatibilityClient client(NULL);
    net::TestCompletionCallback callback;
    client.Start(test_server_.GetURL("").spec(),
                 requests, options, callback.callback(),
                 context_->GetURLRequestContext());
    callback.WaitForResult();
  }

  void ExpectHistogramCount(int expected_count,
                            int expected_value,
                            HistogramField field) {
    const char* name;

    switch (field) {
      case FIELD_CANARY:
        name = "NetConnectivity.Pipeline.CanarySuccess";
        break;

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

    scoped_ptr<HistogramSamples> samples = GetHistogram(name);
    if (!samples.get())
      return;

    if (ContainsKey(original_samples_, name)) {
      samples->Subtract((*original_samples_[name]));
    }

    EXPECT_EQ(expected_count, samples->TotalCount()) << name;
    if (expected_count > 0) {
      EXPECT_EQ(expected_count, samples->GetCount(expected_value)) << name;
    }
  }

  void ExpectRequestHistogramCount(int expected_count,
                                   int expected_value,
                                   int request_id,
                                   HistogramField field) {
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
    scoped_ptr<HistogramSamples> samples = GetHistogram(name.c_str());
    if (!samples.get())
      return;

    if (ContainsKey(original_samples_, name)) {
      samples->Subtract(*(original_samples_[name]));
    }

    EXPECT_EQ(expected_count, samples->TotalCount()) << name;
    if (expected_count > 0) {
      EXPECT_EQ(expected_count, samples->GetCount(expected_value)) << name;
    }
  }

  MessageLoopForIO message_loop_;
  net::TestServer test_server_;
  net::TestURLRequestContextGetter* context_;
  content::TestBrowserThread io_thread_;

 private:
  scoped_ptr<HistogramSamples> GetHistogram(const char* name) {
    scoped_ptr<HistogramSamples> samples;
    HistogramBase* cached_histogram = NULL;
    HistogramBase* current_histogram =
        base::StatisticsRecorder::FindHistogram(name);
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
      samples = cached_histogram->SnapshotSamples();
      if (cached_histogram != current_histogram) {
        samples->Add(*(current_histogram->SnapshotSamples()));
        histograms_[name] = current_histogram;
      }
    } else if (current_histogram) {
      samples = current_histogram->SnapshotSamples();
      histograms_[name] = current_histogram;
    } else if (cached_histogram) {
      samples = cached_histogram->SnapshotSamples();
    }
    return samples.Pass();
  }

  static std::map<std::string, HistogramBase*> histograms_;
  std::map<std::string, HistogramSamples*> original_samples_;
};

// static
std::map<std::string, HistogramBase*>
    HttpPipeliningCompatibilityClientTest::histograms_;

TEST_F(HttpPipeliningCompatibilityClientTest, Success) {
  RequestInfo info;
  info.filename = "files/alphabet.txt";
  info.expected_response = "abcdefghijklmnopqrstuvwxyz";
  std::vector<RequestInfo> requests;
  requests.push_back(info);

  RunTest(requests, HttpPipeliningCompatibilityClient::PIPE_TEST_DEFAULTS);

  ExpectHistogramCount(1, true, FIELD_SUCCESS);
  ExpectHistogramCount(0, 0, FIELD_DEPTH);
  ExpectHistogramCount(0, 0, FIELD_HTTP_1_1);
  ExpectRequestHistogramCount(
      1, internal::PipelineTestRequest::STATUS_SUCCESS, 0, FIELD_STATUS);
  ExpectRequestHistogramCount(0, 0, 0, FIELD_NETWORK_ERROR);
  ExpectRequestHistogramCount(1, 200, 0, FIELD_RESPONSE_CODE);
}

TEST_F(HttpPipeliningCompatibilityClientTest, TooSmall) {
  RequestInfo info;
  info.filename = "files/alphabet.txt";
  info.expected_response = "abcdefghijklmnopqrstuvwxyz26";
  std::vector<RequestInfo> requests;
  requests.push_back(info);

  RunTest(requests, HttpPipeliningCompatibilityClient::PIPE_TEST_DEFAULTS);

  ExpectHistogramCount(1, false, FIELD_SUCCESS);
  ExpectRequestHistogramCount(
      1, internal::PipelineTestRequest::STATUS_TOO_SMALL, 0, FIELD_STATUS);
  ExpectRequestHistogramCount(0, 0, 0, FIELD_NETWORK_ERROR);
  ExpectRequestHistogramCount(1, 200, 0, FIELD_RESPONSE_CODE);
}

TEST_F(HttpPipeliningCompatibilityClientTest, TooLarge) {
  RequestInfo info;
  info.filename = "files/alphabet.txt";
  info.expected_response = "abc";
  std::vector<RequestInfo> requests;
  requests.push_back(info);

  RunTest(requests, HttpPipeliningCompatibilityClient::PIPE_TEST_DEFAULTS);

  ExpectHistogramCount(1, false, FIELD_SUCCESS);
  ExpectRequestHistogramCount(
      1, internal::PipelineTestRequest::STATUS_TOO_LARGE, 0, FIELD_STATUS);
  ExpectRequestHistogramCount(0, 0, 0, FIELD_NETWORK_ERROR);
  ExpectRequestHistogramCount(1, 200, 0, FIELD_RESPONSE_CODE);
}

TEST_F(HttpPipeliningCompatibilityClientTest, Mismatch) {
  RequestInfo info;
  info.filename = "files/alphabet.txt";
  info.expected_response = "zyxwvutsrqponmlkjihgfedcba";
  std::vector<RequestInfo> requests;
  requests.push_back(info);

  RunTest(requests, HttpPipeliningCompatibilityClient::PIPE_TEST_DEFAULTS);

  ExpectHistogramCount(1, false, FIELD_SUCCESS);
  ExpectRequestHistogramCount(
      1, internal::PipelineTestRequest::STATUS_CONTENT_MISMATCH,
      0, FIELD_STATUS);
  ExpectRequestHistogramCount(0, 0, 0, FIELD_NETWORK_ERROR);
  ExpectRequestHistogramCount(1, 200, 0, FIELD_RESPONSE_CODE);
}

TEST_F(HttpPipeliningCompatibilityClientTest, Redirect) {
  RequestInfo info;
  info.filename = "server-redirect?http://foo.bar/asdf";
  info.expected_response = "shouldn't matter";
  std::vector<RequestInfo> requests;
  requests.push_back(info);

  RunTest(requests, HttpPipeliningCompatibilityClient::PIPE_TEST_DEFAULTS);

  ExpectHistogramCount(1, false, FIELD_SUCCESS);
  ExpectRequestHistogramCount(
      1, internal::PipelineTestRequest::STATUS_REDIRECTED, 0, FIELD_STATUS);
  ExpectRequestHistogramCount(0, 0, 0, FIELD_NETWORK_ERROR);
  ExpectRequestHistogramCount(0, 0, 0, FIELD_RESPONSE_CODE);
}

TEST_F(HttpPipeliningCompatibilityClientTest, AuthRequired) {
  RequestInfo info;
  info.filename = "auth-basic";
  info.expected_response = "shouldn't matter";
  std::vector<RequestInfo> requests;
  requests.push_back(info);

  RunTest(requests, HttpPipeliningCompatibilityClient::PIPE_TEST_DEFAULTS);

  ExpectHistogramCount(1, false, FIELD_SUCCESS);
  ExpectRequestHistogramCount(
      1, internal::PipelineTestRequest::STATUS_BAD_RESPONSE_CODE,
      0, FIELD_STATUS);
  ExpectRequestHistogramCount(0, 0, 0, FIELD_NETWORK_ERROR);
  ExpectRequestHistogramCount(1, 401, 0, FIELD_RESPONSE_CODE);
}

TEST_F(HttpPipeliningCompatibilityClientTest, NoContent) {
  RequestInfo info;
  info.filename = "nocontent";
  info.expected_response = "shouldn't matter";
  std::vector<RequestInfo> requests;
  requests.push_back(info);

  RunTest(requests, HttpPipeliningCompatibilityClient::PIPE_TEST_DEFAULTS);

  ExpectHistogramCount(1, false, FIELD_SUCCESS);
  ExpectRequestHistogramCount(
      1, internal::PipelineTestRequest::STATUS_BAD_RESPONSE_CODE,
      0, FIELD_STATUS);
  ExpectRequestHistogramCount(0, 0, 0, FIELD_NETWORK_ERROR);
  ExpectRequestHistogramCount(1, 204, 0, FIELD_RESPONSE_CODE);
}

TEST_F(HttpPipeliningCompatibilityClientTest, CloseSocket) {
  RequestInfo info;
  info.filename = "close-socket";
  info.expected_response = "shouldn't matter";
  std::vector<RequestInfo> requests;
  requests.push_back(info);

  RunTest(requests, HttpPipeliningCompatibilityClient::PIPE_TEST_DEFAULTS);

  ExpectHistogramCount(1, false, FIELD_SUCCESS);
  ExpectRequestHistogramCount(
      1, internal::PipelineTestRequest::STATUS_NETWORK_ERROR, 0, FIELD_STATUS);
  ExpectRequestHistogramCount(
      1, -net::ERR_EMPTY_RESPONSE, 0, FIELD_NETWORK_ERROR);
  ExpectRequestHistogramCount(0, 0, 0, FIELD_RESPONSE_CODE);
}

TEST_F(HttpPipeliningCompatibilityClientTest, OldHttpVersion) {
  RequestInfo info;
  info.filename = "http-1.0";
  info.expected_response = "abcdefghijklmnopqrstuvwxyz";
  std::vector<RequestInfo> requests;
  requests.push_back(info);

  RunTest(requests, HttpPipeliningCompatibilityClient::PIPE_TEST_DEFAULTS);

  ExpectHistogramCount(1, false, FIELD_SUCCESS);
  ExpectRequestHistogramCount(
      1, internal::PipelineTestRequest::STATUS_BAD_HTTP_VERSION,
      0, FIELD_STATUS);
  ExpectRequestHistogramCount(0, 0, 0, FIELD_NETWORK_ERROR);
  ExpectRequestHistogramCount(1, 200, 0, FIELD_RESPONSE_CODE);
}

#if defined(OS_CHROMEOS)
// http://crbug.com/147903: test fails on ChromeOS
#define MAYBE_MultipleRequests DISABLED_MultipleRequests
#else
#define MAYBE_MultipleRequests MultipleRequests
#endif
TEST_F(HttpPipeliningCompatibilityClientTest, MAYBE_MultipleRequests) {
  std::vector<RequestInfo> requests;

  RequestInfo info1;
  info1.filename = "files/alphabet.txt";
  info1.expected_response = "abcdefghijklmnopqrstuvwxyz";
  requests.push_back(info1);

  RequestInfo info2;
  info2.filename = "close-socket";
  info2.expected_response = "shouldn't matter";
  requests.push_back(info2);

  RequestInfo info3;
  info3.filename = "auth-basic";
  info3.expected_response = "shouldn't matter";
  requests.push_back(info3);

  RunTest(requests,
          HttpPipeliningCompatibilityClient::PIPE_TEST_COLLECT_SERVER_STATS);

  ExpectHistogramCount(1, false, FIELD_SUCCESS);

  ExpectRequestHistogramCount(
      1, internal::PipelineTestRequest::STATUS_SUCCESS, 0, FIELD_STATUS);
  ExpectRequestHistogramCount(0, 0, 0, FIELD_NETWORK_ERROR);
  ExpectRequestHistogramCount(1, 200, 0, FIELD_RESPONSE_CODE);

  ExpectRequestHistogramCount(
      1, internal::PipelineTestRequest::STATUS_NETWORK_ERROR, 1, FIELD_STATUS);
  ExpectRequestHistogramCount(
      1, -net::ERR_PIPELINE_EVICTION, 1, FIELD_NETWORK_ERROR);
  ExpectRequestHistogramCount(0, 0, 1, FIELD_RESPONSE_CODE);

  ExpectRequestHistogramCount(
      1, internal::PipelineTestRequest::STATUS_NETWORK_ERROR, 2, FIELD_STATUS);
  ExpectRequestHistogramCount(
      1, -net::ERR_PIPELINE_EVICTION, 2, FIELD_NETWORK_ERROR);
  ExpectRequestHistogramCount(0, 0, 2, FIELD_RESPONSE_CODE);

  ExpectRequestHistogramCount(
      1, internal::PipelineTestRequest::STATUS_NETWORK_ERROR, 3, FIELD_STATUS);
  ExpectRequestHistogramCount(
      1, -net::ERR_PIPELINE_EVICTION, 3, FIELD_NETWORK_ERROR);
  ExpectRequestHistogramCount(0, 0, 3, FIELD_RESPONSE_CODE);
}

TEST_F(HttpPipeliningCompatibilityClientTest, StatsOk) {
  EXPECT_EQ(internal::PipelineTestRequest::STATUS_SUCCESS,
            internal::ProcessStatsResponse(
                "max_pipeline_depth:3,were_all_requests_http_1_1:0"));
  ExpectHistogramCount(1, 3, FIELD_DEPTH);
  ExpectHistogramCount(1, 0, FIELD_HTTP_1_1);
}

TEST_F(HttpPipeliningCompatibilityClientTest, StatsIndifferentToOrder) {
  EXPECT_EQ(internal::PipelineTestRequest::STATUS_SUCCESS,
            internal::ProcessStatsResponse(
                "were_all_requests_http_1_1:1,max_pipeline_depth:2"));
  ExpectHistogramCount(1, 2, FIELD_DEPTH);
  ExpectHistogramCount(1, 1, FIELD_HTTP_1_1);
}

#if defined(OS_CHROMEOS)
// http://crbug.com/147903: test fails on ChromeOS
#define MAYBE_StatsBadField DISABLED_StatsBadField
#else
#define MAYBE_StatsBadField StatsBadField
#endif
TEST_F(HttpPipeliningCompatibilityClientTest, MAYBE_StatsBadField) {
  EXPECT_EQ(internal::PipelineTestRequest::STATUS_CORRUPT_STATS,
            internal::ProcessStatsResponse(
                "foo:3,were_all_requests_http_1_1:1"));
  ExpectHistogramCount(0, 0, FIELD_DEPTH);
  ExpectHistogramCount(0, 0, FIELD_HTTP_1_1);
}

TEST_F(HttpPipeliningCompatibilityClientTest, StatsTooShort) {
  EXPECT_EQ(internal::PipelineTestRequest::STATUS_CORRUPT_STATS,
            internal::ProcessStatsResponse("were_all_requests_http_1_1:1"));
  ExpectHistogramCount(0, 0, FIELD_DEPTH);
  ExpectHistogramCount(0, 0, FIELD_HTTP_1_1);
}

TEST_F(HttpPipeliningCompatibilityClientTest, WaitForCanary) {
  MockFactory* factory = new MockFactory;
  HttpPipeliningCompatibilityClient client(factory);

  MockRequest* request = new MockRequest;
  base::Closure request_cb = base::Bind(
      &internal::PipelineTestRequest::Delegate::OnRequestFinished,
      base::Unretained(&client), 0,
      internal::PipelineTestRequest::STATUS_SUCCESS);

  MockRequest* canary = new MockRequest;
  base::Closure canary_cb = base::Bind(
      &internal::PipelineTestRequest::Delegate::OnCanaryFinished,
      base::Unretained(&client), internal::PipelineTestRequest::STATUS_SUCCESS);

  EXPECT_CALL(*factory, NewRequest(
      0, _, Field(&RequestInfo::filename, StrEq("request.txt")), _, _,
      internal::PipelineTestRequest::TYPE_PIPELINED))
      .Times(1)
      .WillOnce(Return(request));
  EXPECT_CALL(*factory, NewRequest(
      999, _, Field(&RequestInfo::filename, StrEq("index.html")), _, _,
      internal::PipelineTestRequest::TYPE_CANARY))
      .Times(1)
      .WillOnce(Return(canary));

  EXPECT_CALL(*canary, Start())
      .Times(1)
      .WillOnce(Invoke(&canary_cb, &base::Closure::Run));
  EXPECT_CALL(*request, Start())
      .Times(1)
      .WillOnce(Invoke(&request_cb, &base::Closure::Run));

  std::vector<RequestInfo> requests;

  RequestInfo info1;
  info1.filename = "request.txt";
  requests.push_back(info1);

  net::TestCompletionCallback callback;
  client.Start("http://base/", requests,
               HttpPipeliningCompatibilityClient::PIPE_TEST_RUN_CANARY_REQUEST,
               callback.callback(), context_->GetURLRequestContext());
  callback.WaitForResult();

  ExpectHistogramCount(1, true, FIELD_CANARY);
  ExpectHistogramCount(1, true, FIELD_SUCCESS);
  ExpectRequestHistogramCount(
      1, internal::PipelineTestRequest::STATUS_SUCCESS, 0, FIELD_STATUS);
}

#if defined(OS_CHROMEOS)
// http://crbug.com/147903: test fails on ChromeOS
#define MAYBE_CanaryFailure DISABLED_CanaryFailure
#else
#define MAYBE_CanaryFailure CanaryFailure
#endif
TEST_F(HttpPipeliningCompatibilityClientTest, MAYBE_CanaryFailure) {
  MockFactory* factory = new MockFactory;
  HttpPipeliningCompatibilityClient client(factory);

  MockRequest* request = new MockRequest;

  MockRequest* canary = new MockRequest;
  base::Closure canary_cb = base::Bind(
      &internal::PipelineTestRequest::Delegate::OnCanaryFinished,
      base::Unretained(&client),
      internal::PipelineTestRequest::STATUS_REDIRECTED);

  EXPECT_CALL(*factory, NewRequest(
      0, _, Field(&RequestInfo::filename, StrEq("request.txt")), _, _,
      internal::PipelineTestRequest::TYPE_PIPELINED))
      .Times(1)
      .WillOnce(Return(request));
  EXPECT_CALL(*factory, NewRequest(
      999, _, Field(&RequestInfo::filename, StrEq("index.html")), _, _,
      internal::PipelineTestRequest::TYPE_CANARY))
      .Times(1)
      .WillOnce(Return(canary));

  EXPECT_CALL(*canary, Start())
      .Times(1)
      .WillOnce(Invoke(&canary_cb, &base::Closure::Run));
  EXPECT_CALL(*request, Start())
      .Times(0);

  std::vector<RequestInfo> requests;

  RequestInfo info1;
  info1.filename = "request.txt";
  requests.push_back(info1);

  net::TestCompletionCallback callback;
  client.Start("http://base/", requests,
               HttpPipeliningCompatibilityClient::PIPE_TEST_RUN_CANARY_REQUEST,
               callback.callback(), context_->GetURLRequestContext());
  callback.WaitForResult();

  ExpectHistogramCount(1, false, FIELD_CANARY);
  ExpectHistogramCount(0, false, FIELD_SUCCESS);
  ExpectHistogramCount(0, true, FIELD_SUCCESS);
}

}  // anonymous namespace

}  // namespace chrome_browser_net
