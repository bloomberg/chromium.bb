// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/omnibox_result.h"

#include "base/memory/scoped_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/app_list/app_list_test_util.h"
#include "chrome/browser/ui/app_list/test/test_app_list_controller_delegate.h"
#include "chrome/test/base/testing_profile.h"
#include "components/omnibox/autocomplete_match.h"
#include "components/omnibox/autocomplete_match_type.h"
#include "components/search_engines/template_url.h"

namespace app_list {
namespace test {

namespace {

const char kFullQuery[] = "Hello World";
const char kPartialQuery[] = "Hello Wo";
const char kExampleDescription[] = "A website";
const char kExampleUrl[] = "http://example.com/hello";
const int kRelevance = 750;
const double kAppListRelevance = 0.5;

const char kWeatherQuery[] = "weather";
const char kExampleKeyword[] = "example.com";
const char kExampleWeatherUrl[] = "http://example.com/weather";
const char kGoogleKeyword[] = "google.com";
const char kGoogleDescription[] = "Google Search";
const char kGoogleWeatherUrl[] = "http://google.com.au/search?q=weather";

}  // namespace

class OmniboxResultTest : public AppListTestBase {
 public:
  OmniboxResultTest() {}
  ~OmniboxResultTest() override {}

  // AppListTestBase overrides:
  void SetUp() override {
    AppListTestBase::SetUp();

    app_list_controller_delegate_.reset(
        new ::test::TestAppListControllerDelegate);
  }

  scoped_ptr<OmniboxResult> CreateOmniboxResult(
      const std::string& original_query,
      int relevance,
      const std::string& destination_url,
      const std::string& contents,
      const std::string& description,
      AutocompleteMatchType::Type type,
      const std::string& keyword,
      bool is_voice_query) {
    AutocompleteMatch match;
    match.search_terms_args.reset(
        new TemplateURLRef::SearchTermsArgs(base::UTF8ToUTF16(original_query)));
    match.search_terms_args->original_query = base::UTF8ToUTF16(original_query);
    match.relevance = relevance;
    match.destination_url = GURL(destination_url);
    match.contents = base::UTF8ToUTF16(contents);
    match.description = base::UTF8ToUTF16(description);
    match.type = type;
    match.keyword = base::UTF8ToUTF16(keyword);

    return scoped_ptr<OmniboxResult>(
        new OmniboxResult(profile_.get(), app_list_controller_delegate_.get(),
                          nullptr, is_voice_query, match));
  }

  const GURL& GetLastOpenedUrl() const {
    return app_list_controller_delegate_->last_opened_url();
  }

 private:
  scoped_ptr<::test::TestAppListControllerDelegate>
      app_list_controller_delegate_;

  DISALLOW_COPY_AND_ASSIGN(OmniboxResultTest);
};

TEST_F(OmniboxResultTest, Basic) {
  scoped_ptr<OmniboxResult> result = CreateOmniboxResult(
      kFullQuery, kRelevance, kExampleUrl, kFullQuery, kExampleDescription,
      AutocompleteMatchType::HISTORY_URL, kExampleKeyword, false);

  EXPECT_EQ(base::ASCIIToUTF16(kFullQuery), result->title());
  EXPECT_EQ(base::ASCIIToUTF16(kExampleDescription), result->details());
  EXPECT_EQ(kAppListRelevance, result->relevance());
  EXPECT_FALSE(result->voice_result());

  result->Open(0);
  EXPECT_EQ(kExampleUrl, GetLastOpenedUrl().spec());
}

TEST_F(OmniboxResultTest, VoiceResult) {
  // Searching for part of a word, and getting a HISTORY_URL result with that
  // exact string in the title. Should not be automatically launchable as a
  // voice result (because it is not a web search type result).
  {
    scoped_ptr<OmniboxResult> result = CreateOmniboxResult(
        kPartialQuery, kRelevance, kExampleUrl, kPartialQuery,
        kExampleDescription, AutocompleteMatchType::HISTORY_URL,
        kExampleKeyword, false);
    EXPECT_FALSE(result->voice_result());
  }

  // Searching for part of a word, and getting a SEARCH_WHAT_YOU_TYPED result.
  // Such a result should be promoted as a voice result.
  {
    scoped_ptr<OmniboxResult> result = CreateOmniboxResult(
        kPartialQuery, kRelevance, kExampleUrl, kPartialQuery,
        kExampleDescription, AutocompleteMatchType::SEARCH_WHAT_YOU_TYPED,
        kExampleKeyword, false);
    EXPECT_TRUE(result->voice_result());
  }

  // Searching for part of a word, and getting a SEARCH_HISTORY result for that
  // exact string. Such a result should be promoted as a voice result.
  {
    scoped_ptr<OmniboxResult> result = CreateOmniboxResult(
        kPartialQuery, kRelevance, kExampleUrl, kPartialQuery,
        kExampleDescription, AutocompleteMatchType::SEARCH_HISTORY,
        kExampleKeyword, false);
    EXPECT_TRUE(result->voice_result());
  }

  // Searching for part of a word, and getting a SEARCH_HISTORY result for a
  // different string. Should not be automatically launchable as a voice result
  // (because it does not exactly match what you typed).
  {
    scoped_ptr<OmniboxResult> result = CreateOmniboxResult(
        kPartialQuery, kRelevance, kExampleUrl, kFullQuery, kExampleDescription,
        AutocompleteMatchType::SEARCH_HISTORY, kExampleKeyword, false);
    EXPECT_FALSE(result->voice_result());
  }
}

TEST_F(OmniboxResultTest, VoiceQuery) {
  // A voice query to a random domain. URL should not change.
  {
    scoped_ptr<OmniboxResult> result = CreateOmniboxResult(
        kWeatherQuery, kRelevance, kExampleWeatherUrl, kWeatherQuery,
        kExampleDescription, AutocompleteMatchType::SEARCH_WHAT_YOU_TYPED,
        kExampleKeyword, true);
    result->Open(0);
    EXPECT_EQ(kExampleWeatherUrl, GetLastOpenedUrl().spec());
  }

  // A voice query to a Google domain. URL should have magic "speak back" query
  // parameter appended.
  {
    scoped_ptr<OmniboxResult> result = CreateOmniboxResult(
        kWeatherQuery, kRelevance, kGoogleWeatherUrl, kWeatherQuery,
        kGoogleDescription, AutocompleteMatchType::SEARCH_WHAT_YOU_TYPED,
        kGoogleKeyword, true);
    result->Open(0);
    EXPECT_EQ("http://google.com.au/search?q=weather&gs_ivs=1",
              GetLastOpenedUrl().spec());
  }

  // A non-voice query to a Google domain. URL should not change.
  {
    scoped_ptr<OmniboxResult> result = CreateOmniboxResult(
        kWeatherQuery, kRelevance, kGoogleWeatherUrl, kWeatherQuery,
        kGoogleDescription, AutocompleteMatchType::SEARCH_WHAT_YOU_TYPED,
        kGoogleKeyword, false);
    result->Open(0);
    EXPECT_EQ(kGoogleWeatherUrl, GetLastOpenedUrl().spec());
  }
}

}  // namespace test
}  // namespace app_list
