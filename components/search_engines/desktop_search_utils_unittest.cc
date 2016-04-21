// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/search_engines/desktop_search_utils.h"

#include <memory>
#include <string>

#include "base/feature_list.h"
#include "base/macros.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "components/prefs/pref_service.h"
#include "components/search_engines/prepopulated_engines.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_prepopulate_data.h"
#include "components/search_engines/template_url_service.h"
#include "components/search_engines/util.h"
#include "components/syncable_prefs/testing_pref_service_syncable.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

struct ReplaceDesktopSearchURLTestData {
  // Value of the |url| argument of
  // ReplaceDesktopSearchURLWithDefaultSearchURLIfNeeded on input.
  const char* input_arg;

  // Expected value of the |url| argument of
  // ReplaceDesktopSearchURLWithDefaultSearchURLIfNeeded on output.
  const char* expected_output_arg;
};

struct ShouldReplaceDesktopSearchURLTestData {
  bool feature_enabled;
  bool default_search_engine_is_bing;
  const GURL* expected_output_arg;
};

}  // namespace

class DesktopSearchUtilsTest : public testing::Test {
 public:
  DesktopSearchUtilsTest() : template_url_service_(nullptr, 0) {
    RegisterDesktopSearchRedirectionPref(prefs_.registry());
  }

 protected:
  void SetFeatureEnabled(bool enabled) {
    base::FeatureList::ClearInstanceForTesting();
    std::unique_ptr<base::FeatureList> feature_list(new base::FeatureList);
    if (enabled) {
      feature_list->InitializeFromCommandLine(
          kDesktopSearchRedirectionFeature.name, std::string());
    }
    base::FeatureList::SetInstance(std::move(feature_list));
  }

  void SetDefaultSearchEngine(
      const TemplateURLPrepopulateData::PrepopulatedEngine
          default_search_engine) {
    std::unique_ptr<TemplateURLData> template_url_data =
        TemplateURLPrepopulateData::MakeTemplateURLDataFromPrepopulatedEngine(
            default_search_engine);
    TemplateURL template_url(*template_url_data);
    template_url_service_.SetUserSelectedDefaultSearchProvider(&template_url);
  }

  TemplateURLService template_url_service_;
  syncable_prefs::TestingPrefServiceSyncable prefs_;

 private:
  DISALLOW_COPY_AND_ASSIGN(DesktopSearchUtilsTest);
};

// Checks that ReplaceDesktopSearchURLWithDefaultSearchURLIfNeeded correctly
// replaces the URL it receives when the desktop search redirection feature is
// enabled and the default search engine is not Bing.
TEST_F(DesktopSearchUtilsTest, ReplaceDesktopSearchURL) {
  const std::string default_search_url_keyword(
      GetDefaultSearchURLForSearchTerms(&template_url_service_,
                                        base::WideToUTF16(L"keyword"))
          .spec());
  const std::string default_search_url_special_chars(
      GetDefaultSearchURLForSearchTerms(&template_url_service_,
                                        base::WideToUTF16(L"\xE8 \xE9"))
          .spec());

  const ReplaceDesktopSearchURLTestData test_data[] = {
      {"https://www.google.com/", "https://www.google.com/"},
      {"https://www.bing.com/search/", "https://www.bing.com/search/"},
      {"https://www.bing.com/search?q=keyword&form=QBLH",
       "https://www.bing.com/search?q=keyword&form=QBLH"},
      {"https://www.bing.com/search?q=keyword&form=WNSGPH",
       default_search_url_keyword.c_str()},
      {"https://www.bing.com/search?q=keyword&form=WNSBOX",
       default_search_url_keyword.c_str()},
      {"https://www.bing.com/search?q=keyword&FORM=WNSGPH",
       default_search_url_keyword.c_str()},
      {"https://www.bing.com/search?q=keyword&FORM=WNSBOX",
       default_search_url_keyword.c_str()},
      {"https://www.bing.com/search?form=WNSGPH&q=keyword",
       default_search_url_keyword.c_str()},
      {"https://www.bing.com/search?q=keyword&form=WNSGPH&other=stuff",
       default_search_url_keyword.c_str()},
      {"https://www.bing.com/search?q=%C3%A8+%C3%A9&form=WNSGPH",
       default_search_url_special_chars.c_str()},
  };

  SetFeatureEnabled(true);

  for (const auto& test : test_data) {
    GURL url(test.input_arg);
    ReplaceDesktopSearchURLWithDefaultSearchURLIfNeeded(
        &prefs_, &template_url_service_, &url);
    EXPECT_EQ(test.expected_output_arg, url.spec());
  }
}

// Checks that ReplaceDesktopSearchURLWithDefaultSearchURLIfNeeded doesn't
// change the URL it receives when the desktop search redirection feature is
// disabled or when the default search engine is Bing.
TEST_F(DesktopSearchUtilsTest, ShouldReplaceDesktopSearchURL) {
  SetDefaultSearchEngine(TemplateURLPrepopulateData::google);
  const GURL desktop_search_url(
      "https://www.bing.com/search?q=keyword&form=WNSGPH");
  const GURL default_search_url(GetDefaultSearchURLForSearchTerms(
      &template_url_service_, base::WideToUTF16(L"keyword")));

  const ShouldReplaceDesktopSearchURLTestData test_data[] = {
      {false, false, &desktop_search_url},
      {true, false, &default_search_url},
      {false, true, &desktop_search_url},
      {true, true, &desktop_search_url},
  };

  for (const auto& test : test_data) {
    SetFeatureEnabled(test.feature_enabled);
    SetDefaultSearchEngine(test.default_search_engine_is_bing
                               ? TemplateURLPrepopulateData::bing
                               : TemplateURLPrepopulateData::google);

    // Check whether the desktop search URL is replaced.
    GURL url(desktop_search_url);
    ReplaceDesktopSearchURLWithDefaultSearchURLIfNeeded(
        &prefs_, &template_url_service_, &url);
    EXPECT_EQ(*test.expected_output_arg, url);
  }
}
