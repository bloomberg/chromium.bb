// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autocomplete/builtin_provider.h"

#include "base/message_loop/message_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/autocomplete/autocomplete_input.h"
#include "chrome/browser/autocomplete/autocomplete_match.h"
#include "chrome/browser/autocomplete/autocomplete_provider.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/testing_browser_process.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using base::ASCIIToUTF16;

class BuiltinProviderTest : public testing::Test {
 protected:
  template<class ResultType>
  struct test_data {
    const base::string16 input;
    const size_t num_results;
    const ResultType output[3];
  };

  BuiltinProviderTest() : builtin_provider_(NULL) {}
  virtual ~BuiltinProviderTest() {}

  virtual void SetUp();
  virtual void TearDown();

  template<class ResultType>
  void RunTest(test_data<ResultType>* builtin_cases,
               int num_cases,
               ResultType AutocompleteMatch::* member);

 protected:
  scoped_refptr<BuiltinProvider> builtin_provider_;
};

void BuiltinProviderTest::SetUp() {
  builtin_provider_ = new BuiltinProvider(NULL, NULL);
}

void BuiltinProviderTest::TearDown() {
  builtin_provider_ = NULL;
}

template<class ResultType>
void BuiltinProviderTest::RunTest(test_data<ResultType>* builtin_cases,
                                  int num_cases,
                                  ResultType AutocompleteMatch::* member) {
  ACMatches matches;
  for (int i = 0; i < num_cases; ++i) {
    AutocompleteInput input(builtin_cases[i].input, base::string16::npos,
                            base::string16(), GURL(),
                            AutocompleteInput::INVALID_SPEC, true,
                            false, true, AutocompleteInput::ALL_MATCHES);
    builtin_provider_->Start(input, false);
    EXPECT_TRUE(builtin_provider_->done());
    matches = builtin_provider_->matches();
    EXPECT_EQ(builtin_cases[i].num_results, matches.size()) <<
                ASCIIToUTF16("Input was: ") << builtin_cases[i].input;
    if (matches.size() == builtin_cases[i].num_results) {
      for (size_t j = 0; j < builtin_cases[i].num_results; ++j) {
        EXPECT_EQ(builtin_cases[i].output[j], matches[j].*member) <<
                ASCIIToUTF16("Input was: ") << builtin_cases[i].input;
        EXPECT_FALSE(matches[j].allowed_to_be_default_match);
      }
    }
  }
}

TEST_F(BuiltinProviderTest, TypingScheme) {
  const base::string16 kAbout = ASCIIToUTF16(content::kAboutScheme);
  const base::string16 kChrome = ASCIIToUTF16(content::kChromeUIScheme);
  const base::string16 kSeparator1 = ASCIIToUTF16(":");
  const base::string16 kSeparator2 = ASCIIToUTF16(":/");
  const base::string16 kSeparator3 =
      ASCIIToUTF16(content::kStandardSchemeSeparator);

  // These default URLs should correspond with those in BuiltinProvider::Start.
  const GURL kURL1 = GURL(chrome::kChromeUIChromeURLsURL);
  const GURL kURL2 = GURL(chrome::kChromeUISettingsURL);
  const GURL kURL3 = GURL(chrome::kChromeUIVersionURL);

  test_data<GURL> typing_scheme_cases[] = {
    // Typing an unrelated scheme should give nothing.
    {ASCIIToUTF16("h"),        0, {}},
    {ASCIIToUTF16("http"),     0, {}},
    {ASCIIToUTF16("file"),     0, {}},
    {ASCIIToUTF16("abouz"),    0, {}},
    {ASCIIToUTF16("aboutt"),   0, {}},
    {ASCIIToUTF16("aboutt:"),  0, {}},
    {ASCIIToUTF16("chroma"),   0, {}},
    {ASCIIToUTF16("chromee"),  0, {}},
    {ASCIIToUTF16("chromee:"), 0, {}},

    // Typing a portion of about:// should give the default urls.
    {kAbout.substr(0, 1),      3, {kURL1, kURL2, kURL3}},
    {ASCIIToUTF16("A"),        3, {kURL1, kURL2, kURL3}},
    {kAbout,                   3, {kURL1, kURL2, kURL3}},
    {kAbout + kSeparator1,     3, {kURL1, kURL2, kURL3}},
    {kAbout + kSeparator2,     3, {kURL1, kURL2, kURL3}},
    {kAbout + kSeparator3,     3, {kURL1, kURL2, kURL3}},
    {ASCIIToUTF16("aBoUT://"), 3, {kURL1, kURL2, kURL3}},

    // Typing a portion of chrome:// should give the default urls.
    {kChrome.substr(0, 1),      3, {kURL1, kURL2, kURL3}},
    {ASCIIToUTF16("C"),         3, {kURL1, kURL2, kURL3}},
    {kChrome,                   3, {kURL1, kURL2, kURL3}},
    {kChrome + kSeparator1,     3, {kURL1, kURL2, kURL3}},
    {kChrome + kSeparator2,     3, {kURL1, kURL2, kURL3}},
    {kChrome + kSeparator3,     3, {kURL1, kURL2, kURL3}},
    {ASCIIToUTF16("ChRoMe://"), 3, {kURL1, kURL2, kURL3}},
  };

  RunTest<GURL>(typing_scheme_cases, arraysize(typing_scheme_cases),
                &AutocompleteMatch::destination_url);
}

