// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/local_discovery/privet_url_fetcher.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::StrictMock;

namespace local_discovery {

namespace {

const char kSamplePrivetURL[] =
    "http://10.0.0.8:7676/privet/register?action=start";
const char kSamplePrivetToken[] = "MyToken";
const char kEmptyPrivetToken[] = "\"\"";

const char kSampleParsableJSON[] = "{ \"hello\" : 2 }";
const char kSampleUnparsableJSON[] = "{ \"hello\" : }";
const char kSampleJSONWithError[] = "{ \"error\" : \"unittest_example\" }";

class MockPrivetURLFetcherDelegate : public PrivetURLFetcher::Delegate {
 public:
  virtual void OnError(PrivetURLFetcher* fetcher,
                       PrivetURLFetcher::ErrorType error) OVERRIDE {
    OnErrorInternal(error);
  }

  MOCK_METHOD1(OnErrorInternal, void(PrivetURLFetcher::ErrorType error));

  virtual void OnParsedJson(PrivetURLFetcher* fetcher,
                            const base::DictionaryValue* value,
                            bool has_error) OVERRIDE {
    saved_value_.reset(value->DeepCopy());
    OnParsedJsonInternal(has_error);
  }

  MOCK_METHOD1(OnParsedJsonInternal, void(bool has_error));

  virtual void OnNeedPrivetToken(
      PrivetURLFetcher* fetcher,
      const PrivetURLFetcher::TokenCallback& callback) {
  }

  const DictionaryValue* saved_value() { return saved_value_.get(); }

 private:
  scoped_ptr<DictionaryValue> saved_value_;
};

class PrivetURLFetcherTest : public ::testing::Test {
 public:
  PrivetURLFetcherTest() {
    request_context_= new net::TestURLRequestContextGetter(
        base::MessageLoopProxy::current());
    privet_urlfetcher_.reset(new PrivetURLFetcher(
        kSamplePrivetToken,
        GURL(kSamplePrivetURL),
        net::URLFetcher::POST,
        request_context_.get(),
        &delegate_));
  }
  virtual ~PrivetURLFetcherTest() {
  }

 protected:
  base::MessageLoop loop_;
  scoped_refptr<net::TestURLRequestContextGetter> request_context_;
  net::TestURLFetcherFactory fetcher_factory_;
  scoped_ptr<PrivetURLFetcher> privet_urlfetcher_;
  StrictMock<MockPrivetURLFetcherDelegate> delegate_;
};

TEST_F(PrivetURLFetcherTest, FetchSuccess) {
  privet_urlfetcher_->Start();
  net::TestURLFetcher* fetcher = fetcher_factory_.GetFetcherByID(0);
  ASSERT_TRUE(fetcher != NULL);
  fetcher->SetResponseString(kSampleParsableJSON);
  fetcher->set_status(net::URLRequestStatus(net::URLRequestStatus::SUCCESS,
                                            net::OK));
  fetcher->set_response_code(200);

  EXPECT_CALL(delegate_, OnParsedJsonInternal(false));
  fetcher->delegate()->OnURLFetchComplete(fetcher);

  const base::DictionaryValue* value = delegate_.saved_value();
  int hello_value;
  ASSERT_TRUE(value != NULL);
  ASSERT_TRUE(value->GetInteger("hello", &hello_value));
  EXPECT_EQ(2, hello_value);
}

TEST_F(PrivetURLFetcherTest, URLFetcherError) {
  privet_urlfetcher_->Start();
  net::TestURLFetcher* fetcher = fetcher_factory_.GetFetcherByID(0);
  ASSERT_TRUE(fetcher != NULL);
  fetcher->SetResponseString(kSampleParsableJSON);
  fetcher->set_status(net::URLRequestStatus(net::URLRequestStatus::FAILED,
                                            net::ERR_TIMED_OUT));
  fetcher->set_response_code(-1);

  EXPECT_CALL(delegate_, OnErrorInternal(PrivetURLFetcher::URL_FETCH_ERROR));
  fetcher->delegate()->OnURLFetchComplete(fetcher);
}

TEST_F(PrivetURLFetcherTest, ResponseCodeError) {
  privet_urlfetcher_->Start();
  net::TestURLFetcher* fetcher = fetcher_factory_.GetFetcherByID(0);
  ASSERT_TRUE(fetcher != NULL);
  fetcher->SetResponseString(kSampleParsableJSON);
  fetcher->set_status(net::URLRequestStatus(net::URLRequestStatus::SUCCESS,
                                            net::OK));
  fetcher->set_response_code(404);

  EXPECT_CALL(delegate_,
              OnErrorInternal(PrivetURLFetcher::RESPONSE_CODE_ERROR));
  fetcher->delegate()->OnURLFetchComplete(fetcher);
}

TEST_F(PrivetURLFetcherTest, JsonParseError) {
  privet_urlfetcher_->Start();
  net::TestURLFetcher* fetcher = fetcher_factory_.GetFetcherByID(0);
  ASSERT_TRUE(fetcher != NULL);
  fetcher->SetResponseString(kSampleUnparsableJSON);
  fetcher->set_status(net::URLRequestStatus(net::URLRequestStatus::SUCCESS,
                                            net::OK));
  fetcher->set_response_code(200);

  EXPECT_CALL(delegate_,
              OnErrorInternal(PrivetURLFetcher::JSON_PARSE_ERROR));
  fetcher->delegate()->OnURLFetchComplete(fetcher);
}

TEST_F(PrivetURLFetcherTest, Header) {
  privet_urlfetcher_->Start();
  net::TestURLFetcher* fetcher = fetcher_factory_.GetFetcherByID(0);
  ASSERT_TRUE(fetcher != NULL);
  net::HttpRequestHeaders headers;
  fetcher->GetExtraRequestHeaders(&headers);

  std::string header_token;
  ASSERT_TRUE(headers.GetHeader("X-Privet-Token", &header_token));
  EXPECT_EQ(kSamplePrivetToken, header_token);
}

TEST_F(PrivetURLFetcherTest, Header2) {
  privet_urlfetcher_.reset(new PrivetURLFetcher(
      "",
      GURL(kSamplePrivetURL),
      net::URLFetcher::POST,
      request_context_.get(),
      &delegate_));

  privet_urlfetcher_->AllowEmptyPrivetToken();
  privet_urlfetcher_->Start();

  net::TestURLFetcher* fetcher = fetcher_factory_.GetFetcherByID(0);
  ASSERT_TRUE(fetcher != NULL);
  net::HttpRequestHeaders headers;
  fetcher->GetExtraRequestHeaders(&headers);

  std::string header_token;
  ASSERT_TRUE(headers.GetHeader("X-Privet-Token", &header_token));
  EXPECT_EQ(kEmptyPrivetToken, header_token);
}

TEST_F(PrivetURLFetcherTest, FetchHasError) {
  privet_urlfetcher_->Start();
  net::TestURLFetcher* fetcher = fetcher_factory_.GetFetcherByID(0);
  ASSERT_TRUE(fetcher != NULL);
  fetcher->SetResponseString(kSampleJSONWithError);
  fetcher->set_status(net::URLRequestStatus(net::URLRequestStatus::SUCCESS,
                                            net::OK));
  fetcher->set_response_code(200);

  EXPECT_CALL(delegate_, OnParsedJsonInternal(true));
  fetcher->delegate()->OnURLFetchComplete(fetcher);
}

}  // namespace

}  // namespace local_discovery
