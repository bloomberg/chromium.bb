// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autocomplete/builtin_provider.h"

#include "base/format_macros.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/autocomplete/chrome_autocomplete_scheme_classifier.h"
#include "chrome/common/url_constants.h"
#include "components/metrics/proto/omnibox_event.pb.h"
#include "components/omnibox/autocomplete_input.h"
#include "components/omnibox/autocomplete_match.h"
#include "components/omnibox/autocomplete_provider.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using base::ASCIIToUTF16;

class BuiltinProviderTest : public testing::Test {
 protected:
  struct TestData {
    const base::string16 input;
    const size_t num_results;
    const GURL output[3];
  };

  BuiltinProviderTest() : provider_(NULL) {}
  virtual ~BuiltinProviderTest() {}

  virtual void SetUp() OVERRIDE { provider_ = new BuiltinProvider(); }
  virtual void TearDown() OVERRIDE { provider_ = NULL; }

  void RunTest(const TestData cases[], size_t num_cases) {
    ACMatches matches;
    for (size_t i = 0; i < num_cases; ++i) {
      SCOPED_TRACE(base::StringPrintf(
          "case %" PRIuS ": %s", i, base::UTF16ToUTF8(cases[i].input).c_str()));
      const AutocompleteInput input(cases[i].input, base::string16::npos,
                                    base::string16(), GURL(),
                                    metrics::OmniboxEventProto::INVALID_SPEC,
                                    true, false, true, true,
                                    ChromeAutocompleteSchemeClassifier(NULL));
      provider_->Start(input, false);
      EXPECT_TRUE(provider_->done());
      matches = provider_->matches();
      EXPECT_EQ(cases[i].num_results, matches.size());
      if (matches.size() == cases[i].num_results) {
        for (size_t j = 0; j < cases[i].num_results; ++j) {
          EXPECT_EQ(cases[i].output[j], matches[j].destination_url);
          EXPECT_FALSE(matches[j].allowed_to_be_default_match);
        }
      }
    }
  }

 private:
  scoped_refptr<BuiltinProvider> provider_;

  DISALLOW_COPY_AND_ASSIGN(BuiltinProviderTest);
};

