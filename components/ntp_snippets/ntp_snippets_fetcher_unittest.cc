// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_snippets/ntp_snippets_fetcher.h"

#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/thread_task_runner_handle.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ntp_snippets {
namespace {

using testing::IsNull;
using testing::NotNull;

const char kTestContentSnippetsServerUrl[] =
    "https://chromereader-pa.googleapis.com/v1/fetch?key=dummytoken";

class MockSnippetsAvailableCallback {
 public:
  MOCK_METHOD2(Run, void(const std::string& snippets_json,
                         const std::string& status_message));
};

class NTPSnippetsFetcherTest : public testing::Test {
 public:
  NTPSnippetsFetcherTest()
      : fake_url_fetcher_factory_(/*default_factory=*/nullptr),
        snippets_fetcher_(scoped_refptr<base::SequencedTaskRunner>(),
                          scoped_refptr<net::TestURLRequestContextGetter>(
                              new net::TestURLRequestContextGetter(
                                  base::ThreadTaskRunnerHandle::Get())),
                          /*is_stable_channel=*/true) {
    snippets_fetcher_subscription_ = snippets_fetcher_.AddCallback(
        base::Bind(&MockSnippetsAvailableCallback::Run,
                   base::Unretained(&mock_callback_)));
  }

  NTPSnippetsFetcher& snippets_fetcher() { return snippets_fetcher_; }

  net::FakeURLFetcherFactory& fake_url_fetcher_factory() {
    return fake_url_fetcher_factory_;
  }

  MockSnippetsAvailableCallback& mock_callback() { return mock_callback_; }

 private:
  // Instantiation of factory automatically sets itself as URLFetcher's factory.
  net::FakeURLFetcherFactory fake_url_fetcher_factory_;
  // Needed to use ThreadTaskRunnerHandle.
  base::MessageLoop message_loop_;
  NTPSnippetsFetcher snippets_fetcher_;
  MockSnippetsAvailableCallback mock_callback_;
  std::unique_ptr<
      NTPSnippetsFetcher::SnippetsAvailableCallbackList::Subscription>
      snippets_fetcher_subscription_;

  DISALLOW_COPY_AND_ASSIGN(NTPSnippetsFetcherTest);
};

TEST_F(NTPSnippetsFetcherTest, ShouldNotFetchOnCreation) {
  // The lack of registered baked in responses would cause any fetch to fail.
  base::RunLoop().RunUntilIdle();
}

TEST_F(NTPSnippetsFetcherTest, ShouldFetchSuccessfully) {
  const std::string json_str = "{ \"recos\": [] }";
  fake_url_fetcher_factory().SetFakeResponse(
      GURL(kTestContentSnippetsServerUrl),
      /*data=*/json_str, net::HTTP_OK, net::URLRequestStatus::SUCCESS);
  EXPECT_CALL(mock_callback(), Run(/*snippets_json=*/json_str,
                                   /*status_message=*/std::string()))
      .Times(1);
  snippets_fetcher().FetchSnippets(/*hosts=*/std::set<std::string>(),
                                   /*count=*/1);
  base::RunLoop().RunUntilIdle();
}

TEST_F(NTPSnippetsFetcherTest, ShouldReportUrlStatusError) {
  fake_url_fetcher_factory().SetFakeResponse(
      GURL(kTestContentSnippetsServerUrl),
      /*data=*/std::string(), net::HTTP_NOT_FOUND,
      net::URLRequestStatus::FAILED);
  EXPECT_CALL(mock_callback(),
              Run(/*snippets_json=*/std::string(),
                  /*status_message=*/"URLRequestStatus error -2"))
      .Times(1);
  snippets_fetcher().FetchSnippets(/*hosts=*/std::set<std::string>(),
                                   /*count=*/1);
  base::RunLoop().RunUntilIdle();
}

TEST_F(NTPSnippetsFetcherTest, ShouldReportHttpError) {
  fake_url_fetcher_factory().SetFakeResponse(
      GURL(kTestContentSnippetsServerUrl),
      /*data=*/std::string(), net::HTTP_NOT_FOUND,
      net::URLRequestStatus::SUCCESS);
  EXPECT_CALL(mock_callback(), Run(/*snippets_json=*/std::string(),
                                   /*status_message=*/"HTTP error 404"))
      .Times(1);
  snippets_fetcher().FetchSnippets(/*hosts=*/std::set<std::string>(),
                                   /*count=*/1);
  base::RunLoop().RunUntilIdle();
}

TEST_F(NTPSnippetsFetcherTest, ShouldCancelOngoingFetch) {
  const std::string json_str = "{ \"recos\": [] }";
  fake_url_fetcher_factory().SetFakeResponse(
      GURL(kTestContentSnippetsServerUrl),
      /*data=*/json_str, net::HTTP_OK, net::URLRequestStatus::SUCCESS);
  EXPECT_CALL(mock_callback(), Run(/*snippets_json=*/json_str,
                                   /*status_message=*/std::string()))
      .Times(1);
  snippets_fetcher().FetchSnippets(/*hosts=*/std::set<std::string>(),
                                   /*count=*/1);
  // Second call to FetchSnippets() overrides/cancels the previous. Callback is
  // expected to be called once.
  snippets_fetcher().FetchSnippets(/*hosts=*/std::set<std::string>(),
                                   /*count=*/1);
  base::RunLoop().RunUntilIdle();
}

}  // namespace
}  // namespace ntp_snippets
