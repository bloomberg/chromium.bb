// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_snippets/ntp_snippets_service.h"

#include <memory>
#include <vector>

#include "base/json/json_reader.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/test/histogram_tester.h"
#include "base/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "components/image_fetcher/image_fetcher.h"
#include "components/ntp_snippets/ntp_snippet.h"
#include "components/ntp_snippets/ntp_snippets_fetcher.h"
#include "components/ntp_snippets/ntp_snippets_scheduler.h"
#include "components/prefs/testing_pref_service.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::ElementsAre;
using testing::IsEmpty;
using testing::_;

namespace ntp_snippets {

namespace {

const base::Time::Exploded kDefaultCreationTime = {2015, 11, 4, 25, 13, 46, 45};

base::Time GetDefaultCreationTime() {
  return base::Time::FromUTCExploded(kDefaultCreationTime);
}

std::string GetTestJson(const std::string& content_creation_time_str,
                        const std::string& expiry_time_str) {
  char json_str_format[] =
      "{ \"recos\": [ "
      "{ \"contentInfo\": {"
      "\"url\" : \"http://localhost/foobar\","
      "\"title\" : \"Title\","
      "\"snippet\" : \"Snippet\","
      "\"thumbnailUrl\" : \"http://localhost/salient_image\","
      "\"creationTimestampSec\" : \"%s\","
      "\"expiryTimestampSec\" : \"%s\","
      "\"sourceCorpusInfo\" : [ "
      "{\"ampUrl\" : \"http://localhost/amp\","
      "\"corpusId\" : \"http://localhost/foobar\","
      "\"publisherData\": { \"sourceName\" : \"Foo News\"}}]"
      "}}"
      "]}";

  return base::StringPrintf(json_str_format, content_creation_time_str.c_str(),
                            expiry_time_str.c_str());
}

std::string GetTestJsonWithSources(const std::string& content_creation_time_str,
                                   const std::string& expiry_time_str,
                                   const std::vector<std::string>& source_urls,
                                   const std::vector<std::string>& publishers,
                                   const std::vector<std::string>& amp_urls) {
  char json_str_format[] =
      "{ \"recos\": [ "
      "{ \"contentInfo\": {"
      "\"url\" : \"http://localhost/foobar\","
      "\"title\" : \"Title\","
      "\"snippet\" : \"Snippet\","
      "\"thumbnailUrl\" : \"http://localhost/salient_image\","
      "\"creationTimestampSec\" : \"%s\","
      "\"expiryTimestampSec\" : \"%s\","
      "\"sourceCorpusInfo\" : [%s]"
      "}}"
      "]}";

  char source_corpus_info_format[] =
      "{\"corpusId\": \"%s\","
      "\"publisherData\": {"
      "\"sourceName\": \"%s\""
      "},"
      "\"ampUrl\": \"%s\"}";

  std::string source_corpus_info_list_str;
  for (size_t i = 0; i < source_urls.size(); ++i) {
    std::string source_corpus_info_str =
        base::StringPrintf(source_corpus_info_format,
                           source_urls[i].empty() ? "" : source_urls[i].c_str(),
                           publishers[i].empty() ? "" : publishers[i].c_str(),
                           amp_urls[i].empty() ? "" : amp_urls[i].c_str());
    source_corpus_info_list_str.append(source_corpus_info_str);
    source_corpus_info_list_str.append(",");
  }
  // Remove the last comma
  source_corpus_info_list_str.erase(source_corpus_info_list_str.size()-1, 1);
  return base::StringPrintf(json_str_format, content_creation_time_str.c_str(),
                            expiry_time_str.c_str(),
                            source_corpus_info_list_str.c_str());
}

std::string GetTestJsonWithSources(const std::vector<std::string>& source_urls,
                                   const std::vector<std::string>& publishers,
                                   const std::vector<std::string>& amp_urls) {
  base::Time expiry_time = base::Time::Now() + base::TimeDelta::FromHours(1);
  return GetTestJsonWithSources(
      NTPSnippet::TimeToJsonString(GetDefaultCreationTime()),
      NTPSnippet::TimeToJsonString(expiry_time), source_urls, publishers,
      amp_urls);
}

std::string GetTestJson(const std::string& content_creation_time_str) {
  base::Time expiry_time = base::Time::Now() + base::TimeDelta::FromHours(1);
  return GetTestJson(content_creation_time_str,
                     NTPSnippet::TimeToJsonString(expiry_time));
}

std::string GetTestJson() {
  return GetTestJson(NTPSnippet::TimeToJsonString(GetDefaultCreationTime()));
}

std::string GetTestExpiredJson() {
  return GetTestJson(NTPSnippet::TimeToJsonString(GetDefaultCreationTime()),
                     NTPSnippet::TimeToJsonString(base::Time::Now()));
}

std::string GetInvalidJson() {
  std::string json_str = GetTestJson();
  // Make the json invalid by removing the final closing brace.
  return json_str.substr(0, json_str.size() - 1);
}

std::string GetIncompleteJson() {
  std::string json_str = GetTestJson();
  // Rename the "url" entry. The result is syntactically valid json that will
  // fail to parse as snippets.
  size_t pos = json_str.find("\"url\"");
  if (pos == std::string::npos) {
    NOTREACHED();
    return std::string();
  }
  json_str[pos + 1] = 'x';
  return json_str;
}

void ParseJson(
    bool expect_success,
    const std::string& json,
    const ntp_snippets::NTPSnippetsService::SuccessCallback& success_callback,
    const ntp_snippets::NTPSnippetsService::ErrorCallback& error_callback) {
  base::JSONReader json_reader;
  std::unique_ptr<base::Value> value = json_reader.ReadToValue(json);
  bool success = !!value;
  EXPECT_EQ(expect_success, success);
  if (value) {
    success_callback.Run(std::move(value));
  } else {
    error_callback.Run(json_reader.GetErrorMessage());
  }
}

class MockScheduler : public NTPSnippetsScheduler {
 public:
  MOCK_METHOD4(Schedule,
               bool(base::TimeDelta period_wifi_charging,
                    base::TimeDelta period_wifi,
                    base::TimeDelta period_fallback,
                    base::Time reschedule_time));
  MOCK_METHOD0(Unschedule, bool());
};

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

