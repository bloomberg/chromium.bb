// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/precache/core/precache_fetcher.h"

#include <set>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/run_loop.h"
#include "base/test/histogram_tester.h"
#include "base/thread_task_runner_handle.h"
#include "components/precache/core/precache_switches.h"
#include "components/precache/core/proto/precache.pb.h"
#include "net/base/load_flags.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "net/url_request/url_request_status.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace precache {

namespace {

using ::testing::_;

const char kConfigURL[] = "http://config-url.com";
const char kManifestURLPrefix[] = "http://manifest-url-prefix.com/";
const char kCustomConfigURL[] = "http://custom-config-url.com";
const char kCustomManifestURLPrefix[] =
    "http://custom-manifest-url-prefix.com/";
const char kManifestFetchFailureURL[] =
    "http://manifest-url-prefix.com/manifest-fetch-failure.com";
const char kBadManifestURL[] =
    "http://manifest-url-prefix.com/bad-manifest.com";
const char kGoodManifestURL[] =
    "http://manifest-url-prefix.com/good-manifest.com";
const char kCustomGoodManifestURL[] =
    "http://custom-manifest-url-prefix.com/good-manifest.com";
const char kResourceFetchFailureURL[] = "http://resource-fetch-failure.com";
const char kGoodResourceURL[] = "http://good-resource.com";
const char kForcedStartingURLManifestURL[] =
    "http://manifest-url-prefix.com/forced-starting-url.com";

class TestURLFetcherCallback {
 public:
  TestURLFetcherCallback() : total_response_bytes_(0) {}

  scoped_ptr<net::FakeURLFetcher> CreateURLFetcher(
      const GURL& url, net::URLFetcherDelegate* delegate,
      const std::string& response_data, net::HttpStatusCode response_code,
      net::URLRequestStatus::Status status) {
    scoped_ptr<net::FakeURLFetcher> fetcher(new net::FakeURLFetcher(
        url, delegate, response_data, response_code, status));

    total_response_bytes_ += response_data.size();
    requested_urls_.insert(url);

    return fetcher;
  }

  const std::multiset<GURL>& requested_urls() const {
    return requested_urls_;
  }

  int total_response_bytes() const { return total_response_bytes_; }

 private:
  // Multiset with one entry for each URL requested.
  std::multiset<GURL> requested_urls_;
  int total_response_bytes_;
};

class TestPrecacheDelegate : public PrecacheFetcher::PrecacheDelegate {
 public:
  TestPrecacheDelegate() : was_on_done_called_(false) {}

  void OnDone() override { was_on_done_called_ = true; }

  bool was_on_done_called() const {
    return was_on_done_called_;
  }

 private:
  bool was_on_done_called_;
};

class MockURLFetcherFactory : public net::URLFetcherFactory {
 public:
  typedef net::URLFetcher* DoURLFetcher(
      int id,
      const GURL& url,
      net::URLFetcher::RequestType request_type,
      net::URLFetcherDelegate* delegate);

  scoped_ptr<net::URLFetcher> CreateURLFetcher(
      int id,
      const GURL& url,
      net::URLFetcher::RequestType request_type,
      net::URLFetcherDelegate* delegate) override {
    return make_scoped_ptr(DoCreateURLFetcher(id, url, request_type, delegate));
  }

  // The method to mock out, instead of CreateURLFetcher. This is necessary
  // because gmock can't handle move-only types such as scoped_ptr.
  MOCK_METHOD4(DoCreateURLFetcher, DoURLFetcher);

  // A fake successful response. When the action runs, it saves off a pointer to
  // the FakeURLFetcher in its output parameter for later inspection.
  testing::Action<DoURLFetcher> RespondWith(const std::string& body,
                                            net::FakeURLFetcher** fetcher) {
    return RespondWith(body, [](net::FakeURLFetcher* fetcher) {
      fetcher->set_response_code(net::HTTP_OK);
    }, fetcher);
  }

  // A fake custom response. When the action runs, it runs the given modifier to
  // customize the FakeURLFetcher, and then saves off a pointer to the
  // FakeURLFetcher in its output parameter for later inspection. The modifier
  // should be a functor that takes a FakeURLFetcher* and returns void.
  template <typename F>
  testing::Action<DoURLFetcher> RespondWith(const std::string& body,
                                            F modifier,
                                            net::FakeURLFetcher** fetcher) {
    return testing::MakeAction(
        new FakeResponseAction<F>(body, modifier, fetcher));
  }

