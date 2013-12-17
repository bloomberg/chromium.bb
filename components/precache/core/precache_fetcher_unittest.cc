// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/precache/core/precache_fetcher.h"

#include <list>
#include <set>
#include <string>

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/message_loop/message_loop.h"
#include "components/precache/core/precache_switches.h"
#include "components/precache/core/proto/precache.pb.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "net/url_request/url_request_status.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace precache {

namespace {

class TestURLFetcherCallback {
 public:
  scoped_ptr<net::FakeURLFetcher> CreateURLFetcher(
      const GURL& url, net::URLFetcherDelegate* delegate,
      const std::string& response_data, net::HttpStatusCode response_code,
      net::URLRequestStatus::Status status) {
    scoped_ptr<net::FakeURLFetcher> fetcher(new net::FakeURLFetcher(
        url, delegate, response_data, response_code, status));

    if (response_code == net::HTTP_OK) {
      scoped_refptr<net::HttpResponseHeaders> download_headers =
          new net::HttpResponseHeaders("");
      download_headers->AddHeader("Content-Type: text/html");
      fetcher->set_response_headers(download_headers);
    }

    requested_urls_.insert(url);
    return fetcher.Pass();
  }

  const std::multiset<GURL>& requested_urls() const {
    return requested_urls_;
  }

 private:
  // Multiset with one entry for each URL requested.
  std::multiset<GURL> requested_urls_;
};

class TestPrecacheDelegate : public PrecacheFetcher::PrecacheDelegate {
 public:
  TestPrecacheDelegate() : was_on_done_called_(false) {}

  virtual void OnDone() OVERRIDE {
    was_on_done_called_ = true;
  }

  bool was_on_done_called() const {
    return was_on_done_called_;
  }

 private:
  bool was_on_done_called_;
};

class PrecacheFetcherTest : public testing::Test {
 public:
  PrecacheFetcherTest()
      : request_context_(new net::TestURLRequestContextGetter(
            base::MessageLoopProxy::current())),
        factory_(NULL, base::Bind(&TestURLFetcherCallback::CreateURLFetcher,
                                  base::Unretained(&url_callback_))) {}

 protected:
  base::MessageLoopForUI loop_;
  scoped_refptr<net::TestURLRequestContextGetter> request_context_;
  TestURLFetcherCallback url_callback_;
  net::FakeURLFetcherFactory factory_;
  TestPrecacheDelegate precache_delegate_;
};

const char kConfigURL[] = "http://config-url.com";
const char kManfiestURLPrefix[] = "http://manifest-url-prefix.com/";
const char kManifestFetchFailureURL[] =
    "http://manifest-url-prefix.com/"
    "http%253A%252F%252Fmanifest-fetch-failure.com%252F";
const char kBadManifestURL[] =
    "http://manifest-url-prefix.com/http%253A%252F%252Fbad-manifest.com%252F";
const char kGoodManifestURL[] =
    "http://manifest-url-prefix.com/http%253A%252F%252Fgood-manifest.com%252F";
const char kResourceFetchFailureURL[] = "http://resource-fetch-failure.com";
const char kGoodResourceURL[] = "http://good-resource.com";
const char kForcedStartingURLManifestURL[] =
    "http://manifest-url-prefix.com/"
    "http%253A%252F%252Fforced-starting-url.com%252F";

TEST_F(PrecacheFetcherTest, FullPrecache) {
  CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kPrecacheConfigSettingsURL, kConfigURL);
  CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kPrecacheManifestURLPrefix, kManfiestURLPrefix);

  std::list<GURL> starting_urls;
  starting_urls.push_back(GURL("http://manifest-fetch-failure.com"));
  starting_urls.push_back(GURL("http://bad-manifest.com"));
  starting_urls.push_back(GURL("http://good-manifest.com"));
  starting_urls.push_back(GURL("http://not-in-top-3.com"));

  PrecacheConfigurationSettings config;
  config.set_top_sites_count(3);
  config.add_forced_starting_url("http://forced-starting-url.com");
  // Duplicate starting URL, the manifest for this should only be fetched once.
  config.add_forced_starting_url("http://good-manifest.com");

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

  PrecacheFetcher precache_fetcher(starting_urls, request_context_.get(),
                                   &precache_delegate_);
  precache_fetcher.Start();

  base::MessageLoop::current()->RunUntilIdle();

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
}

