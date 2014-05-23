// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"
#include "chrome/browser/chromeos/geolocation/simple_geolocation_provider.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "net/url_request/url_fetcher_impl.h"
#include "net/url_request/url_request_status.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const int kRequestRetryIntervalMilliSeconds = 200;

// This should be different from default to prevent SimpleGeolocationRequest
// from modifying it.
const char kTestGeolocationProviderUrl[] =
    "https://localhost/geolocation/v1/geolocate?";

const char kSimpleResponseBody[] =
    "{\n"
    "  \"location\": {\n"
    "    \"lat\": 51.0,\n"
    "    \"lng\": -0.1\n"
    "  },\n"
    "  \"accuracy\": 1200.4\n"
    "}";
}  // anonymous namespace

namespace chromeos {

// This is helper class for net::FakeURLFetcherFactory.
class TestGeolocationAPIURLFetcherCallback {
 public:
  TestGeolocationAPIURLFetcherCallback(const GURL& url,
                                       const size_t require_retries,
                                       const std::string& response,
                                       SimpleGeolocationProvider* provider)
      : url_(url),
        require_retries_(require_retries),
        response_(response),
        factory_(NULL),
        attempts_(0),
        provider_(provider) {}

  scoped_ptr<net::FakeURLFetcher> CreateURLFetcher(
      const GURL& url,
      net::URLFetcherDelegate* delegate,
      const std::string& response_data,
      net::HttpStatusCode response_code,
      net::URLRequestStatus::Status status) {
    EXPECT_EQ(provider_->requests_.size(), 1U);

    SimpleGeolocationRequest* geolocation_request = provider_->requests_[0];

    const base::TimeDelta base_retry_interval =
        base::TimeDelta::FromMilliseconds(kRequestRetryIntervalMilliSeconds);
    geolocation_request->set_retry_sleep_on_server_error_for_testing(
        base_retry_interval);
    geolocation_request->set_retry_sleep_on_bad_response_for_testing(
        base_retry_interval);

    ++attempts_;
    if (attempts_ > require_retries_) {
      response_code = net::HTTP_OK;
      status = net::URLRequestStatus::SUCCESS;
      factory_->SetFakeResponse(url, response_, response_code, status);
    }
    scoped_ptr<net::FakeURLFetcher> fetcher(new net::FakeURLFetcher(
        url, delegate, response_, response_code, status));
    scoped_refptr<net::HttpResponseHeaders> download_headers =
        new net::HttpResponseHeaders(std::string());
    download_headers->AddHeader("Content-Type: application/json");
    fetcher->set_response_headers(download_headers);
    return fetcher.Pass();
  }

  void Initialize(net::FakeURLFetcherFactory* factory) {
    factory_ = factory;
    factory_->SetFakeResponse(url_,
                              std::string(),
                              net::HTTP_INTERNAL_SERVER_ERROR,
                              net::URLRequestStatus::FAILED);
  }

  size_t attempts() const { return attempts_; }

 private:
  const GURL url_;
  // Respond with OK on required retry attempt.
  const size_t require_retries_;
  std::string response_;
  net::FakeURLFetcherFactory* factory_;
  size_t attempts_;
  SimpleGeolocationProvider* provider_;

  DISALLOW_COPY_AND_ASSIGN(TestGeolocationAPIURLFetcherCallback);
};

// This implements fake Google MAPS Geolocation API remote endpoint.
// Response data is served to SimpleGeolocationProvider via
// net::FakeURLFetcher.
class GeolocationAPIFetcherFactory {
 public:
  GeolocationAPIFetcherFactory(const GURL& url,
                               const std::string& response,
                               const size_t require_retries,
                               SimpleGeolocationProvider* provider) {
    url_callback_.reset(new TestGeolocationAPIURLFetcherCallback(
        url, require_retries, response, provider));
    net::URLFetcherImpl::set_factory(NULL);
    fetcher_factory_.reset(new net::FakeURLFetcherFactory(
        NULL,
        base::Bind(&TestGeolocationAPIURLFetcherCallback::CreateURLFetcher,
                   base::Unretained(url_callback_.get()))));
    url_callback_->Initialize(fetcher_factory_.get());
  }

  size_t attempts() const { return url_callback_->attempts(); }

 private:
  scoped_ptr<TestGeolocationAPIURLFetcherCallback> url_callback_;
  scoped_ptr<net::FakeURLFetcherFactory> fetcher_factory_;

  DISALLOW_COPY_AND_ASSIGN(GeolocationAPIFetcherFactory);
};

class GeolocationReceiver {
 public:
  GeolocationReceiver() : server_error_(false) {}