 private:
  template <typename F>
  class FakeResponseAction : public testing::ActionInterface<DoURLFetcher> {
   public:
    FakeResponseAction(const std::string& body,
                       F modifier,
                       net::FakeURLFetcher** fetcher)
        : body_(body), modifier_(modifier), fetcher_(fetcher) {}

    net::URLFetcher* Perform(
        const testing::tuple<int,
                             const GURL&,
                             net::URLFetcher::RequestType,
                             net::URLFetcherDelegate*>& args) {
      *fetcher_ = new net::FakeURLFetcher(
          testing::get<1>(args), testing::get<3>(args), body_, net::HTTP_OK,
          net::URLRequestStatus::SUCCESS);
      modifier_(*fetcher_);
      return *fetcher_;
    }

   private:
    std::string body_;
    F modifier_;
    net::FakeURLFetcher** fetcher_;
  };
};

class PrecacheFetcherFetcherTest : public testing::Test {
 public:
  PrecacheFetcherFetcherTest()
      : request_context_(new net::TestURLRequestContextGetter(
            base::ThreadTaskRunnerHandle::Get())),
        scoped_url_fetcher_factory_(&factory_),
        callback_(base::Bind(&PrecacheFetcherFetcherTest::Callback,
                             base::Unretained(this))),
        callback_called_(false) {}

  void Callback(const net::URLFetcher&) { callback_called_ = true; }

 protected:
  base::MessageLoopForUI loop_;
  scoped_refptr<net::TestURLRequestContextGetter> request_context_;
  MockURLFetcherFactory factory_;
  net::ScopedURLFetcherFactory scoped_url_fetcher_factory_;
  base::Callback<void(const net::URLFetcher&)> callback_;
  bool callback_called_;
};

TEST_F(PrecacheFetcherFetcherTest, Config) {
  GURL url(kConfigURL);

  net::FakeURLFetcher* fetcher = nullptr;
  EXPECT_CALL(factory_, DoCreateURLFetcher(_, url, net::URLFetcher::GET, _))
      .WillOnce(factory_.RespondWith("", &fetcher));

  PrecacheFetcher::Fetcher precache_fetcher(
      request_context_.get(), url, callback_, false /* is_resource_request */);

  loop_.RunUntilIdle();

  ASSERT_NE(nullptr, fetcher);
  EXPECT_EQ(kNoTracking, fetcher->GetLoadFlags());

  EXPECT_EQ(true, callback_called_);
}

TEST_F(PrecacheFetcherFetcherTest, ResourceNotInCache) {
  GURL url(kGoodResourceURL);

  net::FakeURLFetcher *fetcher1 = nullptr, *fetcher2 = nullptr;
  EXPECT_CALL(factory_, DoCreateURLFetcher(_, url, net::URLFetcher::GET, _))
      .WillOnce(factory_.RespondWith(
          "",
          [](net::FakeURLFetcher* fetcher) {
            fetcher->set_status(net::URLRequestStatus(
                net::URLRequestStatus::FAILED, net::ERR_CACHE_MISS));
          },
          &fetcher1))
      .WillOnce(factory_.RespondWith("", &fetcher2));

  PrecacheFetcher::Fetcher precache_fetcher(
      request_context_.get(), url, callback_, true /* is_resource_request */);

  loop_.RunUntilIdle();

  ASSERT_NE(nullptr, fetcher1);
  EXPECT_EQ(net::LOAD_ONLY_FROM_CACHE | kNoTracking, fetcher1->GetLoadFlags());
  ASSERT_NE(nullptr, fetcher2);
  EXPECT_EQ(net::LOAD_VALIDATE_CACHE | kNoTracking, fetcher2->GetLoadFlags());

  EXPECT_EQ(true, callback_called_);
}

TEST_F(PrecacheFetcherFetcherTest, ResourceHasStrongValidators) {
  GURL url(kGoodResourceURL);

  net::FakeURLFetcher *fetcher1 = nullptr, *fetcher2 = nullptr;
  EXPECT_CALL(factory_, DoCreateURLFetcher(_, url, net::URLFetcher::GET, _))
      .WillOnce(factory_.RespondWith(
          "",
          [](net::FakeURLFetcher* fetcher) {
            std::string raw_headers("HTTP/1.1 200 OK\0ETag: foo\0\0", 27);
            fetcher->set_response_headers(
                make_scoped_refptr(new net::HttpResponseHeaders(raw_headers)));
          },
          &fetcher1))
      .WillOnce(factory_.RespondWith("", &fetcher2));

  PrecacheFetcher::Fetcher precache_fetcher(
      request_context_.get(), url, callback_, true /* is_resource_request */);

  loop_.RunUntilIdle();

  ASSERT_NE(nullptr, fetcher1);
  EXPECT_EQ(net::LOAD_ONLY_FROM_CACHE | kNoTracking, fetcher1->GetLoadFlags());
  ASSERT_NE(nullptr, fetcher2);
  EXPECT_EQ(net::LOAD_VALIDATE_CACHE | kNoTracking, fetcher2->GetLoadFlags());

  EXPECT_EQ(true, callback_called_);
}

TEST_F(PrecacheFetcherFetcherTest, ResourceHasNoValidators) {
  GURL url(kGoodResourceURL);

  net::FakeURLFetcher* fetcher;
  EXPECT_CALL(factory_, DoCreateURLFetcher(_, url, net::URLFetcher::GET, _))
      .WillOnce(factory_.RespondWith("", &fetcher));

  PrecacheFetcher::Fetcher precache_fetcher(
      request_context_.get(), url, callback_, true /* is_resource_request */);

  loop_.RunUntilIdle();

  EXPECT_EQ(net::LOAD_ONLY_FROM_CACHE | kNoTracking, fetcher->GetLoadFlags());

  EXPECT_EQ(true, callback_called_);
}

class PrecacheFetcherTest : public testing::Test {
 public:
  PrecacheFetcherTest()
      : request_context_(new net::TestURLRequestContextGetter(
            base::ThreadTaskRunnerHandle::Get())),
        factory_(NULL,
                 base::Bind(&TestURLFetcherCallback::CreateURLFetcher,
                            base::Unretained(&url_callback_))),
        expected_total_response_bytes_(0) {}

