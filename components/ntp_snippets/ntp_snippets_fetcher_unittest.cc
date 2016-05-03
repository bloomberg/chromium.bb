// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_snippets/ntp_snippets_fetcher.h"

#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/histogram_tester.h"
#include "base/thread_task_runner_handle.h"
#include "google_apis/google_api_keys.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ntp_snippets {
namespace {

using testing::ElementsAre;
using testing::IsEmpty;
using testing::IsNull;
using testing::Not;
using testing::NotNull;

const char kTestContentSnippetsServerFormat[] =
    "https://chromereader-pa.googleapis.com/v1/fetch?key=%s";

class MockSnippetsAvailableCallback {
 public:
  MOCK_METHOD2(Run, void(const std::string& snippets_json,
                         const std::string& status_message));
};

// Factory for FakeURLFetcher objects that always generate errors.
class FailingFakeURLFetcherFactory : public net::URLFetcherFactory {
 public:
  std::unique_ptr<net::URLFetcher> CreateURLFetcher(
      int id, const GURL& url, net::URLFetcher::RequestType request_type,
      net::URLFetcherDelegate* d) override {
    return base::WrapUnique(new net::FakeURLFetcher(
        url, d, /*response_data=*/std::string(), net::HTTP_NOT_FOUND,
        net::URLRequestStatus::FAILED));
  }
};

class NTPSnippetsFetcherTest : public testing::Test {
 public:
  NTPSnippetsFetcherTest()
      : fake_url_fetcher_factory_(
            /*default_factory=*/&failing_url_fetcher_factory_),
        snippets_fetcher_(scoped_refptr<net::TestURLRequestContextGetter>(
                              new net::TestURLRequestContextGetter(
                                  base::ThreadTaskRunnerHandle::Get())),
                          /*is_stable_channel=*/true),
        test_url_(base::StringPrintf(kTestContentSnippetsServerFormat,
                                     google_apis::GetAPIKey().c_str())) {
    snippets_fetcher_subscription_ = snippets_fetcher_.AddCallback(
        base::Bind(&MockSnippetsAvailableCallback::Run,
                   base::Unretained(&mock_callback_)));
  }

  net::FakeURLFetcherFactory& fake_url_fetcher_factory() {
    return fake_url_fetcher_factory_;
  }

  NTPSnippetsFetcher& snippets_fetcher() { return snippets_fetcher_; }
  MockSnippetsAvailableCallback& mock_callback() { return mock_callback_; }
  void RunUntilIdle() { message_loop_.RunUntilIdle(); }
  const GURL& test_url() { return test_url_; }

  base::HistogramTester& histogram_tester() { return histogram_tester_; }

 private:
  FailingFakeURLFetcherFactory failing_url_fetcher_factory_;
  // Instantiation of factory automatically sets itself as URLFetcher's factory.
  net::FakeURLFetcherFactory fake_url_fetcher_factory_;
  // Needed to use ThreadTaskRunnerHandle.
  base::MessageLoop message_loop_;
  NTPSnippetsFetcher snippets_fetcher_;
  MockSnippetsAvailableCallback mock_callback_;
  std::unique_ptr<
      NTPSnippetsFetcher::SnippetsAvailableCallbackList::Subscription>
      snippets_fetcher_subscription_;
  const GURL test_url_;
  base::HistogramTester histogram_tester_;

