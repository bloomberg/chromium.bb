// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/json/json_reader.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "components/ntp_snippets/ntp_snippet.h"
#include "components/ntp_snippets/ntp_snippets_fetcher.h"
#include "components/ntp_snippets/ntp_snippets_service.h"
#include "components/prefs/testing_pref_service.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ntp_snippets {

namespace {

std::string GetTestJson(const std::string& content_creation_time_str,
                        const std::string& expiry_time_str) {
  char json_str_format[] =
      "{ \"recos\": [ "
      "{ \"contentInfo\": {"
      "\"url\" : \"http://localhost/foobar\","
      "\"site_title\" : \"Site Title\","
      "\"favicon_url\" : \"http://localhost/favicon\","
      "\"title\" : \"Title\","
      "\"snippet\" : \"Snippet\","
      "\"thumbnailUrl\" : \"http://localhost/salient_image\","
      "\"creationTimestampSec\" : \"%s\","
      "\"expiryTimestampSec\" : \"%s\""
      "}}"
      "]}";

  return base::StringPrintf(json_str_format, content_creation_time_str.c_str(),
                            expiry_time_str.c_str());
}

std::string GetTestJson(const std::string& content_creation_time_str) {
  int64_t expiry_time =
      (base::Time::Now() + base::TimeDelta::FromHours(1)).ToJavaTime() /
      base::Time::kMillisecondsPerSecond;

  return GetTestJson(content_creation_time_str,
                     base::Int64ToString(expiry_time));
}

std::string GetTestJson(int64_t content_creation_time_sec) {
  int64_t expiry_time =
      (base::Time::Now() + base::TimeDelta::FromHours(1)).ToJavaTime() /
      base::Time::kMillisecondsPerSecond;

  return GetTestJson(base::Int64ToString(content_creation_time_sec),
                     base::Int64ToString(expiry_time));
}

std::string GetTestExpiredJson(int64_t content_creation_time_sec) {
  int64_t expiry_time =
      (base::Time::Now()).ToJavaTime() / base::Time::kMillisecondsPerSecond;

  return GetTestJson(base::Int64ToString(content_creation_time_sec),
                     base::Int64ToString(expiry_time));
}

void ParseJson(
    const std::string& json,
    const ntp_snippets::NTPSnippetsService::SuccessCallback& success_callback,
    const ntp_snippets::NTPSnippetsService::ErrorCallback& error_callback) {
  base::JSONReader json_reader;
  scoped_ptr<base::Value> value = json_reader.ReadToValue(json);
  if (value) {
    success_callback.Run(std::move(value));
  } else {
    error_callback.Run(json_reader.GetErrorMessage());
  }
}

}  // namespace

class NTPSnippetsServiceTest : public testing::Test {
 public:
  NTPSnippetsServiceTest()
      : pref_service_(new TestingPrefServiceSimple()) {}
  ~NTPSnippetsServiceTest() override {}

  void SetUp() override {
    NTPSnippetsService::RegisterProfilePrefs(pref_service_->registry());

    CreateSnippetsService();
  }

  void CreateSnippetsService() {
    scoped_refptr<base::SingleThreadTaskRunner> task_runner(
        base::ThreadTaskRunnerHandle::Get());
    scoped_refptr<net::TestURLRequestContextGetter> request_context_getter =
        new net::TestURLRequestContextGetter(task_runner.get());

    service_.reset(new NTPSnippetsService(
        pref_service_.get(), nullptr, task_runner, std::string("fr"), nullptr,
        make_scoped_ptr(new NTPSnippetsFetcher(
            task_runner, std::move(request_context_getter), true)),
        base::Bind(&ParseJson)));
  }

 protected:
  NTPSnippetsService* service() {
    return service_.get();
  }

  bool LoadFromJSONString(const std::string& json) {
    scoped_ptr<base::Value> value = base::JSONReader::Read(json);
    if (!value)
      return false;

    return service_->LoadFromValue(*value);
  }

 private:
  base::MessageLoop message_loop_;
  scoped_ptr<TestingPrefServiceSimple> pref_service_;
  scoped_ptr<NTPSnippetsService> service_;