#if !defined(OS_ANDROID)
TEST_F(BuiltinProviderTest, TypingScheme) {
  const base::string16 kAbout = ASCIIToUTF16(url::kAboutScheme);
  const base::string16 kChrome = ASCIIToUTF16(content::kChromeUIScheme);
  const base::string16 kSeparator1 = ASCIIToUTF16(":");
  const base::string16 kSeparator2 = ASCIIToUTF16(":/");
  const base::string16 kSeparator3 =
      ASCIIToUTF16(url::kStandardSchemeSeparator);

  // These default URLs should correspond with those in BuiltinProvider::Start.
  const GURL kURL1 = GURL(chrome::kChromeUIChromeURLsURL);
  const GURL kURL2 = GURL(chrome::kChromeUISettingsURL);
  const GURL kURL3 = GURL(chrome::kChromeUIVersionURL);

  TestData typing_scheme_cases[] = {
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

  RunTest(typing_scheme_cases, arraysize(typing_scheme_cases));
}
#else // Android uses a subset of the URLs
TEST_F(BuiltinProviderTest, TypingScheme) {
  const base::string16 kAbout = ASCIIToUTF16(url::kAboutScheme);
  const base::string16 kChrome = ASCIIToUTF16(content::kChromeUIScheme);
  const base::string16 kSeparator1 = ASCIIToUTF16(":");
  const base::string16 kSeparator2 = ASCIIToUTF16(":/");
  const base::string16 kSeparator3 =
      ASCIIToUTF16(url::kStandardSchemeSeparator);

  // These default URLs should correspond with those in BuiltinProvider::Start.
  const GURL kURL1 = GURL(chrome::kChromeUIChromeURLsURL);
  const GURL kURL2 = GURL(chrome::kChromeUIVersionURL);

  TestData typing_scheme_cases[] = {
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
    {kAbout.substr(0, 1),      2, {kURL1, kURL2}},
    {ASCIIToUTF16("A"),        2, {kURL1, kURL2}},
    {kAbout,                   2, {kURL1, kURL2}},
    {kAbout + kSeparator1,     2, {kURL1, kURL2}},
    {kAbout + kSeparator2,     2, {kURL1, kURL2}},
    {kAbout + kSeparator3,     2, {kURL1, kURL2}},
    {ASCIIToUTF16("aBoUT://"), 2, {kURL1, kURL2}},

    // Typing a portion of chrome:// should give the default urls.
    {kChrome.substr(0, 1),      2, {kURL1, kURL2}},
    {ASCIIToUTF16("C"),         2, {kURL1, kURL2}},
    {kChrome,                   2, {kURL1, kURL2}},
    {kChrome + kSeparator1,     2, {kURL1, kURL2}},
    {kChrome + kSeparator2,     2, {kURL1, kURL2}},
    {kChrome + kSeparator3,     2, {kURL1, kURL2}},
    {ASCIIToUTF16("ChRoMe://"), 2, {kURL1, kURL2}},
  };

  RunTest(typing_scheme_cases, arraysize(typing_scheme_cases));
}
#endif

TEST_F(BuiltinProviderTest, NonChromeURLs) {
  TestData non_chrome_url_cases[] = {
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

  RunTest(non_chrome_url_cases, arraysize(non_chrome_url_cases));
}

TEST_F(BuiltinProviderTest, ChromeURLs) {
  const base::string16 kAbout = ASCIIToUTF16(url::kAboutScheme);
  const base::string16 kChrome = ASCIIToUTF16(content::kChromeUIScheme);
  const base::string16 kSeparator1 = ASCIIToUTF16(":");
  const base::string16 kSeparator2 = ASCIIToUTF16(":/");
  const base::string16 kSeparator3 =
      ASCIIToUTF16(url::kStandardSchemeSeparator);

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

  TestData chrome_url_cases[] = {
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

  RunTest(chrome_url_cases, arraysize(chrome_url_cases));
}

TEST_F(BuiltinProviderTest, AboutBlank) {
  const base::string16 kAbout = ASCIIToUTF16(url::kAboutScheme);
  const base::string16 kChrome = ASCIIToUTF16(content::kChromeUIScheme);
  const base::string16 kAboutBlank = ASCIIToUTF16(url::kAboutBlankURL);
  const base::string16 kBlank = ASCIIToUTF16("blank");
  const base::string16 kSeparator1 =
      ASCIIToUTF16(url::kStandardSchemeSeparator);
  const base::string16 kSeparator2 = ASCIIToUTF16(":///");
  const base::string16 kSeparator3 = ASCIIToUTF16(";///");

  const GURL kURLBlob = GURL(kChrome + kSeparator1 +
                             ASCIIToUTF16(content::kChromeUIBlobInternalsHost));
  const GURL kURLBlank = GURL(kAboutBlank);

  TestData about_blank_cases[] = {
    // Typing an about:blank prefix should yield about:blank, among other URLs.
    {kAboutBlank.substr(0, 8), 2, {kURLBlank, kURLBlob}},
    {kAboutBlank.substr(0, 9), 1, {kURLBlank}},

    // Using any separator that is supported by fixup should yield about:blank.
    // For now, BuiltinProvider does not suggest url-what-you-typed matches for
    // for about:blank; check "about:blan" and "about;blan" substrings instead.
    {kAbout + kSeparator2.substr(0, 1) + kBlank.substr(0, 4), 1, {kURLBlank}},
    {kAbout + kSeparator2.substr(0, 2) + kBlank,              1, {kURLBlank}},
    {kAbout + kSeparator2.substr(0, 3) + kBlank,              1, {kURLBlank}},
    {kAbout + kSeparator2 + kBlank,                           1, {kURLBlank}},
    {kAbout + kSeparator3.substr(0, 1) + kBlank.substr(0, 4), 1, {kURLBlank}},
    {kAbout + kSeparator3.substr(0, 2) + kBlank,              1, {kURLBlank}},
    {kAbout + kSeparator3.substr(0, 3) + kBlank,              1, {kURLBlank}},
    {kAbout + kSeparator3 + kBlank,                           1, {kURLBlank}},

    // Using the chrome scheme should not yield about:blank.
    {kChrome + kSeparator1.substr(0, 1) + kBlank, 0, {}},
    {kChrome + kSeparator1.substr(0, 2) + kBlank, 0, {}},
    {kChrome + kSeparator1.substr(0, 3) + kBlank, 0, {}},
    {kChrome + kSeparator1 + kBlank,              0, {}},

    // Adding trailing text should not yield about:blank.
    {kAboutBlank + ASCIIToUTF16("/"),  0, {}},
    {kAboutBlank + ASCIIToUTF16("/p"), 0, {}},
    {kAboutBlank + ASCIIToUTF16("x"),  0, {}},
    {kAboutBlank + ASCIIToUTF16("?q"), 0, {}},
    {kAboutBlank + ASCIIToUTF16("#r"), 0, {}},

    // Interrupting "blank" with conflicting text should not yield about:blank.
    {kAboutBlank.substr(0, 9) + ASCIIToUTF16("/"),  0, {}},
    {kAboutBlank.substr(0, 9) + ASCIIToUTF16("/p"), 0, {}},
    {kAboutBlank.substr(0, 9) + ASCIIToUTF16("x"),  0, {}},
    {kAboutBlank.substr(0, 9) + ASCIIToUTF16("?q"), 0, {}},
    {kAboutBlank.substr(0, 9) + ASCIIToUTF16("#r"), 0, {}},
  };

  RunTest(about_blank_cases, arraysize(about_blank_cases));
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

  TestData settings_subpage_cases[] = {
    // Typing the settings path should show settings and the first two subpages.
    {kSettings, 3, {GURL(kSettings), kDefaultURL1, kDefaultURL2}},

    // Typing a subpage path should return the appropriate results.
    {kSettings + kPage1.substr(0, 1),                   2, {kURL1, kURL2}},
    {kSettings + kPage1.substr(0, 2),                   1, {kURL1}},
    {kSettings + kPage1.substr(0, kPage1.length() - 1), 1, {kURL1}},
    {kSettings + kPage1,                                1, {kURL1}},
    {kSettings + kPage2,                                1, {kURL2}},
  };

  RunTest(settings_subpage_cases, arraysize(settings_subpage_cases));
}
#endif