  DISALLOW_COPY_AND_ASSIGN(NTPSnippetsFetcherTest);
};

TEST_F(NTPSnippetsFetcherTest, ShouldNotFetchOnCreation) {
  // The lack of registered baked in responses would cause any fetch to fail.
  RunUntilIdle();
  EXPECT_THAT(histogram_tester().GetAllSamples(
                  "NewTabPage.Snippets.FetchHttpResponseOrErrorCode"),
              IsEmpty());
}

TEST_F(NTPSnippetsFetcherTest, ShouldFetchSuccessfully) {
  const std::string json_str = "{ \"recos\": [] }";
  fake_url_fetcher_factory().SetFakeResponse(test_url(),
                                             /*data=*/json_str, net::HTTP_OK,
                                             net::URLRequestStatus::SUCCESS);
  EXPECT_CALL(mock_callback(), Run(/*snippets_json=*/json_str,
                                   /*status_message=*/std::string()))
      .Times(1);
  snippets_fetcher().FetchSnippets(/*hosts=*/std::set<std::string>(),
                                   /*count=*/1);
  RunUntilIdle();
  EXPECT_THAT(histogram_tester().GetAllSamples(
                  "NewTabPage.Snippets.FetchHttpResponseOrErrorCode"),
              ElementsAre(base::Bucket(/*min=*/200, /*count=*/1)));
}

TEST_F(NTPSnippetsFetcherTest, ShouldReportUrlStatusError) {
  fake_url_fetcher_factory().SetFakeResponse(test_url(),
                                             /*data=*/std::string(),
                                             net::HTTP_NOT_FOUND,
                                             net::URLRequestStatus::FAILED);
  EXPECT_CALL(mock_callback(),
              Run(/*snippets_json=*/std::string(),
                  /*status_message=*/"URLRequestStatus error -2"))
      .Times(1);
  snippets_fetcher().FetchSnippets(/*hosts=*/std::set<std::string>(),
                                   /*count=*/1);
  RunUntilIdle();
  EXPECT_THAT(histogram_tester().GetAllSamples(
                  "NewTabPage.Snippets.FetchHttpResponseOrErrorCode"),
              ElementsAre(base::Bucket(/*min=*/-2, /*count=*/1)));
}

TEST_F(NTPSnippetsFetcherTest, ShouldReportHttpError) {
  fake_url_fetcher_factory().SetFakeResponse(test_url(),
                                             /*data=*/std::string(),
                                             net::HTTP_NOT_FOUND,
                                             net::URLRequestStatus::SUCCESS);
  EXPECT_CALL(mock_callback(), Run(/*snippets_json=*/std::string(),
                                   /*status_message=*/"HTTP error 404"))
      .Times(1);
  snippets_fetcher().FetchSnippets(/*hosts=*/std::set<std::string>(),
                                   /*count=*/1);
  RunUntilIdle();
  EXPECT_THAT(histogram_tester().GetAllSamples(
                  "NewTabPage.Snippets.FetchHttpResponseOrErrorCode"),
              ElementsAre(base::Bucket(/*min=*/404, /*count=*/1)));
}

// This test actually verifies that the test setup itself is sane, to prevent
// hard-to-reproduce test failures.
TEST_F(NTPSnippetsFetcherTest, ShouldReportHttpErrorForMissingBakedResponse) {
  EXPECT_CALL(mock_callback(), Run(/*snippets_json=*/std::string(),
                                   /*status_message=*/Not(IsEmpty())))
      .Times(1);
  snippets_fetcher().FetchSnippets(/*hosts=*/std::set<std::string>(),
                                   /*count=*/1);
  RunUntilIdle();
}

TEST_F(NTPSnippetsFetcherTest, ShouldCancelOngoingFetch) {
  const std::string json_str = "{ \"recos\": [] }";
  fake_url_fetcher_factory().SetFakeResponse(test_url(),
                                             /*data=*/json_str, net::HTTP_OK,
                                             net::URLRequestStatus::SUCCESS);
  EXPECT_CALL(mock_callback(), Run(/*snippets_json=*/json_str,
                                   /*status_message=*/std::string()))
      .Times(1);
  snippets_fetcher().FetchSnippets(/*hosts=*/std::set<std::string>(),
                                   /*count=*/1);
  // Second call to FetchSnippets() overrides/cancels the previous. Callback is
  // expected to be called once.
  snippets_fetcher().FetchSnippets(/*hosts=*/std::set<std::string>(),
                                   /*count=*/1);
  RunUntilIdle();
  EXPECT_THAT(histogram_tester().GetAllSamples(
                  "NewTabPage.Snippets.FetchHttpResponseOrErrorCode"),
              ElementsAre(base::Bucket(/*min=*/200, /*count=*/1)));
}

}  // namespace
}  // namespace ntp_snippets