TEST_F(BuiltinProviderTest, NonChromeURLs) {
  test_data<GURL> non_chrome_url_cases[] = {
    // Typing an unrelated scheme should give nothing.
    {ASCIIToUTF16("g@rb@g3"),                      0, {}},
    {ASCIIToUTF16("www.google.com"),               0, {}},
    {ASCIIToUTF16("http:www.google.com"),          0, {}},
    {ASCIIToUTF16("http://www.google.com"),        0, {}},
    {ASCIIToUTF16("file:filename"),                0, {}},
    {ASCIIToUTF16("scheme:"),                      0, {}},
    {ASCIIToUTF16("scheme://"),                    0, {}},
    {ASCIIToUTF16("scheme://host"),                0, {}},
    {ASCIIToUTF16("scheme:host/path?query#ref"),   0, {}},
    {ASCIIToUTF16("scheme://host/path?query#ref"), 0, {}},
  };

  RunTest<GURL>(non_chrome_url_cases, arraysize(non_chrome_url_cases),
                &AutocompleteMatch::destination_url);
}

TEST_F(BuiltinProviderTest, ChromeURLs) {
  const base::string16 kAbout = ASCIIToUTF16(content::kAboutScheme);
  const base::string16 kChrome = ASCIIToUTF16(content::kChromeUIScheme);
  const base::string16 kSeparator1 = ASCIIToUTF16(":");
  const base::string16 kSeparator2 = ASCIIToUTF16(":/");
  const base::string16 kSeparator3 =
      ASCIIToUTF16(content::kStandardSchemeSeparator);

  // This makes assumptions about the chrome URLs listed by the BuiltinProvider.
  // Currently they are derived from chrome::kChromeHostURLs[].
  const base::string16 kHostM1 =
      ASCIIToUTF16(content::kChromeUIMediaInternalsHost);
  const base::string16 kHostM2 =
      ASCIIToUTF16(chrome::kChromeUIMemoryHost);
  const base::string16 kHostM3 =
      ASCIIToUTF16(chrome::kChromeUIMemoryInternalsHost);
  const GURL kURLM1 = GURL(kChrome + kSeparator3 + kHostM1);
  const GURL kURLM2 = GURL(kChrome + kSeparator3 + kHostM2);
  const GURL kURLM3 = GURL(kChrome + kSeparator3 + kHostM3);

  test_data<GURL> chrome_url_cases[] = {
    // Typing an about URL with an unknown host should give nothing.
    {kAbout + kSeparator1 + ASCIIToUTF16("host"), 0, {}},
    {kAbout + kSeparator2 + ASCIIToUTF16("host"), 0, {}},
    {kAbout + kSeparator3 + ASCIIToUTF16("host"), 0, {}},

    // Typing a chrome URL with an unknown host should give nothing.
    {kChrome + kSeparator1 + ASCIIToUTF16("host"), 0, {}},
    {kChrome + kSeparator2 + ASCIIToUTF16("host"), 0, {}},
    {kChrome + kSeparator3 + ASCIIToUTF16("host"), 0, {}},

    // Typing an about URL should provide matching URLs.
    {kAbout + kSeparator1 + kHostM1.substr(0, 1), 3, {kURLM1, kURLM2, kURLM3}},
    {kAbout + kSeparator2 + kHostM1.substr(0, 2), 3, {kURLM1, kURLM2, kURLM3}},
    {kAbout + kSeparator3 + kHostM1.substr(0, 3), 1, {kURLM1}},
    {kAbout + kSeparator3 + kHostM2.substr(0, 3), 2, {kURLM2, kURLM3}},
    {kAbout + kSeparator3 + kHostM1,              1, {kURLM1}},
    {kAbout + kSeparator2 + kHostM2,              2, {kURLM2, kURLM3}},
    {kAbout + kSeparator2 + kHostM3,              1, {kURLM3}},

    // Typing a chrome URL should provide matching URLs.
    {kChrome + kSeparator1 + kHostM1.substr(0, 1), 3, {kURLM1, kURLM2, kURLM3}},
    {kChrome + kSeparator2 + kHostM1.substr(0, 2), 3, {kURLM1, kURLM2, kURLM3}},
    {kChrome + kSeparator3 + kHostM1.substr(0, 3), 1, {kURLM1}},
    {kChrome + kSeparator3 + kHostM2.substr(0, 3), 2, {kURLM2, kURLM3}},
    {kChrome + kSeparator3 + kHostM1,              1, {kURLM1}},
    {kChrome + kSeparator2 + kHostM2,              2, {kURLM2, kURLM3}},
    {kChrome + kSeparator2 + kHostM3,              1, {kURLM3}},
  };

  RunTest<GURL>(chrome_url_cases, arraysize(chrome_url_cases),
                &AutocompleteMatch::destination_url);
}