  DISALLOW_COPY_AND_ASSIGN(NTPSnippetsServiceTest);
};

TEST_F(NTPSnippetsServiceTest, Loop) {
  std::string json_str(
      "{ \"recos\": [ "
      "{ \"contentInfo\": { \"url\" : \"http://localhost/foobar\" }}"
      "]}");
  ASSERT_TRUE(LoadFromJSONString(json_str));

  // The same for loop without the '&' should not compile.
  for (auto& snippet : *service()) {
    // Snippet here is a const.
    EXPECT_EQ(snippet.url(), GURL("http://localhost/foobar"));
  }
  // Without the const, this should not compile.
  for (const NTPSnippet& snippet : *service()) {
    EXPECT_EQ(snippet.url(), GURL("http://localhost/foobar"));
  }
}

TEST_F(NTPSnippetsServiceTest, Full) {
  std::string json_str(GetTestJson(1448459205));

  ASSERT_TRUE(LoadFromJSONString(json_str));
  EXPECT_EQ(service()->size(), 1u);

  // The same for loop without the '&' should not compile.
  for (auto& snippet : *service()) {
    // Snippet here is a const.
    EXPECT_EQ(snippet.url(), GURL("http://localhost/foobar"));
    EXPECT_EQ(snippet.site_title(), "Site Title");
    EXPECT_EQ(snippet.favicon_url(), GURL("http://localhost/favicon"));
    EXPECT_EQ(snippet.title(), "Title");
    EXPECT_EQ(snippet.snippet(), "Snippet");
    EXPECT_EQ(snippet.salient_image_url(),
              GURL("http://localhost/salient_image"));
    base::Time then =
        base::Time::FromUTCExploded({2015, 11, 4, 25, 13, 46, 45});
    EXPECT_EQ(then, snippet.publish_date());
  }
}

TEST_F(NTPSnippetsServiceTest, Discard) {
  std::string json_str(
      "{ \"recos\": [ { \"contentInfo\": { \"url\" : \"http://site.com\" }}]}");
  ASSERT_TRUE(LoadFromJSONString(json_str));

  ASSERT_EQ(1u, service()->size());

  // Discarding a non-existent snippet shouldn't do anything.
  EXPECT_FALSE(service()->DiscardSnippet(GURL("http://othersite.com")));
  EXPECT_EQ(1u, service()->size());

  // Discard the snippet.
  EXPECT_TRUE(service()->DiscardSnippet(GURL("http://site.com")));
  EXPECT_EQ(0u, service()->size());

  // Make sure that fetching the same snippet again does not re-add it.
  ASSERT_TRUE(LoadFromJSONString(json_str));
  EXPECT_EQ(0u, service()->size());

  // The snippet should stay discarded even after re-creating the service.
  CreateSnippetsService();
  // Init the service, so the prefs get loaded.
  // TODO(treib): This should happen in CreateSnippetsService.
  service()->Init(true);
  ASSERT_TRUE(LoadFromJSONString(json_str));
  EXPECT_EQ(0u, service()->size());
}

TEST_F(NTPSnippetsServiceTest, CreationTimestampParseFail) {
  std::string json_str(GetTestJson("aaa1448459205"));

  ASSERT_TRUE(LoadFromJSONString(json_str));
  EXPECT_EQ(service()->size(), 1u);

  // The same for loop without the '&' should not compile.
  for (auto& snippet : *service()) {
    // Snippet here is a const.
    EXPECT_EQ(snippet.url(), GURL("http://localhost/foobar"));
    EXPECT_EQ(snippet.title(), "Title");
    EXPECT_EQ(snippet.snippet(), "Snippet");
    EXPECT_EQ(base::Time::UnixEpoch(), snippet.publish_date());
  }
}

TEST_F(NTPSnippetsServiceTest, RemoveExpiredContent) {
  std::string json_str(GetTestExpiredJson(1448459205));

  ASSERT_TRUE(LoadFromJSONString(json_str));
  EXPECT_EQ(service()->size(), 0u);
}

}  // namespace ntp_snippets
