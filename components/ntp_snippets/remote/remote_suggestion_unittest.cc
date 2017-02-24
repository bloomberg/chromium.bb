// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_snippets/remote/remote_suggestion.h"

#include <utility>

#include "base/json/json_reader.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/ntp_snippets/remote/proto/ntp_snippets.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ntp_snippets {
namespace {

using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::IsNull;
using ::testing::NotNull;

std::unique_ptr<RemoteSuggestion> SnippetFromContentSuggestionJSON(
    const std::string& json,
    const base::Time& fetch_date) {
  auto json_value = base::JSONReader::Read(json);
  base::DictionaryValue* json_dict;
  if (!json_value->GetAsDictionary(&json_dict)) {
    return nullptr;
  }
  return RemoteSuggestion::CreateFromContentSuggestionsDictionary(
      *json_dict, kArticlesRemoteId, fetch_date);
}

TEST(RemoteSuggestionTest, FromChromeContentSuggestionsDictionary) {
  const std::string kJsonStr =
      "{"
      "  \"ids\" : [\"http://localhost/foobar\"],"
      "  \"title\" : \"Foo Barred from Baz\","
      "  \"snippet\" : \"...\","
      "  \"fullPageUrl\" : \"http://localhost/foobar\","
      "  \"creationTime\" : \"2016-06-30T11:01:37.000Z\","
      "  \"expirationTime\" : \"2016-07-01T11:01:37.000Z\","
      "  \"attribution\" : \"Foo News\","
      "  \"imageUrl\" : \"http://localhost/foobar.jpg\","
      "  \"ampUrl\" : \"http://localhost/amp\","
      "  \"faviconUrl\" : \"http://localhost/favicon.ico\", "
      "  \"score\": 9001,\n"
      "  \"notificationInfo\": {\n"
      "    \"shouldNotify\": true,"
      "    \"deadline\": \"2016-06-30T13:01:37.000Z\"\n"
      "  }\n"
      "}";
  const base::Time fetch_date = base::Time::FromInternalValue(1466634774L);
  auto snippet = SnippetFromContentSuggestionJSON(kJsonStr, fetch_date);
  ASSERT_THAT(snippet, NotNull());

  EXPECT_EQ(snippet->id(), "http://localhost/foobar");
  EXPECT_EQ(snippet->title(), "Foo Barred from Baz");
  EXPECT_EQ(snippet->snippet(), "...");
  EXPECT_EQ(snippet->salient_image_url(), GURL("http://localhost/foobar.jpg"));
  EXPECT_EQ(snippet->score(), 9001);
  auto unix_publish_date = snippet->publish_date() - base::Time::UnixEpoch();
  auto expiry_duration = snippet->expiry_date() - snippet->publish_date();
  EXPECT_FLOAT_EQ(unix_publish_date.InSecondsF(), 1467284497.000000f);
  EXPECT_FLOAT_EQ(expiry_duration.InSecondsF(), 86400.000000f);

  EXPECT_EQ(snippet->publisher_name(), "Foo News");
  EXPECT_EQ(snippet->url(), GURL("http://localhost/foobar"));
  EXPECT_EQ(snippet->amp_url(), GURL("http://localhost/amp"));

  EXPECT_TRUE(snippet->should_notify());
  auto notification_duration =
      snippet->notification_deadline() - snippet->publish_date();
  EXPECT_EQ(7200.0f, notification_duration.InSecondsF());
  EXPECT_EQ(fetch_date, snippet->fetch_date());
}

std::unique_ptr<RemoteSuggestion> SnippetFromChromeReaderDict(
    std::unique_ptr<base::DictionaryValue> dict,
    const base::Time& fetch_date) {
  if (!dict) {
    return nullptr;
  }
  return RemoteSuggestion::CreateFromChromeReaderDictionary(*dict, fetch_date);
}

const char kChromeReaderCreationTimestamp[] = "1234567890";
const char kChromeReaderExpiryTimestamp[] = "2345678901";

// Old form, from chromereader-pa.googleapis.com. Two sources.
std::unique_ptr<base::DictionaryValue> ChromeReaderSnippetWithTwoSources() {
  const std::string kJsonStr = base::StringPrintf(
      "{\n"
      "  \"contentInfo\": {\n"
      "    \"url\":                   \"http://url.com\",\n"
      "    \"title\":                 \"Source 1 Title\",\n"
      "    \"snippet\":               \"Source 1 Snippet\",\n"
      "    \"thumbnailUrl\":          \"http://url.com/thumbnail\",\n"
      "    \"creationTimestampSec\":  \"%s\",\n"
      "    \"expiryTimestampSec\":    \"%s\",\n"
      "    \"sourceCorpusInfo\": [{\n"
      "      \"corpusId\":            \"http://source1.com\",\n"
      "      \"publisherData\": {\n"
      "        \"sourceName\":        \"Source 1\"\n"
      "      },\n"
      "      \"ampUrl\": \"http://source1.amp.com\"\n"
      "    }, {\n"
      "      \"corpusId\":            \"http://source2.com\",\n"
      "      \"publisherData\": {\n"
      "        \"sourceName\":        \"Source 2\"\n"
      "      },\n"
      "      \"ampUrl\": \"http://source2.amp.com\"\n"
      "    }]\n"
      "  },\n"
      "  \"score\": 5.0\n"
      "}\n",
      kChromeReaderCreationTimestamp, kChromeReaderExpiryTimestamp);

  auto json_value = base::JSONReader::Read(kJsonStr);
  base::DictionaryValue* json_dict;
  if (!json_value->GetAsDictionary(&json_dict)) {
    return nullptr;
  }
  return json_dict->CreateDeepCopy();
}

TEST(RemoteSuggestionTest, TestMultipleSources) {
  auto snippet = SnippetFromChromeReaderDict(
      ChromeReaderSnippetWithTwoSources(), base::Time());
  ASSERT_THAT(snippet, NotNull());

  // Expect the first source to be chosen.
  EXPECT_EQ(snippet->id(), "http://url.com");
  EXPECT_EQ(snippet->url(), GURL("http://source1.com"));
  EXPECT_EQ(snippet->publisher_name(), std::string("Source 1"));
  EXPECT_EQ(snippet->amp_url(), GURL("http://source1.amp.com"));
}

TEST(RemoteSuggestionTest, TestMultipleIncompleteSources1) {
  // Set Source 2 to have no AMP url, and Source 1 to have no publisher name
  // Source 2 should win since we favor publisher name over amp url
  auto dict = ChromeReaderSnippetWithTwoSources();
  base::ListValue* sources;
  ASSERT_TRUE(dict->GetList("contentInfo.sourceCorpusInfo", &sources));
  base::DictionaryValue* source;
  ASSERT_TRUE(sources->GetDictionary(0, &source));
  source->Remove("publisherData.sourceName", nullptr);
  ASSERT_TRUE(sources->GetDictionary(1, &source));
  source->Remove("ampUrl", nullptr);

  auto snippet = SnippetFromChromeReaderDict(std::move(dict), base::Time());
  ASSERT_THAT(snippet, NotNull());

  EXPECT_EQ(snippet->id(), "http://url.com");
  EXPECT_EQ(snippet->url(), GURL("http://source2.com"));
  EXPECT_EQ(snippet->publisher_name(), std::string("Source 2"));
  EXPECT_EQ(snippet->amp_url(), GURL());
}

TEST(RemoteSuggestionTest, TestMultipleIncompleteSources2) {
  // Set Source 1 to have no AMP url, and Source 2 to have no publisher name
  // Source 1 should win in this case since we prefer publisher name to AMP url
  auto dict = ChromeReaderSnippetWithTwoSources();
  base::ListValue* sources;
  ASSERT_TRUE(dict->GetList("contentInfo.sourceCorpusInfo", &sources));
  base::DictionaryValue* source;
  ASSERT_TRUE(sources->GetDictionary(0, &source));
  source->Remove("ampUrl", nullptr);
  ASSERT_TRUE(sources->GetDictionary(1, &source));
  source->Remove("publisherData.sourceName", nullptr);

  auto snippet = SnippetFromChromeReaderDict(std::move(dict), base::Time());
  ASSERT_THAT(snippet, NotNull());

  EXPECT_EQ(snippet->id(), "http://url.com");
  EXPECT_EQ(snippet->url(), GURL("http://source1.com"));
  EXPECT_EQ(snippet->publisher_name(), std::string("Source 1"));
  EXPECT_EQ(snippet->amp_url(), GURL());
}

TEST(RemoteSuggestionTest, TestMultipleIncompleteSources3) {
  // Set source 1 to have no AMP url and no source, and source 2 to only have
  // amp url. There should be no snippets since we only add sources we consider
  // complete
  auto dict = ChromeReaderSnippetWithTwoSources();
  base::ListValue* sources;
  ASSERT_TRUE(dict->GetList("contentInfo.sourceCorpusInfo", &sources));
  base::DictionaryValue* source;
  ASSERT_TRUE(sources->GetDictionary(0, &source));
  source->Remove("publisherData.sourceName", nullptr);
  source->Remove("ampUrl", nullptr);
  ASSERT_TRUE(sources->GetDictionary(1, &source));
  source->Remove("publisherData.sourceName", nullptr);

  auto snippet = SnippetFromChromeReaderDict(std::move(dict), base::Time());
  ASSERT_THAT(snippet, NotNull());
  ASSERT_FALSE(snippet->is_complete());
}

TEST(RemoteSuggestionTest, ShouldFillInCreation) {
  auto dict = ChromeReaderSnippetWithTwoSources();
  ASSERT_TRUE(dict->Remove("contentInfo.creationTimestampSec", nullptr));
  auto snippet = SnippetFromChromeReaderDict(std::move(dict), base::Time());
  ASSERT_THAT(snippet, NotNull());

  // Publish date should have been filled with "now" - just make sure it's not
  // empty and not the test default value.
  base::Time publish_date = snippet->publish_date();
  EXPECT_FALSE(publish_date.is_null());
  EXPECT_NE(publish_date, RemoteSuggestion::TimeFromJsonString(
                              kChromeReaderCreationTimestamp));
  // Expiry date should have kept the test default value.
  base::Time expiry_date = snippet->expiry_date();
  EXPECT_FALSE(expiry_date.is_null());
  EXPECT_EQ(expiry_date,
            RemoteSuggestion::TimeFromJsonString(kChromeReaderExpiryTimestamp));
}

TEST(RemoteSuggestionTest, ShouldFillInExpiry) {
  auto dict = ChromeReaderSnippetWithTwoSources();
  ASSERT_TRUE(dict->Remove("contentInfo.expiryTimestampSec", nullptr));
  auto snippet = SnippetFromChromeReaderDict(std::move(dict), base::Time());
  ASSERT_THAT(snippet, NotNull());

  base::Time publish_date = snippet->publish_date();
  ASSERT_FALSE(publish_date.is_null());
  // Expiry date should have been filled with creation date + offset.
  base::Time expiry_date = snippet->expiry_date();
  EXPECT_FALSE(expiry_date.is_null());
  EXPECT_EQ(publish_date + base::TimeDelta::FromMinutes(
                               kChromeReaderDefaultExpiryTimeMins),
            expiry_date);
}

TEST(RemoteSuggestionTest, ShouldFillInCreationAndExpiry) {
  auto dict = ChromeReaderSnippetWithTwoSources();
  ASSERT_TRUE(dict->Remove("contentInfo.creationTimestampSec", nullptr));
  ASSERT_TRUE(dict->Remove("contentInfo.expiryTimestampSec", nullptr));
  auto snippet = SnippetFromChromeReaderDict(std::move(dict), base::Time());
  ASSERT_THAT(snippet, NotNull());

  // Publish date should have been filled with "now" - just make sure it's not
  // empty and not the test default value.
  base::Time publish_date = snippet->publish_date();
  EXPECT_FALSE(publish_date.is_null());
  EXPECT_NE(publish_date, RemoteSuggestion::TimeFromJsonString(
                              kChromeReaderCreationTimestamp));
  // Expiry date should have been filled with creation date + offset.
  base::Time expiry_date = snippet->expiry_date();
  EXPECT_FALSE(expiry_date.is_null());
  EXPECT_EQ(publish_date + base::TimeDelta::FromMinutes(
                               kChromeReaderDefaultExpiryTimeMins),
            expiry_date);
}

TEST(RemoteSuggestionTest, ShouldNotOverwriteExpiry) {
  auto dict = ChromeReaderSnippetWithTwoSources();
  ASSERT_TRUE(dict->Remove("contentInfo.creationTimestampSec", nullptr));
  auto snippet = SnippetFromChromeReaderDict(std::move(dict), base::Time());
  ASSERT_THAT(snippet, NotNull());

  // Expiry date should have kept the test default value.
  base::Time expiry_date = snippet->expiry_date();
  EXPECT_FALSE(expiry_date.is_null());
  EXPECT_EQ(expiry_date,
            RemoteSuggestion::TimeFromJsonString(kChromeReaderExpiryTimestamp));
}

// Old form, from chromereader-pa.googleapis.com. Three sources.
std::unique_ptr<base::DictionaryValue> ChromeReaderSnippetWithThreeSources() {
  const std::string kJsonStr = base::StringPrintf(
      "{\n"
      "  \"contentInfo\": {\n"
      "    \"url\":                   \"http://url.com\",\n"
      "    \"title\":                 \"Source 1 Title\",\n"
      "    \"snippet\":               \"Source 1 Snippet\",\n"
      "    \"thumbnailUrl\":          \"http://url.com/thumbnail\",\n"
      "    \"creationTimestampSec\":  \"%s\",\n"
      "    \"expiryTimestampSec\":    \"%s\",\n"
      "    \"sourceCorpusInfo\": [{\n"
      "      \"corpusId\":            \"http://source1.com\",\n"
      "      \"publisherData\": {\n"
      "        \"sourceName\":        \"Source 1\"\n"
      "      },\n"
      "      \"ampUrl\": \"http://source1.amp.com\"\n"
      "    }, {\n"
      "      \"corpusId\":            \"http://source2.com\",\n"
      "      \"publisherData\": {\n"
      "        \"sourceName\":        \"Source 2\"\n"
      "      },\n"
      "      \"ampUrl\": \"http://source2.amp.com\"\n"
      "    }, {\n"
      "      \"corpusId\":            \"http://source3.com\",\n"
      "      \"publisherData\": {\n"
      "        \"sourceName\":        \"Source 3\"\n"
      "      },\n"
      "      \"ampUrl\": \"http://source3.amp.com\"\n"
      "    }]\n"
      "  },\n"
      "  \"score\": 5.0\n"
      "}\n",
      kChromeReaderCreationTimestamp, kChromeReaderExpiryTimestamp);

  auto json_value = base::JSONReader::Read(kJsonStr);
  base::DictionaryValue* json_dict;
  if (!json_value->GetAsDictionary(&json_dict)) {
    return nullptr;
  }
  return json_dict->CreateDeepCopy();
}

TEST(RemoteSuggestionTest, TestMultipleCompleteSources1) {
  // Test 2 complete sources, we should choose the first complete source
  auto dict = ChromeReaderSnippetWithThreeSources();
  base::ListValue* sources;
  ASSERT_TRUE(dict->GetList("contentInfo.sourceCorpusInfo", &sources));
  base::DictionaryValue* source;
  ASSERT_TRUE(sources->GetDictionary(1, &source));
  source->Remove("publisherData.sourceName", nullptr);

  auto snippet = SnippetFromChromeReaderDict(std::move(dict), base::Time());
  ASSERT_THAT(snippet, NotNull());

  EXPECT_EQ(snippet->id(), "http://url.com");
  EXPECT_THAT(snippet->GetAllIDs(),
              ElementsAre("http://url.com", "http://source1.com",
                          "http://source2.com", "http://source3.com"));
  EXPECT_EQ(snippet->url(), GURL("http://source1.com"));
  EXPECT_EQ(snippet->publisher_name(), std::string("Source 1"));
  EXPECT_EQ(snippet->amp_url(), GURL("http://source1.amp.com"));
}

TEST(RemoteSuggestionTest, TestMultipleCompleteSources2) {
  // Test 2 complete sources, we should choose the first complete source
  auto dict = ChromeReaderSnippetWithThreeSources();
  base::ListValue* sources;
  ASSERT_TRUE(dict->GetList("contentInfo.sourceCorpusInfo", &sources));
  base::DictionaryValue* source;
  ASSERT_TRUE(sources->GetDictionary(0, &source));
  source->Remove("publisherData.sourceName", nullptr);

  auto snippet = SnippetFromChromeReaderDict(std::move(dict), base::Time());
  ASSERT_THAT(snippet, NotNull());

  EXPECT_EQ(snippet->id(), "http://url.com");
  EXPECT_EQ(snippet->url(), GURL("http://source2.com"));
  EXPECT_EQ(snippet->publisher_name(), std::string("Source 2"));
  EXPECT_EQ(snippet->amp_url(), GURL("http://source2.amp.com"));
}

TEST(RemoteSuggestionTest, TestMultipleCompleteSources3) {
  // Test 3 complete sources, we should choose the first complete source
  auto dict = ChromeReaderSnippetWithThreeSources();
  auto snippet = SnippetFromChromeReaderDict(std::move(dict), base::Time());
  ASSERT_THAT(snippet, NotNull());

  EXPECT_EQ(snippet->id(), "http://url.com");
  EXPECT_EQ(snippet->url(), GURL("http://source1.com"));
  EXPECT_EQ(snippet->publisher_name(), std::string("Source 1"));
  EXPECT_EQ(snippet->amp_url(), GURL("http://source1.amp.com"));
}

TEST(RemoteSuggestionTest,
     ShouldSupportMultipleIdsFromContentSuggestionsServer) {
  const std::string kJsonStr =
      "{"
      "  \"ids\" : [\"http://localhost/foobar\", \"012345\"],"
      "  \"title\" : \"Foo Barred from Baz\","
      "  \"snippet\" : \"...\","
      "  \"fullPageUrl\" : \"http://localhost/foobar\","
      "  \"creationTime\" : \"2016-06-30T11:01:37.000Z\","
      "  \"expirationTime\" : \"2016-07-01T11:01:37.000Z\","
      "  \"attribution\" : \"Foo News\","
      "  \"imageUrl\" : \"http://localhost/foobar.jpg\","
      "  \"ampUrl\" : \"http://localhost/amp\","
      "  \"faviconUrl\" : \"http://localhost/favicon.ico\" "
      "}";
  auto snippet = SnippetFromContentSuggestionJSON(kJsonStr, base::Time());
  ASSERT_THAT(snippet, NotNull());

  EXPECT_EQ(snippet->id(), "http://localhost/foobar");
  EXPECT_THAT(snippet->GetAllIDs(),
              ElementsAre("http://localhost/foobar", "012345"));
}

TEST(RemoteSuggestionTest, CreateFromProtoToProtoRoundtrip) {
  SnippetProto proto;
  proto.add_ids("foo");
  proto.add_ids("bar");
  proto.set_title("a suggestion title");
  proto.set_snippet("the snippet describing the suggestion.");
  proto.set_salient_image_url("http://google.com/logo/");
  proto.set_publish_date(1476095492);
  proto.set_expiry_date(1476354691);
  proto.set_score(0.1f);
  proto.set_dismissed(false);
  proto.set_remote_category_id(1);
  proto.set_fetch_date(1476364691);
  auto* source = proto.add_sources();
  source->set_url("http://cool-suggestions.com/");
  source->set_publisher_name("Great Suggestions Inc.");
  source->set_amp_url("http://cdn.ampproject.org/c/foo/");

  std::unique_ptr<RemoteSuggestion> snippet =
      RemoteSuggestion::CreateFromProto(proto);
  ASSERT_THAT(snippet, NotNull());
  // The snippet database relies on the fact that the first id in the protocol
  // buffer is considered the unique id.
  EXPECT_EQ(snippet->id(), "foo");
  // Unfortunately, we only have MessageLite protocol buffers in Chrome, so
  // comparing via DebugString() or MessageDifferencer is not working.
  // So we either need to compare field-by-field (maintenance heavy) or
  // compare the binary version (unusable diagnostic). Deciding for the latter.
  std::string proto_serialized, round_tripped_serialized;
  proto.SerializeToString(&proto_serialized);
  snippet->ToProto().SerializeToString(&round_tripped_serialized);
  EXPECT_EQ(proto_serialized, round_tripped_serialized);
}

TEST(RemoteSuggestionTest, CreateFromProtoIgnoreMissingFetchDate) {
  SnippetProto proto;
  proto.add_ids("foo");
  proto.add_ids("bar");
  proto.set_title("a suggestion title");
  proto.set_snippet("the snippet describing the suggestion.");
  proto.set_salient_image_url("http://google.com/logo/");
  proto.set_publish_date(1476095492);
  proto.set_expiry_date(1476354691);
  proto.set_score(0.1f);
  proto.set_dismissed(false);
  proto.set_remote_category_id(1);
  auto* source = proto.add_sources();
  source->set_url("http://cool-suggestions.com/");
  source->set_publisher_name("Great Suggestions Inc.");
  source->set_amp_url("http://cdn.ampproject.org/c/foo/");

  std::unique_ptr<RemoteSuggestion> snippet =
      RemoteSuggestion::CreateFromProto(proto);
  ASSERT_THAT(snippet, NotNull());
  // The snippet database relies on the fact that the first id in the protocol
  // buffer is considered the unique id.
  EXPECT_EQ(snippet->id(), "foo");
  EXPECT_EQ(snippet->fetch_date(), base::Time());
}

// New form, from chromecontentsuggestions-pa.googleapis.com.
std::unique_ptr<base::DictionaryValue> ContentSuggestionSnippet() {
  const std::string kJsonStr =
      "{"
      "  \"ids\" : [\"http://localhost/foobar\"],"
      "  \"title\" : \"Foo Barred from Baz\","
      "  \"snippet\" : \"...\","
      "  \"fullPageUrl\" : \"http://localhost/foobar\","
      "  \"creationTime\" : \"2016-06-30T11:01:37.000Z\","
      "  \"expirationTime\" : \"2016-07-01T11:01:37.000Z\","
      "  \"attribution\" : \"Foo News\","
      "  \"imageUrl\" : \"http://localhost/foobar.jpg\","
      "  \"ampUrl\" : \"http://localhost/amp\","
      "  \"faviconUrl\" : \"http://localhost/favicon.ico\", "
      "  \"score\": 9001\n"
      "}";
  auto json_value = base::JSONReader::Read(kJsonStr);
  base::DictionaryValue* json_dict;
  CHECK(json_value->GetAsDictionary(&json_dict));
  return json_dict->CreateDeepCopy();
}

TEST(RemoteSuggestionTest, NotifcationInfoAllSpecified) {
  auto json = ContentSuggestionSnippet();
  json->SetBoolean("notificationInfo.shouldNotify", true);
  json->SetString("notificationInfo.deadline", "2016-06-30T13:01:37.000Z");
  auto snippet = RemoteSuggestion::CreateFromContentSuggestionsDictionary(
      *json, 0, base::Time());
  EXPECT_TRUE(snippet->should_notify());
  EXPECT_EQ(7200.0f,
            (snippet->notification_deadline() - snippet->publish_date())
                .InSecondsF());
}

TEST(RemoteSuggestionTest, NotificationInfoDeadlineInvalid) {
  auto json = ContentSuggestionSnippet();
  json->SetBoolean("notificationInfo.shouldNotify", true);
  json->SetInteger("notificationInfo.notificationDeadline", 0);
  auto snippet = RemoteSuggestion::CreateFromContentSuggestionsDictionary(
      *json, 0, base::Time());
  EXPECT_TRUE(snippet->should_notify());
  EXPECT_EQ(base::Time::Max(), snippet->notification_deadline());
}

TEST(RemoteSuggestionTest, NotificationInfoDeadlineAbsent) {
  auto json = ContentSuggestionSnippet();
  json->SetBoolean("notificationInfo.shouldNotify", true);
  auto snippet = RemoteSuggestion::CreateFromContentSuggestionsDictionary(
      *json, 0, base::Time());
  EXPECT_TRUE(snippet->should_notify());
  EXPECT_EQ(base::Time::Max(), snippet->notification_deadline());
}

TEST(RemoteSuggestionTest, NotificationInfoShouldNotifyInvalid) {
  auto json = ContentSuggestionSnippet();
  json->SetString("notificationInfo.shouldNotify", "non-bool");
  auto snippet = RemoteSuggestion::CreateFromContentSuggestionsDictionary(
      *json, 0, base::Time());
  EXPECT_FALSE(snippet->should_notify());
}

TEST(RemoteSuggestionTest, NotificationInfoAbsent) {
  auto json = ContentSuggestionSnippet();
  auto snippet = RemoteSuggestion::CreateFromContentSuggestionsDictionary(
      *json, 0, base::Time());
  EXPECT_FALSE(snippet->should_notify());
}

TEST(RemoteSuggestionTest, ToContentSuggestion) {
  auto json = ContentSuggestionSnippet();
  const base::Time fetch_date = base::Time::FromInternalValue(1466634774L);
  auto snippet = RemoteSuggestion::CreateFromContentSuggestionsDictionary(
      *json, 0, fetch_date);
  ASSERT_THAT(snippet, NotNull());
  ContentSuggestion sugg = snippet->ToContentSuggestion(
      Category::FromKnownCategory(KnownCategories::ARTICLES));

  EXPECT_THAT(sugg.id().category(),
              Eq(Category::FromKnownCategory(KnownCategories::ARTICLES)));
  EXPECT_THAT(sugg.id().id_within_category(), Eq("http://localhost/foobar"));
  EXPECT_THAT(sugg.url(), Eq(GURL("http://localhost/amp")));
  EXPECT_THAT(sugg.title(), Eq(base::UTF8ToUTF16("Foo Barred from Baz")));
  EXPECT_THAT(sugg.snippet_text(), Eq(base::UTF8ToUTF16("...")));
  EXPECT_THAT(sugg.publish_date().ToJavaTime(), Eq(1467284497000));
  EXPECT_THAT(sugg.publisher_name(), Eq(base::UTF8ToUTF16("Foo News")));
  EXPECT_THAT(sugg.score(), Eq(9001));
  EXPECT_THAT(sugg.download_suggestion_extra(), IsNull());
  EXPECT_THAT(sugg.recent_tab_suggestion_extra(), IsNull());
  EXPECT_THAT(sugg.notification_extra(), IsNull());
  EXPECT_THAT(sugg.fetch_date(), Eq(fetch_date));
}

TEST(RemoteSuggestionTest, ToContentSuggestionWithNotificationInfo) {
  auto json = ContentSuggestionSnippet();
  json->SetBoolean("notificationInfo.shouldNotify", true);
  json->SetString("notificationInfo.deadline", "2016-06-30T13:01:37.000Z");
  auto snippet = RemoteSuggestion::CreateFromContentSuggestionsDictionary(
      *json, 0, base::Time());
  ASSERT_THAT(snippet, NotNull());
  ContentSuggestion sugg = snippet->ToContentSuggestion(
      Category::FromKnownCategory(KnownCategories::ARTICLES));

  EXPECT_THAT(sugg.id().category(),
              Eq(Category::FromKnownCategory(KnownCategories::ARTICLES)));
  EXPECT_THAT(sugg.id().id_within_category(), Eq("http://localhost/foobar"));
  EXPECT_THAT(sugg.url(), Eq(GURL("http://localhost/amp")));
  EXPECT_THAT(sugg.title(), Eq(base::UTF8ToUTF16("Foo Barred from Baz")));
  EXPECT_THAT(sugg.snippet_text(), Eq(base::UTF8ToUTF16("...")));
  EXPECT_THAT(sugg.publish_date().ToJavaTime(), Eq(1467284497000));
  EXPECT_THAT(sugg.publisher_name(), Eq(base::UTF8ToUTF16("Foo News")));
  EXPECT_THAT(sugg.score(), Eq(9001));
  EXPECT_THAT(sugg.download_suggestion_extra(), IsNull());
  EXPECT_THAT(sugg.recent_tab_suggestion_extra(), IsNull());
  ASSERT_THAT(sugg.notification_extra(), NotNull());
  EXPECT_THAT(sugg.notification_extra()->deadline.ToJavaTime(),
              Eq(1467291697000));
}

}  // namespace
}  // namespace ntp_snippets