  void OnRequestDone(const Geoposition& position,
                     bool server_error,
                     const base::TimeDelta elapsed) {
    position_ = position;
    server_error_ = server_error;
    elapsed_ = elapsed;

    message_loop_runner_->Quit();
  }

  void WaitUntilRequestDone() {
    message_loop_runner_.reset(new base::RunLoop);
    message_loop_runner_->Run();
  }

  const Geoposition& position() const { return position_; }
  bool server_error() const { return server_error_; }
  base::TimeDelta elapsed() const { return elapsed_; }

 private:
  Geoposition position_;
  bool server_error_;
  base::TimeDelta elapsed_;
  scoped_ptr<base::RunLoop> message_loop_runner_;
};

class SimpleGeolocationTest : public testing::Test {
 private:
  content::TestBrowserThreadBundle thread_bundle_;
};

TEST_F(SimpleGeolocationTest, ResponseOK) {
  SimpleGeolocationProvider provider(NULL, GURL(kTestGeolocationProviderUrl));

  GeolocationAPIFetcherFactory url_factory(GURL(kTestGeolocationProviderUrl),
                                           std::string(kSimpleResponseBody),
                                           0 /* require_retries */,
                                           &provider);

  GeolocationReceiver receiver;
  provider.RequestGeolocation(base::TimeDelta::FromSeconds(1),
                              base::Bind(&GeolocationReceiver::OnRequestDone,
                                         base::Unretained(&receiver)));
  receiver.WaitUntilRequestDone();

  EXPECT_EQ(
      "latitude=51.000000, longitude=-0.100000, accuracy=1200.400000, "
      "error_code=0, error_message='', status=1 (OK)",
      receiver.position().ToString());
  EXPECT_FALSE(receiver.server_error());
  EXPECT_EQ(1U, url_factory.attempts());
}

TEST_F(SimpleGeolocationTest, ResponseOKWithRetries) {
  SimpleGeolocationProvider provider(NULL, GURL(kTestGeolocationProviderUrl));

  GeolocationAPIFetcherFactory url_factory(GURL(kTestGeolocationProviderUrl),
                                           std::string(kSimpleResponseBody),
                                           3 /* require_retries */,
                                           &provider);

  GeolocationReceiver receiver;
  provider.RequestGeolocation(base::TimeDelta::FromSeconds(1),
                              base::Bind(&GeolocationReceiver::OnRequestDone,
                                         base::Unretained(&receiver)));
  receiver.WaitUntilRequestDone();
  EXPECT_EQ(
      "latitude=51.000000, longitude=-0.100000, accuracy=1200.400000, "
      "error_code=0, error_message='', status=1 (OK)",
      receiver.position().ToString());
  EXPECT_FALSE(receiver.server_error());
  EXPECT_EQ(4U, url_factory.attempts());
}

TEST_F(SimpleGeolocationTest, InvalidResponse) {
  SimpleGeolocationProvider provider(NULL, GURL(kTestGeolocationProviderUrl));

  GeolocationAPIFetcherFactory url_factory(GURL(kTestGeolocationProviderUrl),
                                           "invalid JSON string",
                                           0 /* require_retries */,
                                           &provider);

  GeolocationReceiver receiver;

  const int timeout_seconds = 1;
  size_t expected_retries = static_cast<size_t>(
      timeout_seconds * 1000 / kRequestRetryIntervalMilliSeconds);
  ASSERT_GE(expected_retries, 2U);

  provider.RequestGeolocation(base::TimeDelta::FromSeconds(timeout_seconds),
                              base::Bind(&GeolocationReceiver::OnRequestDone,
                                         base::Unretained(&receiver)));
  receiver.WaitUntilRequestDone();

  EXPECT_EQ(
      "latitude=200.000000, longitude=200.000000, accuracy=-1.000000, "
      "error_code=0, error_message='SimpleGeolocation provider at "
      "'https://localhost/' : JSONReader failed: Line: 1, column: 1, "
      "Unexpected token..', status=4 (TIMEOUT)",
      receiver.position().ToString());
  EXPECT_TRUE(receiver.server_error());
  EXPECT_GE(url_factory.attempts(), 2U);
  if (url_factory.attempts() > expected_retries + 1) {
    LOG(WARNING)
        << "SimpleGeolocationTest::InvalidResponse: Too many attempts ("
        << url_factory.attempts() << "), no more then " << expected_retries + 1
        << " expected.";
  }
  if (url_factory.attempts() < expected_retries - 1) {
    LOG(WARNING)
        << "SimpleGeolocationTest::InvalidResponse: Too little attempts ("
        << url_factory.attempts() << "), greater then " << expected_retries - 1
        << " expected.";
  }
}

}  // namespace chromeos
