// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"

#include <string>

#include "chrome/browser/extensions/api/push_messaging/obfuscated_gaia_id_fetcher.h"
#include "chrome/browser/extensions/api/push_messaging/push_messaging_api.h"
#include "net/url_request/test_url_fetcher_factory.h"

namespace {

static const char kGoodData[] = "{ \"id\" : \"My-channel-id\" }";
static const char kBadJsonData[] = "I am not a JSON string";
static const char kDictionaryMissingIdData[] = "{ \"foo\" : \"bar\" }";
static const char kNonDictionaryJsonData[] = "{ 0.5 }";

// Delegate class for catching notifications from the ObfuscatedGaiaIdFetcher.
class TestDelegate : public extensions::ObfuscatedGaiaIdFetcher::Delegate {
 public:
  virtual void OnObfuscatedGaiaIdFetchSuccess(
      const std::string& obfuscated_id) OVERRIDE {
    succeeded_ = true;
  }
  virtual void OnObfuscatedGaiaIdFetchFailure(
      const GoogleServiceAuthError& error) OVERRIDE {
    failed_ = true;
  }
  TestDelegate() : succeeded_(false), failed_(false) {}
  virtual ~TestDelegate() {}
  bool succeeded() const { return succeeded_; }
  bool failed() const { return failed_; }

 private:
  bool succeeded_;
  bool failed_;
  DISALLOW_COPY_AND_ASSIGN(TestDelegate);
};

}  // namespace

namespace extensions {

TEST(ObfuscatedGaiaIdFetcherTest, ParseResponse) {
  // Try a good response string.
  std::string channel_id_out1;
  bool ret1 = ObfuscatedGaiaIdFetcher::ParseResponse(
      kGoodData, &channel_id_out1);
  EXPECT_EQ("My-channel-id", channel_id_out1);
  EXPECT_TRUE(ret1);

  // Try badly formatted JSON.
  std::string channel_id_out2;
  bool ret2 = ObfuscatedGaiaIdFetcher::ParseResponse(
      kBadJsonData, &channel_id_out2);
  EXPECT_TRUE(channel_id_out2.empty());
  EXPECT_FALSE(ret2);

  // Try a JSON dictionary with no "id" value.
  std::string channel_id_out3;
  bool ret3 = ObfuscatedGaiaIdFetcher::ParseResponse(
      kDictionaryMissingIdData, &channel_id_out3);
  EXPECT_TRUE(channel_id_out3.empty());
  EXPECT_FALSE(ret3);

  // Try JSON, but not a dictionary.
  std::string channel_id_out4;
  bool ret4 = ObfuscatedGaiaIdFetcher::ParseResponse(
      kNonDictionaryJsonData, &channel_id_out4);
  EXPECT_TRUE(channel_id_out4.empty());
  EXPECT_FALSE(ret4);
}

TEST(ObfuscatedGaiaIdFetcherTest, ProcessApiCallSuccess) {
  TestDelegate delegate;
  ObfuscatedGaiaIdFetcher fetcher(NULL, &delegate, std::string());

  net::TestURLFetcher url_fetcher(1, GURL("http://www.google.com"), NULL);
  url_fetcher.set_status(
      net::URLRequestStatus(net::URLRequestStatus::SUCCESS, 0));
  url_fetcher.SetResponseString(kGoodData);

  // Test the happy path.
  fetcher.ProcessApiCallSuccess(&url_fetcher);
  EXPECT_TRUE(delegate.succeeded());
}

TEST(ObfuscatedGaiaIdFetcherTest, ProcessApiCallFailure) {
  TestDelegate delegate;
  ObfuscatedGaiaIdFetcher fetcher(NULL, &delegate, std::string());

  net::TestURLFetcher url_fetcher(1, GURL("http://www.google.com"), NULL);
  url_fetcher.set_status(
      net::URLRequestStatus(net::URLRequestStatus::SUCCESS, 0));
  url_fetcher.SetResponseString(kDictionaryMissingIdData);

  // Test with bad data, ensure it fails.
  fetcher.ProcessApiCallSuccess(&url_fetcher);
  EXPECT_TRUE(delegate.failed());

  // TODO(petewil): add case for when the base class fails and calls
  // ProcessApiCallFailure
}

}  // namespace extensions