 protected:
  void SetDefaultFlags() {
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        switches::kPrecacheConfigSettingsURL, kConfigURL);
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        switches::kPrecacheManifestURLPrefix, kManifestURLPrefix);
  }

  base::MessageLoopForUI loop_;
  scoped_refptr<net::TestURLRequestContextGetter> request_context_;
  TestURLFetcherCallback url_callback_;
  net::FakeURLFetcherFactory factory_;
  TestPrecacheDelegate precache_delegate_;
  int expected_total_response_bytes_;
};

TEST_F(PrecacheFetcherTest, FullPrecache) {
  SetDefaultFlags();

  std::vector<std::string> starting_hosts;
  starting_hosts.push_back("manifest-fetch-failure.com");
  starting_hosts.push_back("bad-manifest.com");
  starting_hosts.push_back("good-manifest.com");
  starting_hosts.push_back("not-in-top-3.com");

  PrecacheConfigurationSettings config;
  config.set_top_sites_count(3);
  config.add_forced_site("forced-starting-url.com");
  // Duplicate starting URL, the manifest for this should only be fetched once.
  config.add_forced_site("good-manifest.com");

  PrecacheManifest good_manifest;
  good_manifest.add_resource()->set_url(kResourceFetchFailureURL);
  good_manifest.add_resource();  // Resource with no URL, should not be fetched.
  good_manifest.add_resource()->set_url(kGoodResourceURL);

  factory_.SetFakeResponse(GURL(kConfigURL), config.SerializeAsString(),
                           net::HTTP_OK, net::URLRequestStatus::SUCCESS);
  factory_.SetFakeResponse(GURL(kManifestFetchFailureURL), "",
                           net::HTTP_INTERNAL_SERVER_ERROR,
                           net::URLRequestStatus::FAILED);
  factory_.SetFakeResponse(GURL(kBadManifestURL), "bad protobuf", net::HTTP_OK,
                           net::URLRequestStatus::SUCCESS);
  factory_.SetFakeResponse(GURL(kGoodManifestURL),
                           good_manifest.SerializeAsString(), net::HTTP_OK,
                           net::URLRequestStatus::SUCCESS);
  factory_.SetFakeResponse(GURL(kResourceFetchFailureURL),
                           "", net::HTTP_INTERNAL_SERVER_ERROR,
                           net::URLRequestStatus::FAILED);
  factory_.SetFakeResponse(GURL(kGoodResourceURL), "good", net::HTTP_OK,
                           net::URLRequestStatus::SUCCESS);
  factory_.SetFakeResponse(GURL(kForcedStartingURLManifestURL),
                           PrecacheManifest().SerializeAsString(), net::HTTP_OK,
                           net::URLRequestStatus::SUCCESS);

  base::HistogramTester histogram;

  {
    PrecacheFetcher precache_fetcher(starting_hosts, request_context_.get(),
                                     GURL(), std::string(),
                                     &precache_delegate_);
    precache_fetcher.Start();

    loop_.RunUntilIdle();

    // Destroy the PrecacheFetcher after it has finished, to record metrics.
  }

  std::multiset<GURL> expected_requested_urls;
  expected_requested_urls.insert(GURL(kConfigURL));
  expected_requested_urls.insert(GURL(kManifestFetchFailureURL));
  expected_requested_urls.insert(GURL(kBadManifestURL));
  expected_requested_urls.insert(GURL(kGoodManifestURL));
  expected_requested_urls.insert(GURL(kResourceFetchFailureURL));
  expected_requested_urls.insert(GURL(kGoodResourceURL));
  expected_requested_urls.insert(GURL(kForcedStartingURLManifestURL));

  EXPECT_EQ(expected_requested_urls, url_callback_.requested_urls());

  EXPECT_TRUE(precache_delegate_.was_on_done_called());

  histogram.ExpectUniqueSample("Precache.Fetch.PercentCompleted", 100, 1);
  histogram.ExpectUniqueSample("Precache.Fetch.ResponseBytes.Total",
                               url_callback_.total_response_bytes(), 1);
  histogram.ExpectTotalCount("Precache.Fetch.TimeToComplete", 1);
}