  virtual void CreateSnippetsService() {
    CreateSnippetsServiceEnabled(true);
  }

  void CreateSnippetsServiceEnabled(bool enabled) {
    scheduler_.reset(new MockScheduler);
    scoped_refptr<base::SingleThreadTaskRunner> task_runner(
        base::ThreadTaskRunnerHandle::Get());
    scoped_refptr<net::TestURLRequestContextGetter> request_context_getter =
        new net::TestURLRequestContextGetter(task_runner.get());

    service_.reset(new NTPSnippetsService(
        pref_service_.get(), nullptr, task_runner, std::string("fr"),
        scheduler_.get(),
        base::WrapUnique(new NTPSnippetsFetcher(
            std::move(request_context_getter), true)),
        base::Bind(&ParseJson, true), nullptr));
    if (enabled)
      EXPECT_CALL(*scheduler_, Schedule(_, _, _, _));
    else
      EXPECT_CALL(*scheduler_, Unschedule());
    service_->Init(enabled);
  }

 protected:
  NTPSnippetsService* service() { return service_.get(); }

  void LoadFromJSONString(const std::string& json) {
    service_->OnSnippetsDownloaded(json, std::string());
  }

  void SetExpectJsonParseSuccess(bool expect_success) {
    service_->parse_json_callback_ = base::Bind(&ParseJson, expect_success);
  }

 private:
  base::MessageLoop message_loop_;
  std::unique_ptr<TestingPrefServiceSimple> pref_service_;
  std::unique_ptr<NTPSnippetsService> service_;
  std::unique_ptr<MockScheduler> scheduler_;

