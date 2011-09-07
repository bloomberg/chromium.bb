// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/autocomplete/autocomplete_match.h"
#include "chrome/browser/autocomplete/builtin_provider.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/testing_browser_process.h"
#include "googleurl/src/gurl.h"
#include "testing/gtest/include/gtest/gtest.h"

class BuiltinProviderTest : public testing::Test {
 protected:
  template<class ResultType>
  struct test_data {
    const string16 input;
    const size_t num_results;
    const ResultType output[3];
  };

  BuiltinProviderTest() : builtin_provider_(NULL) { }
  virtual ~BuiltinProviderTest() { }

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
    AutocompleteInput input(builtin_cases[i].input, string16(), true,
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
      }
    }
  }
}

TEST_F(BuiltinProviderTest, TypingScheme) {
  const string16 kAbout = ASCIIToUTF16(chrome::kAboutScheme);
  const string16 kChrome = ASCIIToUTF16(chrome::kChromeUIScheme);
  const string16 kSeparator1 = ASCIIToUTF16(":");
  const string16 kSeparator2 = ASCIIToUTF16(":/");
  const string16 kSeparator3 = ASCIIToUTF16(chrome::kStandardSchemeSeparator);

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

TEST_F(BuiltinProviderTest, DISABLED_ChromeURLs) {
  const string16 kAbout = ASCIIToUTF16(chrome::kAboutScheme);
  const string16 kChrome = ASCIIToUTF16(chrome::kChromeUIScheme);
  const string16 kSeparator1 = ASCIIToUTF16(":");
  const string16 kSeparator2 = ASCIIToUTF16(":/");
  const string16 kSeparator3 = ASCIIToUTF16(chrome::kStandardSchemeSeparator);

  // This makes assumptions about the chrome URLs listed by the BuiltinProvider.
  // Currently they are derived from ChromePaths() in browser_about_handler.cc.
  const string16 kHostE = ASCIIToUTF16(chrome::kChromeUIExtensionsHost);
  const GURL kURLE = GURL(kChrome + kSeparator3 + kHostE);
  const string16 kHostF1 = ASCIIToUTF16(chrome::kChromeUIFlagsHost);
  const string16 kHostF2 = ASCIIToUTF16(chrome::kChromeUIFlashHost);
  const GURL kURLF1 = GURL(kChrome + kSeparator3 + kHostF1);
  const GURL kURLF2 = GURL(kChrome + kSeparator3 + kHostF2);

  test_data<GURL> chrome_url_cases[] = {
    // Typing an about URL with an unknown host should give nothing.
    {kAbout + kSeparator1 + ASCIIToUTF16("host"), 0, {}},
    {kAbout + kSeparator2 + ASCIIToUTF16("host"), 0, {}},
    {kAbout + kSeparator3 + ASCIIToUTF16("host"), 0, {}},

    // Typing a chrome URL with an unknown host should give nothing.
    {kChrome + kSeparator1 + ASCIIToUTF16("host"), 0, {}},
    {kChrome + kSeparator2 + ASCIIToUTF16("host"), 0, {}},
    {kChrome + kSeparator3 + ASCIIToUTF16("host"), 0, {}},

    // Typing an about URL for a unique host should provide that full URL.
    {kAbout + kSeparator1 + kHostE.substr(0, 1),                   1, {kURLE}},
    {kAbout + kSeparator2 + kHostE.substr(0, 2),                   1, {kURLE}},
    {kAbout + kSeparator3 + kHostE.substr(0, kHostE.length() - 1), 1, {kURLE}},
    {kAbout + kSeparator1 + kHostE,                                1, {kURLE}},
    {kAbout + kSeparator2 + kHostE,                                1, {kURLE}},
    {kAbout + kSeparator3 + kHostE,                                1, {kURLE}},

    // Typing a chrome URL for a unique host should provide that full URL.
    {kChrome + kSeparator1 + kHostE.substr(0, 1),                   1, {kURLE}},
    {kChrome + kSeparator2 + kHostE.substr(0, 2),                   1, {kURLE}},
    {kChrome + kSeparator3 + kHostE.substr(0, kHostE.length() - 1), 1, {kURLE}},
    {kChrome + kSeparator1 + kHostE,                                1, {kURLE}},
    {kChrome + kSeparator2 + kHostE,                                1, {kURLE}},
    {kChrome + kSeparator3 + kHostE,                                1, {kURLE}},

    // Typing an about URL with a non-unique host should provide matching URLs.
    {kAbout + kSeparator1 + kHostF1.substr(0, 1), 2, {kURLF1, kURLF2}},
    {kAbout + kSeparator2 + kHostF1.substr(0, 2), 2, {kURLF1, kURLF2}},
    {kAbout + kSeparator3 + kHostF2.substr(0, 3), 2, {kURLF1, kURLF2}},
    {kAbout + kSeparator3 + kHostF1.substr(0, 4), 1, {kURLF1}},
    {kAbout + kSeparator3 + kHostF2.substr(0, 4), 1, {kURLF2}},
    {kAbout + kSeparator3 + kHostF1,              1, {kURLF1}},
    {kAbout + kSeparator2 + kHostF2,              1, {kURLF2}},

    // Typing a chrome URL with a non-unique host should provide matching URLs.
    {kChrome + kSeparator1 + kHostF1.substr(0, 1), 2, {kURLF1, kURLF2}},
    {kChrome + kSeparator2 + kHostF1.substr(0, 2), 2, {kURLF1, kURLF2}},
    {kChrome + kSeparator3 + kHostF2.substr(0, 3), 2, {kURLF1, kURLF2}},
    {kChrome + kSeparator3 + kHostF1.substr(0, 4), 1, {kURLF1}},
    {kChrome + kSeparator3 + kHostF2.substr(0, 4), 1, {kURLF2}},
    {kChrome + kSeparator3 + kHostF1,              1, {kURLF1}},
    {kChrome + kSeparator2 + kHostF2,              1, {kURLF2}},
  };

  RunTest<GURL>(chrome_url_cases, arraysize(chrome_url_cases),
                &AutocompleteMatch::destination_url);
}

TEST_F(BuiltinProviderTest, ChromeSettingsSubpages) {
  // This makes assumptions about the chrome URLs listed by the BuiltinProvider.
  // Currently they are derived from ChromePaths() in browser_about_handler.cc.
  const string16 kSettings = ASCIIToUTF16(chrome::kChromeUISettingsURL);
  const string16 kDefaultPage1 = ASCIIToUTF16(chrome::kAdvancedOptionsSubPage);
  const string16 kDefaultPage2 = ASCIIToUTF16(chrome::kAutofillSubPage);
  const GURL kDefaultURL1 = GURL(kSettings + kDefaultPage1);
  const GURL kDefaultURL2 = GURL(kSettings + kDefaultPage2);
  const string16 kPage1 = ASCIIToUTF16(chrome::kPersonalOptionsSubPage);
  const string16 kPage2 = ASCIIToUTF16(chrome::kPasswordManagerSubPage);
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