TEST_F(PrecacheFetcherTest, CustomURLs) {
  SetDefaultFlags();

  std::vector<std::string> starting_hosts;
  starting_hosts.push_back("good-manifest.com");

  PrecacheConfigurationSettings config;

  PrecacheManifest good_manifest;
  good_manifest.add_resource()->set_url(kGoodResourceURL);

  factory_.SetFakeResponse(GURL(kCustomConfigURL), config.SerializeAsString(),
                           net::HTTP_OK, net::URLRequestStatus::SUCCESS);
  factory_.SetFakeResponse(GURL(kCustomGoodManifestURL),
                           good_manifest.SerializeAsString(), net::HTTP_OK,
                           net::URLRequestStatus::SUCCESS);
  factory_.SetFakeResponse(GURL(kGoodResourceURL), "good", net::HTTP_OK,
                           net::URLRequestStatus::SUCCESS);

  PrecacheFetcher precache_fetcher(
      starting_hosts, request_context_.get(), GURL(kCustomConfigURL),
      kCustomManifestURLPrefix, &precache_delegate_);
  precache_fetcher.Start();

  loop_.RunUntilIdle();

  std::multiset<GURL> expected_requested_urls;
  expected_requested_urls.insert(GURL(kCustomConfigURL));
  expected_requested_urls.insert(GURL(kCustomGoodManifestURL));
  expected_requested_urls.insert(GURL(kGoodResourceURL));

  EXPECT_EQ(expected_requested_urls, url_callback_.requested_urls());

  EXPECT_TRUE(precache_delegate_.was_on_done_called());
}

TEST_F(PrecacheFetcherTest, ConfigFetchFailure) {
  SetDefaultFlags();

  std::vector<std::string> starting_hosts(1, "good-manifest.com");

  factory_.SetFakeResponse(GURL(kConfigURL), "",
                           net::HTTP_INTERNAL_SERVER_ERROR,
                           net::URLRequestStatus::FAILED);
  factory_.SetFakeResponse(GURL(kGoodManifestURL), "", net::HTTP_OK,
                           net::URLRequestStatus::SUCCESS);

  PrecacheFetcher precache_fetcher(starting_hosts, request_context_.get(),
                                   GURL(), std::string(), &precache_delegate_);
  precache_fetcher.Start();

  loop_.RunUntilIdle();

  std::multiset<GURL> expected_requested_urls;
  expected_requested_urls.insert(GURL(kConfigURL));
  expected_requested_urls.insert(GURL(kGoodManifestURL));
  EXPECT_EQ(expected_requested_urls, url_callback_.requested_urls());

  EXPECT_TRUE(precache_delegate_.was_on_done_called());
}

TEST_F(PrecacheFetcherTest, BadConfig) {
  SetDefaultFlags();

  std::vector<std::string> starting_hosts(1, "good-manifest.com");

  factory_.SetFakeResponse(GURL(kConfigURL), "bad protobuf", net::HTTP_OK,
                           net::URLRequestStatus::SUCCESS);
  factory_.SetFakeResponse(GURL(kGoodManifestURL), "", net::HTTP_OK,
                           net::URLRequestStatus::SUCCESS);

  PrecacheFetcher precache_fetcher(starting_hosts, request_context_.get(),
                                   GURL(), std::string(), &precache_delegate_);
  precache_fetcher.Start();

  loop_.RunUntilIdle();

  std::multiset<GURL> expected_requested_urls;
  expected_requested_urls.insert(GURL(kConfigURL));
  expected_requested_urls.insert(GURL(kGoodManifestURL));
  EXPECT_EQ(expected_requested_urls, url_callback_.requested_urls());

  EXPECT_TRUE(precache_delegate_.was_on_done_called());
}