  DISALLOW_COPY_AND_ASSIGN(NTPSnippetsServiceTest);
};

class NTPSnippetsServiceDisabledTest : public NTPSnippetsServiceTest {
 public:
  void CreateSnippetsService() override {
    CreateSnippetsServiceEnabled(false);
  }
};

TEST_F(NTPSnippetsServiceTest, Schedule) {
  // CreateSnippetsServiceEnabled checks that Schedule is called.
}

TEST_F(NTPSnippetsServiceDisabledTest, Unschedule) {
  // CreateSnippetsServiceEnabled checks that Unschedule is called.
}

TEST_F(NTPSnippetsServiceTest, Loop) {
  std::string json_str(
      "{ \"recos\": [ "
      "  { \"contentInfo\": { \"url\" : \"http://localhost/foobar\" }}"
      "]}");
  LoadFromJSONString(json_str);

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
  std::string json_str(GetTestJson());

  LoadFromJSONString(json_str);
  EXPECT_EQ(service()->size(), 1u);

  // The same for loop without the '&' should not compile.
  for (auto& snippet : *service()) {
    // Snippet here is a const.
    EXPECT_EQ(snippet.url(), GURL("http://localhost/foobar"));
    EXPECT_EQ(snippet.best_source().publisher_name, "Foo News");
    EXPECT_EQ(snippet.title(), "Title");
    EXPECT_EQ(snippet.snippet(), "Snippet");
    EXPECT_EQ(snippet.salient_image_url(),
              GURL("http://localhost/salient_image"));
    EXPECT_EQ(GetDefaultCreationTime(), snippet.publish_date());
    EXPECT_EQ(snippet.best_source().amp_url.spec(),
              GURL("http://localhost/amp").spec());
  }
}

TEST_F(NTPSnippetsServiceTest, Clear) {
  std::string json_str(GetTestJson());

  LoadFromJSONString(json_str);
  EXPECT_EQ(service()->size(), 1u);

  service()->ClearSnippets();
  EXPECT_EQ(service()->size(), 0u);
}

TEST_F(NTPSnippetsServiceTest, InsertAtFront) {
  base::Time expiry_time = base::Time::Now() + base::TimeDelta::FromHours(1);
  char json_str_format[] =
      "{ \"recos\": [ "
      "{ \"contentInfo\": {"
      "\"url\" : \"%s\","
      "\"title\" : \"Title\","
      "\"snippet\" : \"Snippet\","
      "\"thumbnailUrl\" : \"http://localhost/salient_image\","
      "\"creationTimestampSec\" : \"%s\","
      "\"expiryTimestampSec\" : \"%s\","
      "\"sourceCorpusInfo\" : [{\"corpusId\": \"http://first\","
      "\"publisherData\": {"
      "\"sourceName\": \"Source 1\""
      "},"
      "\"ampUrl\": \"\"}]"
      "}}"
      "]}";
  std::string json_str(base::StringPrintf(
      json_str_format, "http://first",
      NTPSnippet::TimeToJsonString(GetDefaultCreationTime()).c_str(),
      NTPSnippet::TimeToJsonString(expiry_time).c_str()));

  LoadFromJSONString(json_str);

  ASSERT_EQ(service()->size(), 1u);

  json_str = base::StringPrintf(
      json_str_format, "http://second",
      NTPSnippet::TimeToJsonString(GetDefaultCreationTime()).c_str(),
      NTPSnippet::TimeToJsonString(expiry_time).c_str());

  LoadFromJSONString(json_str);
  ASSERT_EQ(service()->size(), 2u);

  // The snippet loaded last should be at the first position in the list now.
  const NTPSnippet& first_snippet = *service()->begin();
  EXPECT_EQ(first_snippet.url(), GURL("http://second"));
}

TEST_F(NTPSnippetsServiceTest, LimitNumSnippets) {
  int max_snippet_count = NTPSnippetsService::GetMaxSnippetCountForTesting();
  int snippets_per_load = max_snippet_count / 2 + 1;

  base::Time expiry_time = base::Time::Now() + base::TimeDelta::FromHours(1);
  char json_str_format[] =
      "{ \"contentInfo\": {"
      "\"url\" : \"http://localhost/%i\","
      "\"title\" : \"Title\","
      "\"snippet\" : \"Snippet\","
      "\"thumbnailUrl\" : \"http://localhost/salient_image\","
      "\"creationTimestampSec\" : \"%s\","
      "\"expiryTimestampSec\" : \"%s\","
      "\"sourceCorpusInfo\" : [{\"corpusId\": \"http://first\","
      "\"publisherData\": {"
      "\"sourceName\": \"Source 1\""
      "},"
      "\"ampUrl\": \"\"}]"
      "}}";

  std::vector<std::string> snippets1;
  std::vector<std::string> snippets2;
  for (int i = 0; i < snippets_per_load; i++) {
    snippets1.push_back(base::StringPrintf(
        json_str_format, i,
        NTPSnippet::TimeToJsonString(GetDefaultCreationTime()).c_str(),
        NTPSnippet::TimeToJsonString(expiry_time).c_str()));
    snippets2.push_back(base::StringPrintf(
        json_str_format, snippets_per_load + i,
        NTPSnippet::TimeToJsonString(GetDefaultCreationTime()).c_str(),
        NTPSnippet::TimeToJsonString(expiry_time).c_str()));
  }

  LoadFromJSONString(
      "{ \"recos\": [ " + base::JoinString(snippets1, ", ") + "]}");
  ASSERT_EQ(snippets1.size(), service()->size());

  LoadFromJSONString(
      "{ \"recos\": [ " + base::JoinString(snippets2, ", ") + "]}");
  EXPECT_EQ(max_snippet_count, (int)service()->size());
}

TEST_F(NTPSnippetsServiceTest, LoadInvalidJson) {
  SetExpectJsonParseSuccess(false);
  LoadFromJSONString(GetInvalidJson());
  EXPECT_EQ(service()->size(), 0u);
}

TEST_F(NTPSnippetsServiceTest, LoadInvalidJsonWithExistingSnippets) {
  LoadFromJSONString(GetTestJson());
  ASSERT_EQ(service()->size(), 1u);

  SetExpectJsonParseSuccess(false);
  LoadFromJSONString(GetInvalidJson());
  // This should not have changed the existing snippets.
  EXPECT_EQ(service()->size(), 1u);
}

TEST_F(NTPSnippetsServiceTest, LoadIncompleteJson) {
  LoadFromJSONString(GetIncompleteJson());
  EXPECT_EQ(service()->size(), 0u);
}

TEST_F(NTPSnippetsServiceTest, LoadIncompleteJsonWithExistingSnippets) {
  LoadFromJSONString(GetTestJson());
  ASSERT_EQ(service()->size(), 1u);

  LoadFromJSONString(GetIncompleteJson());
  // This should not have changed the existing snippets.
  EXPECT_EQ(service()->size(), 1u);
}

TEST_F(NTPSnippetsServiceTest, Discard) {
  std::vector<std::string> source_urls, publishers, amp_urls;
  source_urls.push_back(std::string("http://site.com"));
  publishers.push_back(std::string("Source 1"));
  amp_urls.push_back(std::string());
  std::string json_str(
      GetTestJsonWithSources(source_urls, publishers, amp_urls));

  LoadFromJSONString(json_str);

  ASSERT_EQ(1u, service()->size());

  // Discarding a non-existent snippet shouldn't do anything.
  EXPECT_FALSE(service()->DiscardSnippet(GURL("http://othersite.com")));
  EXPECT_EQ(1u, service()->size());

  // Discard the snippet.
  EXPECT_TRUE(service()->DiscardSnippet(GURL("http://localhost/foobar")));
  EXPECT_EQ(0u, service()->size());

  // Make sure that fetching the same snippet again does not re-add it.
  LoadFromJSONString(json_str);
  EXPECT_EQ(0u, service()->size());

  // The snippet should stay discarded even after re-creating the service.
  CreateSnippetsService();
  LoadFromJSONString(json_str);
  EXPECT_EQ(0u, service()->size());

  // The snippet can be added again after clearing discarded snippets.
  service()->ClearDiscardedSnippets();
  EXPECT_EQ(0u, service()->size());
  LoadFromJSONString(json_str);
  EXPECT_EQ(1u, service()->size());
}

TEST_F(NTPSnippetsServiceTest, GetDiscarded) {
  LoadFromJSONString(GetTestJson());

  // For the test, we need the snippet to get discarded.
  ASSERT_TRUE(service()->DiscardSnippet(GURL("http://localhost/foobar")));
  const NTPSnippetsService::NTPSnippetStorage& snippets =
      service()->discarded_snippets();
  EXPECT_EQ(1u, snippets.size());
  for (auto& snippet : snippets) {
    EXPECT_EQ(GURL("http://localhost/foobar"), snippet->url());
  }

  // There should be no discarded snippet after clearing the list.
  service()->ClearDiscardedSnippets();
  EXPECT_EQ(0u, service()->discarded_snippets().size());
}

TEST_F(NTPSnippetsServiceTest, CreationTimestampParseFail) {
  std::string json_str(GetTestJson("aaa1448459205"));

  LoadFromJSONString(json_str);
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
  std::string json_str(GetTestExpiredJson());

  LoadFromJSONString(json_str);
  EXPECT_EQ(service()->size(), 0u);
}

TEST_F(NTPSnippetsServiceTest, TestSingleSource) {
  std::vector<std::string> source_urls, publishers, amp_urls;
  source_urls.push_back(std::string("http://source1.com"));
  publishers.push_back(std::string("Source 1"));
  amp_urls.push_back(std::string("http://source1.amp.com"));
  std::string json_str(
      GetTestJsonWithSources(source_urls, publishers, amp_urls));

  LoadFromJSONString(json_str);

  EXPECT_EQ(service()->size(), 1u);

  for (auto& snippet : *service()) {
    EXPECT_EQ(snippet.sources().size(), 1u);
    EXPECT_EQ(snippet.url(), GURL("http://localhost/foobar"));
    EXPECT_EQ(snippet.best_source().url, GURL("http://source1.com"));
    EXPECT_EQ(snippet.best_source().publisher_name, std::string("Source 1"));
    EXPECT_EQ(snippet.best_source().amp_url, GURL("http://source1.amp.com"));
  }
}

TEST_F(NTPSnippetsServiceTest, TestSingleSourceWithMalformedUrl) {
  std::vector<std::string> source_urls, publishers, amp_urls;
  source_urls.push_back(std::string("aaaa"));
  publishers.push_back(std::string("Source 1"));
  amp_urls.push_back(std::string("http://source1.amp.com"));
  std::string json_str(
      GetTestJsonWithSources(source_urls, publishers, amp_urls));

  LoadFromJSONString(json_str);
  EXPECT_EQ(service()->size(), 0u);
}

TEST_F(NTPSnippetsServiceTest, TestSingleSourceWithMissingData) {
  std::vector<std::string> source_urls, publishers, amp_urls;
  source_urls.push_back(std::string("http://source1.com"));
  publishers.push_back(std::string());
  amp_urls.push_back(std::string());
  std::string json_str(
      GetTestJsonWithSources(source_urls, publishers, amp_urls));

  LoadFromJSONString(json_str);
  EXPECT_EQ(service()->size(), 0u);
}

TEST_F(NTPSnippetsServiceTest, TestMultipleSources) {
  std::vector<std::string> source_urls, publishers, amp_urls;
  source_urls.push_back(std::string("http://source1.com"));
  source_urls.push_back(std::string("http://source2.com"));
  publishers.push_back(std::string("Source 1"));
  publishers.push_back(std::string("Source 2"));
  amp_urls.push_back(std::string("http://source1.amp.com"));
  amp_urls.push_back(std::string("http://source2.amp.com"));
  std::string json_str(
      GetTestJsonWithSources(source_urls, publishers, amp_urls));

  LoadFromJSONString(json_str);
  EXPECT_EQ(service()->size(), 1u);

  // Expect the first source to be chosen
  for (auto& snippet : *service()) {
    EXPECT_EQ(snippet.sources().size(), 2u);
    EXPECT_EQ(snippet.url(), GURL("http://localhost/foobar"));
    EXPECT_EQ(snippet.best_source().url, GURL("http://source1.com"));
    EXPECT_EQ(snippet.best_source().publisher_name, std::string("Source 1"));
    EXPECT_EQ(snippet.best_source().amp_url, GURL("http://source1.amp.com"));
  }
}

TEST_F(NTPSnippetsServiceTest, TestMultipleIncompleteSources) {
  // Set Source 2 to have no AMP url, and Source 1 to have no publisher name
  // Source 2 should win since we favor publisher name over amp url
  std::vector<std::string> source_urls, publishers, amp_urls;
  source_urls.push_back(std::string("http://source1.com"));
  source_urls.push_back(std::string("http://source2.com"));
  publishers.push_back(std::string());
  publishers.push_back(std::string("Source 2"));
  amp_urls.push_back(std::string("http://source1.amp.com"));
  amp_urls.push_back(std::string());
  std::string json_str(
      GetTestJsonWithSources(source_urls, publishers, amp_urls));

  LoadFromJSONString(json_str);
  EXPECT_EQ(service()->size(), 1u);

  for (auto& snippet : *service()) {
    EXPECT_EQ(snippet.sources().size(), 2u);
    EXPECT_EQ(snippet.url(), GURL("http://localhost/foobar"));
    EXPECT_EQ(snippet.best_source().url, GURL("http://source2.com"));
    EXPECT_EQ(snippet.best_source().publisher_name, std::string("Source 2"));
    EXPECT_EQ(snippet.best_source().amp_url, GURL());
  }

  service()->ClearSnippets();
  // Set Source 1 to have no AMP url, and Source 2 to have no publisher name
  // Source 1 should win in this case since we prefer publisher name to AMP url
  source_urls.clear();
  source_urls.push_back(std::string("http://source1.com"));
  source_urls.push_back(std::string("http://source2.com"));
  publishers.clear();
  publishers.push_back(std::string("Source 1"));
  publishers.push_back(std::string());
  amp_urls.clear();
  amp_urls.push_back(std::string());
  amp_urls.push_back(std::string("http://source2.amp.com"));
  json_str = GetTestJsonWithSources(source_urls, publishers, amp_urls);

  LoadFromJSONString(json_str);
  EXPECT_EQ(service()->size(), 1u);

  for (auto& snippet : *service()) {
    EXPECT_EQ(snippet.sources().size(), 2u);
    EXPECT_EQ(snippet.url(), GURL("http://localhost/foobar"));
    EXPECT_EQ(snippet.best_source().url, GURL("http://source1.com"));
    EXPECT_EQ(snippet.best_source().publisher_name, std::string("Source 1"));
    EXPECT_EQ(snippet.best_source().amp_url, GURL());
  }

  service()->ClearSnippets();
  // Set source 1 to have no AMP url and no source, and source 2 to only have
  // amp url. There should be no snippets since we only add sources we consider
  // complete
  source_urls.clear();
  source_urls.push_back(std::string("http://source1.com"));
  source_urls.push_back(std::string("http://source2.com"));
  publishers.clear();
  publishers.push_back(std::string());
  publishers.push_back(std::string());
  amp_urls.clear();
  amp_urls.push_back(std::string());
  amp_urls.push_back(std::string("http://source2.amp.com"));
  json_str = GetTestJsonWithSources(source_urls, publishers, amp_urls);

  LoadFromJSONString(json_str);
  EXPECT_EQ(service()->size(), 0u);
}

TEST_F(NTPSnippetsServiceTest, TestMultipleCompleteSources) {
  // Test 2 complete sources, we should choose the first complete source
  std::vector<std::string> source_urls, publishers, amp_urls;
  source_urls.push_back(std::string("http://source1.com"));
  source_urls.push_back(std::string("http://source2.com"));
  source_urls.push_back(std::string("http://source3.com"));
  publishers.push_back(std::string("Source 1"));
  publishers.push_back(std::string());
  publishers.push_back(std::string("Source 3"));
  amp_urls.push_back(std::string("http://source1.amp.com"));
  amp_urls.push_back(std::string("http://source2.amp.com"));
  amp_urls.push_back(std::string("http://source3.amp.com"));
  std::string json_str(
      GetTestJsonWithSources(source_urls, publishers, amp_urls));

  LoadFromJSONString(json_str);
  EXPECT_EQ(service()->size(), 1u);

  for (auto& snippet : *service()) {
    EXPECT_EQ(snippet.sources().size(), 3u);
    EXPECT_EQ(snippet.url(), GURL("http://localhost/foobar"));
    EXPECT_EQ(snippet.best_source().url, GURL("http://source1.com"));
    EXPECT_EQ(snippet.best_source().publisher_name, std::string("Source 1"));
    EXPECT_EQ(snippet.best_source().amp_url, GURL("http://source1.amp.com"));
  }

  // Test 2 complete sources, we should choose the first complete source
  service()->ClearSnippets();
  source_urls.clear();
  source_urls.push_back(std::string("http://source1.com"));
  source_urls.push_back(std::string("http://source2.com"));
  source_urls.push_back(std::string("http://source3.com"));
  publishers.clear();
  publishers.push_back(std::string());
  publishers.push_back(std::string("Source 2"));
  publishers.push_back(std::string("Source 3"));
  amp_urls.clear();
  amp_urls.push_back(std::string("http://source1.amp.com"));
  amp_urls.push_back(std::string("http://source2.amp.com"));
  amp_urls.push_back(std::string("http://source3.amp.com"));
  json_str = GetTestJsonWithSources(source_urls, publishers, amp_urls);

  LoadFromJSONString(json_str);
  EXPECT_EQ(service()->size(), 1u);

  for (auto& snippet : *service()) {
    EXPECT_EQ(snippet.sources().size(), 3u);
    EXPECT_EQ(snippet.url(), GURL("http://localhost/foobar"));
    EXPECT_EQ(snippet.best_source().url, GURL("http://source2.com"));
    EXPECT_EQ(snippet.best_source().publisher_name, std::string("Source 2"));
    EXPECT_EQ(snippet.best_source().amp_url, GURL("http://source2.amp.com"));
  }

  // Test 3 complete sources, we should choose the first complete source
  service()->ClearSnippets();
  source_urls.clear();
  source_urls.push_back(std::string("http://source1.com"));
  source_urls.push_back(std::string("http://source2.com"));
  source_urls.push_back(std::string("http://source3.com"));
  publishers.clear();
  publishers.push_back(std::string("Source 1"));
  publishers.push_back(std::string("Source 2"));
  publishers.push_back(std::string("Source 3"));
  amp_urls.clear();
  amp_urls.push_back(std::string());
  amp_urls.push_back(std::string("http://source2.amp.com"));
  amp_urls.push_back(std::string("http://source3.amp.com"));
  json_str = GetTestJsonWithSources(source_urls, publishers, amp_urls);

  LoadFromJSONString(json_str);
  EXPECT_EQ(service()->size(), 1u);

  for (auto& snippet : *service()) {
    EXPECT_EQ(snippet.sources().size(), 3u);
    EXPECT_EQ(snippet.url(), GURL("http://localhost/foobar"));
    EXPECT_EQ(snippet.best_source().url, GURL("http://source2.com"));
    EXPECT_EQ(snippet.best_source().publisher_name, std::string("Source 2"));
    EXPECT_EQ(snippet.best_source().amp_url, GURL("http://source2.amp.com"));
  }
}

TEST_F(NTPSnippetsServiceTest, LogNumArticlesHistogram) {
  base::HistogramTester tester;
  SetExpectJsonParseSuccess(false);
  LoadFromJSONString(GetInvalidJson());
  EXPECT_THAT(tester.GetAllSamples("NewTabPage.Snippets.NumArticles"),
              ElementsAre(base::Bucket(/*min=*/0, /*count=*/1)));
  // Invalid JSON shouldn't contribute to NumArticlesFetched.
  EXPECT_THAT(tester.GetAllSamples("NewTabPage.Snippets.NumArticlesFetched"),
              IsEmpty());
  SetExpectJsonParseSuccess(true);
  // Valid JSON with empty list.
  LoadFromJSONString("{ \"recos\": []}");
  EXPECT_THAT(tester.GetAllSamples("NewTabPage.Snippets.NumArticles"),
              ElementsAre(base::Bucket(/*min=*/0, /*count=*/2)));
  EXPECT_THAT(tester.GetAllSamples("NewTabPage.Snippets.NumArticlesFetched"),
              ElementsAre(base::Bucket(/*min=*/0, /*count=*/1)));
  // Snippet list should be populated with size 1.
  LoadFromJSONString(GetTestJson());
  EXPECT_THAT(tester.GetAllSamples("NewTabPage.Snippets.NumArticles"),
              ElementsAre(base::Bucket(/*min=*/0, /*count=*/2),
                          base::Bucket(/*min=*/1, /*count=*/1)));
  EXPECT_THAT(tester.GetAllSamples("NewTabPage.Snippets.NumArticlesFetched"),
              ElementsAre(base::Bucket(/*min=*/0, /*count=*/1),
                          base::Bucket(/*min=*/1, /*count=*/1)));
  // Duplicate snippet shouldn't increase the list size.
  LoadFromJSONString(GetTestJson());
  EXPECT_THAT(tester.GetAllSamples("NewTabPage.Snippets.NumArticles"),
              ElementsAre(base::Bucket(/*min=*/0, /*count=*/2),
                          base::Bucket(/*min=*/1, /*count=*/2)));
  EXPECT_THAT(tester.GetAllSamples("NewTabPage.Snippets.NumArticlesFetched"),
              ElementsAre(base::Bucket(/*min=*/0, /*count=*/1),
                          base::Bucket(/*min=*/1, /*count=*/2)));
  EXPECT_THAT(
      tester.GetAllSamples("NewTabPage.Snippets.NumArticlesZeroDueToDiscarded"),
      IsEmpty());
  // Discarding a snippet should decrease the list size. This will only be
  // logged after the next fetch.
  EXPECT_TRUE(service()->DiscardSnippet(GURL("http://localhost/foobar")));
  LoadFromJSONString(GetTestJson());
  EXPECT_THAT(tester.GetAllSamples("NewTabPage.Snippets.NumArticles"),
              ElementsAre(base::Bucket(/*min=*/0, /*count=*/3),
                          base::Bucket(/*min=*/1, /*count=*/2)));
  // Discarded snippets shouldn't influence NumArticlesFetched.
  EXPECT_THAT(tester.GetAllSamples("NewTabPage.Snippets.NumArticlesFetched"),
              ElementsAre(base::Bucket(/*min=*/0, /*count=*/1),
                          base::Bucket(/*min=*/1, /*count=*/3)));
  EXPECT_THAT(
      tester.GetAllSamples("NewTabPage.Snippets.NumArticlesZeroDueToDiscarded"),
      ElementsAre(base::Bucket(/*min=*/1, /*count=*/1)));
  // Recreating the service and loading from prefs shouldn't count as fetched
  // articles.
  CreateSnippetsService();
  tester.ExpectTotalCount("NewTabPage.Snippets.NumArticlesFetched", 4);
}
}  // namespace ntp_snippets