#if !defined(OS_ANDROID)
// Disabled on Android where we use native UI instead of chrome://settings.
TEST_F(BuiltinProviderTest, ChromeSettingsSubpages) {
  // This makes assumptions about the chrome URLs listed by the BuiltinProvider.
  // Currently they are derived from chrome::kChromeHostURLs[].
  const base::string16 kSettings = ASCIIToUTF16(chrome::kChromeUISettingsURL);
  const base::string16 kDefaultPage1 = ASCIIToUTF16(chrome::kAutofillSubPage);
  const base::string16 kDefaultPage2 =
      ASCIIToUTF16(chrome::kClearBrowserDataSubPage);
  const GURL kDefaultURL1 = GURL(kSettings + kDefaultPage1);
  const GURL kDefaultURL2 = GURL(kSettings + kDefaultPage2);
  const base::string16 kPage1 = ASCIIToUTF16(chrome::kSearchEnginesSubPage);
  const base::string16 kPage2 = ASCIIToUTF16(chrome::kSyncSetupSubPage);
  const GURL kURL1 = GURL(kSettings + kPage1);
  const GURL kURL2 = GURL(kSettings + kPage2);

  test_data<GURL> settings_subpage_cases[] = {
    // Typing the settings path should show settings and the first two subpages.
    {kSettings, 3, {GURL(kSettings), kDefaultURL1, kDefaultURL2}},

    // Typing a subpage path should return the appropriate results.
    {kSettings + kPage1.substr(0, 1),                   2, {kURL1, kURL2}},
    {kSettings + kPage1.substr(0, 2),                   1, {kURL1}},
    {kSettings + kPage1.substr(0, kPage1.length() - 1), 1, {kURL1}},
    {kSettings + kPage1,                                1, {kURL1}},
    {kSettings + kPage2,                                1, {kURL2}},
  };

  RunTest<GURL>(settings_subpage_cases, arraysize(settings_subpage_cases),
                &AutocompleteMatch::destination_url);
}
#endif