TEST_F(PrecacheFetcherTest, Cancel) {
  SetDefaultFlags();

  std::vector<std::string> starting_hosts(1, "starting-url.com");

  PrecacheConfigurationSettings config;
  config.set_top_sites_count(1);

  factory_.SetFakeResponse(GURL(kConfigURL), config.SerializeAsString(),
                           net::HTTP_OK, net::URLRequestStatus::SUCCESS);

  base::HistogramTester histogram;

  {
    PrecacheFetcher precache_fetcher(starting_hosts, request_context_.get(),
                                     GURL(), std::string(),
                                     &precache_delegate_);
    precache_fetcher.Start();

    // Destroy the PrecacheFetcher, to cancel precaching and record metrics.
    // This should not cause OnDone to be called on the precache delegate.
  }

  loop_.RunUntilIdle();

  std::multiset<GURL> expected_requested_urls;
  expected_requested_urls.insert(GURL(kConfigURL));
  EXPECT_EQ(expected_requested_urls, url_callback_.requested_urls());

  EXPECT_FALSE(precache_delegate_.was_on_done_called());

  histogram.ExpectUniqueSample("Precache.Fetch.PercentCompleted", 0, 1);
  histogram.ExpectUniqueSample("Precache.Fetch.ResponseBytes.Total", 0, 1);
  histogram.ExpectTotalCount("Precache.Fetch.TimeToComplete", 0);
}

#if defined(PRECACHE_CONFIG_SETTINGS_URL)

// If the default precache configuration settings URL is defined, then test that
// it works with the PrecacheFetcher.
TEST_F(PrecacheFetcherTest, PrecacheUsingDefaultConfigSettingsURL) {
  std::vector<std::string> starting_hosts(1, "starting-url.com");

  PrecacheConfigurationSettings config;
  config.set_top_sites_count(0);

  factory_.SetFakeResponse(GURL(PRECACHE_CONFIG_SETTINGS_URL),
                           config.SerializeAsString(), net::HTTP_OK,
                           net::URLRequestStatus::SUCCESS);

  PrecacheFetcher precache_fetcher(starting_hosts, request_context_.get(),
                                   GURL(), std::string(), &precache_delegate_);
  precache_fetcher.Start();

  loop_.RunUntilIdle();

  std::multiset<GURL> expected_requested_urls;
  expected_requested_urls.insert(GURL(PRECACHE_CONFIG_SETTINGS_URL));
  EXPECT_EQ(expected_requested_urls, url_callback_.requested_urls());

  EXPECT_TRUE(precache_delegate_.was_on_done_called());
}

#endif  // PRECACHE_CONFIG_SETTINGS_URL

#if defined(PRECACHE_MANIFEST_URL_PREFIX)

// If the default precache manifest URL prefix is defined, then test that it
// works with the PrecacheFetcher.
TEST_F(PrecacheFetcherTest, PrecacheUsingDefaultManifestURLPrefix) {
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kPrecacheConfigSettingsURL, kConfigURL);

  std::vector<std::string> starting_hosts(1, "starting-url.com");

  PrecacheConfigurationSettings config;
  config.set_top_sites_count(1);

  GURL manifest_url(PRECACHE_MANIFEST_URL_PREFIX "starting-url.com");

  factory_.SetFakeResponse(GURL(kConfigURL), config.SerializeAsString(),
                           net::HTTP_OK, net::URLRequestStatus::SUCCESS);
  factory_.SetFakeResponse(manifest_url, PrecacheManifest().SerializeAsString(),
                           net::HTTP_OK, net::URLRequestStatus::SUCCESS);

  PrecacheFetcher precache_fetcher(starting_hosts, request_context_.get(),
                                   GURL(), std::string(), &precache_delegate_);
  precache_fetcher.Start();

  loop_.RunUntilIdle();

  std::multiset<GURL> expected_requested_urls;
  expected_requested_urls.insert(GURL(kConfigURL));
  expected_requested_urls.insert(manifest_url);
  EXPECT_EQ(expected_requested_urls, url_callback_.requested_urls());

  EXPECT_TRUE(precache_delegate_.was_on_done_called());
}

#endif  // PRECACHE_MANIFEST_URL_PREFIX

}  // namespace

}  // namespace precache