TEST_F(PrecacheFetcherTest, ConfigFetchFailure) {
  CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kPrecacheConfigSettingsURL, kConfigURL);

  std::list<GURL> starting_urls(1, GURL("http://starting-url.com"));

  factory_.SetFakeResponse(GURL(kConfigURL), "",
                           net::HTTP_INTERNAL_SERVER_ERROR,
                           net::URLRequestStatus::FAILED);

  PrecacheFetcher precache_fetcher(starting_urls, request_context_.get(),
                                   &precache_delegate_);
  precache_fetcher.Start();

  base::MessageLoop::current()->RunUntilIdle();

  std::multiset<GURL> expected_requested_urls;
  expected_requested_urls.insert(GURL(kConfigURL));
  EXPECT_EQ(expected_requested_urls, url_callback_.requested_urls());

  EXPECT_TRUE(precache_delegate_.was_on_done_called());
}

TEST_F(PrecacheFetcherTest, BadConfig) {
  CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kPrecacheConfigSettingsURL, kConfigURL);

  std::list<GURL> starting_urls(1, GURL("http://starting-url.com"));

  factory_.SetFakeResponse(GURL(kConfigURL), "bad protobuf", net::HTTP_OK,
                           net::URLRequestStatus::SUCCESS);

  PrecacheFetcher precache_fetcher(starting_urls, request_context_.get(),
                                   &precache_delegate_);
  precache_fetcher.Start();

  base::MessageLoop::current()->RunUntilIdle();

  std::multiset<GURL> expected_requested_urls;
  expected_requested_urls.insert(GURL(kConfigURL));
  EXPECT_EQ(expected_requested_urls, url_callback_.requested_urls());

  EXPECT_TRUE(precache_delegate_.was_on_done_called());
}

TEST_F(PrecacheFetcherTest, Cancel) {
  CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kPrecacheConfigSettingsURL, kConfigURL);

  std::list<GURL> starting_urls(1, GURL("http://starting-url.com"));

  PrecacheConfigurationSettings config;
  config.set_top_sites_count(1);

  factory_.SetFakeResponse(GURL(kConfigURL), config.SerializeAsString(),
                           net::HTTP_OK, net::URLRequestStatus::SUCCESS);

  scoped_ptr<PrecacheFetcher> precache_fetcher(new PrecacheFetcher(
      starting_urls, request_context_.get(), &precache_delegate_));
  precache_fetcher->Start();

  // Destroy the PrecacheFetcher to cancel precaching. This should not cause
  // OnDone to be called on the precache delegate.
  precache_fetcher.reset();

  base::MessageLoop::current()->RunUntilIdle();

  std::multiset<GURL> expected_requested_urls;
  expected_requested_urls.insert(GURL(kConfigURL));
  EXPECT_EQ(expected_requested_urls, url_callback_.requested_urls());

  EXPECT_FALSE(precache_delegate_.was_on_done_called());
}

#if defined(PRECACHE_CONFIG_SETTINGS_URL)

// If the default precache configuration settings URL is defined, then test that
// it works with the PrecacheFetcher.
TEST_F(PrecacheFetcherTest, PrecacheUsingDefaultConfigSettingsURL) {
  std::list<GURL> starting_urls(1, GURL("http://starting-url.com"));

  PrecacheConfigurationSettings config;
  config.set_top_sites_count(0);

  factory_.SetFakeResponse(GURL(PRECACHE_CONFIG_SETTINGS_URL),
                           config.SerializeAsString(), net::HTTP_OK,
                           net::URLRequestStatus::SUCCESS);

  PrecacheFetcher precache_fetcher(starting_urls, request_context_.get(),
                                   &precache_delegate_);
  precache_fetcher.Start();

  base::MessageLoop::current()->RunUntilIdle();

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
  CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kPrecacheConfigSettingsURL, kConfigURL);

  std::list<GURL> starting_urls(1, GURL("http://starting-url.com"));

  PrecacheConfigurationSettings config;
  config.set_top_sites_count(1);

  GURL manifest_url(PRECACHE_MANIFEST_URL_PREFIX
                    "http%253A%252F%252Fstarting-url.com%252F");

  factory_.SetFakeResponse(GURL(kConfigURL), config.SerializeAsString(),
                           net::HTTP_OK, net::URLRequestStatus::SUCCESS);
  factory_.SetFakeResponse(manifest_url, PrecacheManifest().SerializeAsString(),
                           net::HTTP_OK, net::URLRequestStatus::SUCCESS);

  PrecacheFetcher precache_fetcher(starting_urls, request_context_.get(),
                                   &precache_delegate_);
  precache_fetcher.Start();

  base::MessageLoop::current()->RunUntilIdle();

  std::multiset<GURL> expected_requested_urls;
  expected_requested_urls.insert(GURL(kConfigURL));
  expected_requested_urls.insert(manifest_url);
  EXPECT_EQ(expected_requested_urls, url_callback_.requested_urls());

  EXPECT_TRUE(precache_delegate_.was_on_done_called());
}

#endif  // PRECACHE_MANIFEST_URL_PREFIX

}  // namespace

}  // namespace precache
