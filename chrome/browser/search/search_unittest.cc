// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/prefs/pref_service.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/search_engines/template_url_service.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/web_contents.h"

namespace chrome {

TEST(EmbeddedSearchFieldTrialTest, GetFieldTrialInfo) {
  FieldTrialFlags flags;
  uint64 group_number = 0;
  const uint64 ZERO = 0;

  EXPECT_FALSE(GetFieldTrialInfo("", &flags, &group_number));
  EXPECT_EQ(ZERO, group_number);
  EXPECT_EQ(ZERO, flags.size());

  EXPECT_TRUE(GetFieldTrialInfo("Group77", &flags, &group_number));
  EXPECT_EQ(uint64(77), group_number);
  EXPECT_EQ(ZERO, flags.size());

  group_number = 0;
  EXPECT_FALSE(GetFieldTrialInfo("Group77.2", &flags, &group_number));
  EXPECT_EQ(ZERO, group_number);
  EXPECT_EQ(ZERO, flags.size());

  EXPECT_FALSE(GetFieldTrialInfo("Invalid77", &flags, &group_number));
  EXPECT_EQ(ZERO, group_number);
  EXPECT_EQ(ZERO, flags.size());

  EXPECT_FALSE(GetFieldTrialInfo("Invalid77", &flags, NULL));
  EXPECT_EQ(ZERO, flags.size());

  EXPECT_TRUE(GetFieldTrialInfo("Group77 ", &flags, &group_number));
  EXPECT_EQ(uint64(77), group_number);
  EXPECT_EQ(ZERO, flags.size());

  group_number = 0;
  flags.clear();
  EXPECT_EQ(uint64(9999), GetUInt64ValueForFlagWithDefault("foo", 9999, flags));
  EXPECT_TRUE(GetFieldTrialInfo("Group77 foo:6", &flags, &group_number));
  EXPECT_EQ(uint64(77), group_number);
  EXPECT_EQ(uint64(1), flags.size());
  EXPECT_EQ(uint64(6), GetUInt64ValueForFlagWithDefault("foo", 9999, flags));

  group_number = 0;
  flags.clear();
  EXPECT_TRUE(GetFieldTrialInfo(
      "Group77 bar:1 baz:7 cat:dogs", &flags, &group_number));
  EXPECT_EQ(uint64(77), group_number);
  EXPECT_EQ(uint64(3), flags.size());
  EXPECT_EQ(true, GetBoolValueForFlagWithDefault("bar", false, flags));
  EXPECT_EQ(uint64(7), GetUInt64ValueForFlagWithDefault("baz", 0, flags));
  EXPECT_EQ("dogs", GetStringValueForFlagWithDefault("cat", "", flags));
  EXPECT_EQ("default", GetStringValueForFlagWithDefault(
      "moose", "default", flags));

  group_number = 0;
  flags.clear();
  EXPECT_FALSE(GetFieldTrialInfo(
      "Group77 bar:1 baz:7 cat:dogs DISABLED", &flags, &group_number));
  EXPECT_EQ(ZERO, group_number);
  EXPECT_EQ(ZERO, flags.size());
}

class SearchTest : public BrowserWithTestWindowTest {
 protected:
  virtual void SetUp() OVERRIDE {
    BrowserWithTestWindowTest::SetUp();
    TemplateURLServiceFactory::GetInstance()->SetTestingFactoryAndUse(
        profile(), &TemplateURLServiceFactory::BuildInstanceFor);
    TemplateURLService* template_url_service =
        TemplateURLServiceFactory::GetForProfile(profile());
    ui_test_utils::WaitForTemplateURLServiceToLoad(template_url_service);

    TemplateURLData data;
    data.SetURL("http://foo.com/url?bar={searchTerms}");
    data.instant_url = "http://foo.com/instant?"
        "{google:omniboxStartMarginParameter}foo=foo#foo=foo";
    data.alternate_urls.push_back("http://foo.com/alt#quux={searchTerms}");
    data.search_terms_replacement_key = "strk";

    TemplateURL* template_url = new TemplateURL(profile(), data);
    // Takes ownership of |template_url|.
    template_url_service->Add(template_url);
    template_url_service->SetDefaultSearchProvider(template_url);
  }
};

struct SearchTestCase {
  const char* url;
  bool expected_result;
  const char* comment;
};

TEST_F(SearchTest, ShouldAssignURLToInstantRendererExtendedDisabled) {
  const SearchTestCase kTestCases[] = {
    {"chrome-search://foo/bar",                 true,  ""},
    {"http://foo.com/instant",                  true,  ""},
    {"http://foo.com/instant?foo=bar",          true,  ""},
    {"https://foo.com/instant",                 true,  ""},
    {"https://foo.com/instant#foo=bar",         true,  ""},
    {"HtTpS://fOo.CoM/instant",                 true,  ""},
    {"http://foo.com:80/instant",               true,  ""},
    {"invalid URL",                             false, "Invalid URL"},
    {"unknown://scheme/path",                   false, "Unknown scheme"},
    {"ftp://foo.com/instant",                   false, "Non-HTTP scheme"},
    {"http://sub.foo.com/instant",              false, "Non-exact host"},
    {"http://foo.com:26/instant",               false, "Non-default port"},
    {"http://foo.com/instant/bar",              false, "Non-exact path"},
    {"http://foo.com/Instant",                  false, "Case sensitive path"},
    {"http://foo.com/",                         false, "Non-exact path"},
    {"https://foo.com/",                        false, "Non-exact path"},
    {"https://foo.com/url?strk",                false, "Non-extended mode"},
    {"https://foo.com/alt?strk",                false, "Non-extended mode"},
  };

  for (size_t i = 0; i < arraysize(kTestCases); ++i) {
    const SearchTestCase& test = kTestCases[i];
    EXPECT_EQ(test.expected_result,
              ShouldAssignURLToInstantRenderer(GURL(test.url), profile()))
        << test.url << " " << test.comment;
  }
}

TEST_F(SearchTest, ShouldAssignURLToInstantRendererExtendedEnabled) {
  EnableInstantExtendedAPIForTesting();

  const SearchTestCase kTestCases[] = {
    {chrome::kChromeSearchLocalOmniboxPopupURL, true,  ""},
    {chrome::kChromeSearchLocalNtpUrl, true,  ""},
    {"https://foo.com/instant?strk",   true,  ""},
    {"https://foo.com/instant#strk",   true,  ""},
    {"https://foo.com/instant?strk=0", true,  ""},
    {"https://foo.com/url?strk",       true,  ""},
    {"https://foo.com/alt?strk",       true,  ""},
    {"http://foo.com/instant",         false, "Non-HTTPS"},
    {"http://foo.com/instant?strk",    false, "Non-HTTPS"},
    {"http://foo.com/instant?strk=1",  false, "Non-HTTPS"},
    {"https://foo.com/instant",        false, "No search terms replacement"},
    {"https://foo.com/?strk",          false, "Non-exact path"},
  };

  for (size_t i = 0; i < arraysize(kTestCases); ++i) {
    const SearchTestCase& test = kTestCases[i];
    EXPECT_EQ(test.expected_result,
              ShouldAssignURLToInstantRenderer(GURL(test.url), profile()))
        << test.url << " " << test.comment;
  }
}

TEST_F(SearchTest, CoerceCommandLineURLToTemplateURL) {
  TemplateURL* template_url =
      TemplateURLServiceFactory::GetForProfile(profile())->
          GetDefaultSearchProvider();
  EXPECT_EQ(
      GURL("https://foo.com/instant?bar=bar#bar=bar"),
      CoerceCommandLineURLToTemplateURL(
          GURL("http://myserver.com:9000/dev?bar=bar#bar=bar"),
          template_url->instant_url_ref(), kDisableStartMargin));
}

const SearchTestCase kInstantNTPTestCases[] = {
  {"https://foo.com/instant?strk",     true,  "Valid Instant URL"},
  {"https://foo.com/instant#strk",     true,  "Valid Instant URL"},
  {"https://foo.com/url?strk",         true,  "Valid search URL"},
  {"https://foo.com/url#strk",         true,  "Valid search URL"},
  {"https://foo.com/alt?strk",         true,  "Valid alternative URL"},
  {"https://foo.com/alt#strk",         true,  "Valid alternative URL"},
  {"https://foo.com/url?strk&bar=",    true,  "No query terms"},
  {"https://foo.com/url?strk&q=abc",   true,  "No query terms key"},
  {"https://foo.com/url?strk#bar=abc", true,  "Query terms key in ref"},
  {"https://foo.com/url?strk&bar=abc", false, "Has query terms"},
  {"http://foo.com/instant?strk=1",    false, "Insecure URL"},
  {"https://foo.com/instant",          false, "No search terms replacement"},
  {"chrome://blank/",                  false, "Chrome scheme"},
  {"chrome-search//foo",               false, "Chrome-search scheme"},
  {chrome::kChromeSearchLocalOmniboxPopupURL, false, "Local omnibox popup"},
  {chrome::kChromeSearchLocalNtpUrl,   true,  "Local new tab page"},
  {"https://bar.com/instant?strk=1",   false, "Random non-search page"},
};

TEST_F(SearchTest, InstantNTPExtendedEnabled) {
  EnableInstantExtendedAPIForTesting();
  AddTab(browser(), GURL("chrome://blank"));
  for (size_t i = 0; i < arraysize(kInstantNTPTestCases); ++i) {
    const SearchTestCase& test = kInstantNTPTestCases[i];
    NavigateAndCommitActiveTab(GURL(test.url));
    const content::WebContents* contents =
        browser()->tab_strip_model()->GetWebContentsAt(0);
    EXPECT_EQ(test.expected_result, IsInstantNTP(contents))
        << test.url << " " << test.comment;
  }
}

TEST_F(SearchTest, InstantNTPExtendedDisabled) {
  AddTab(browser(), GURL("chrome://blank"));
  for (size_t i = 0; i < arraysize(kInstantNTPTestCases); ++i) {
    const SearchTestCase& test = kInstantNTPTestCases[i];
    NavigateAndCommitActiveTab(GURL(test.url));
    const content::WebContents* contents =
        browser()->tab_strip_model()->GetWebContentsAt(0);
    EXPECT_FALSE(IsInstantNTP(contents)) << test.url << " " << test.comment;
  }
}

TEST_F(SearchTest, InstantNTPCustomNavigationEntry) {
  EnableInstantExtendedAPIForTesting();
  AddTab(browser(), GURL("chrome://blank"));
  for (size_t i = 0; i < arraysize(kInstantNTPTestCases); ++i) {
    const SearchTestCase& test = kInstantNTPTestCases[i];
    NavigateAndCommitActiveTab(GURL(test.url));
    content::WebContents* contents =
        browser()->tab_strip_model()->GetWebContentsAt(0);
    content::NavigationController& controller = contents->GetController();
    controller.SetTransientEntry(
        controller.CreateNavigationEntry(GURL("chrome://blank"),
                                         content::Referrer(),
                                         content::PAGE_TRANSITION_LINK,
                                         false,
                                         std::string(),
                                         contents->GetBrowserContext()));
    // The active entry is chrome://blank and not an NTP.
    EXPECT_FALSE(IsInstantNTP(contents));
    EXPECT_EQ(test.expected_result,
              NavEntryIsInstantNTP(contents,
                                   controller.GetLastCommittedEntry()))
        << test.url << " " << test.comment;
  }
}

TEST_F(SearchTest, GetInstantURLExtendedDisabled) {
  // Instant is disabled, so no Instant URL.
  EXPECT_EQ(GURL(), GetInstantURL(profile(), kDisableStartMargin));

  // Enable Instant.
  profile()->GetPrefs()->SetBoolean(prefs::kInstantEnabled, true);
  EXPECT_EQ(GURL("http://foo.com/instant?foo=foo#foo=foo"),
            GetInstantURL(profile(), kDisableStartMargin));

  // Override the Instant URL on the commandline.
  CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kInstantURL,
      "http://myserver.com:9000/dev?bar=bar#bar=bar");
  EXPECT_EQ(GURL("http://myserver.com:9000/dev?bar=bar#bar=bar"),
            GetInstantURL(profile(), kDisableStartMargin));
}

TEST_F(SearchTest, GetInstantURLExtendedEnabled) {
  EnableInstantExtendedAPIForTesting();

  // Instant is disabled, so no Instant URL.
  EXPECT_EQ(GURL(), GetInstantURL(profile(), kDisableStartMargin));

  // Enable Instant. Still no Instant URL because "strk" is missing.
  profile()->GetPrefs()->SetBoolean(prefs::kInstantExtendedEnabled, true);
  EXPECT_EQ(GURL(), GetInstantURL(profile(), kDisableStartMargin));

  {
    // Set an Instant URL with a valid search terms replacement key.
    TemplateURLService* template_url_service =
        TemplateURLServiceFactory::GetForProfile(profile());

    TemplateURLData data;
    data.SetURL("http://foo.com/url?bar={searchTerms}");
    data.instant_url = "http://foo.com/instant?foo=foo#foo=foo&strk";
    data.search_terms_replacement_key = "strk";

    TemplateURL* template_url = new TemplateURL(profile(), data);
    // Takes ownership of |template_url|.
    template_url_service->Add(template_url);
    template_url_service->SetDefaultSearchProvider(template_url);
  }

  // Now there should be a valid Instant URL. Note the HTTPS "upgrade".
  EXPECT_EQ(GURL("https://foo.com/instant?foo=foo#foo=foo&strk"),
            GetInstantURL(profile(), kDisableStartMargin));

  // Enable suggest. No difference.
  profile()->GetPrefs()->SetBoolean(prefs::kSearchSuggestEnabled, true);
  EXPECT_EQ(GURL("https://foo.com/instant?foo=foo#foo=foo&strk"),
            GetInstantURL(profile(), kDisableStartMargin));

  // Disable Instant. No difference, because suggest is still enabled.
  profile()->GetPrefs()->SetBoolean(prefs::kInstantExtendedEnabled, false);
  EXPECT_EQ(GURL("https://foo.com/instant?foo=foo#foo=foo&strk"),
            GetInstantURL(profile(), kDisableStartMargin));

  // Override the Instant URL on the commandline. Oops, forgot "strk".
  CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kInstantURL,
      "http://myserver.com:9000/dev?bar=bar#bar=bar");
  EXPECT_EQ(GURL(), GetInstantURL(profile(), kDisableStartMargin));

  // Override with "strk". For fun, put it in the query, instead of the ref.
  CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kInstantURL,
      "http://myserver.com:9000/dev?bar=bar&strk#bar=bar");
  EXPECT_EQ(GURL("http://myserver.com:9000/dev?bar=bar&strk#bar=bar"),
            GetInstantURL(profile(), kDisableStartMargin));

  // Disable suggest. No Instant URL.
  profile()->GetPrefs()->SetBoolean(prefs::kSearchSuggestEnabled, false);
  EXPECT_EQ(GURL(), GetInstantURL(profile(), kDisableStartMargin));
}

TEST_F(SearchTest, StartMarginCGI) {
  // Instant is disabled, so no Instant URL.
  EXPECT_EQ(GURL(), GetInstantURL(profile(), kDisableStartMargin));

  // Enable Instant.  No margin.
  profile()->GetPrefs()->SetBoolean(prefs::kInstantEnabled, true);
  EXPECT_EQ(GURL("http://foo.com/instant?foo=foo#foo=foo"),
            GetInstantURL(profile(), kDisableStartMargin));

  // With start margin.
  EXPECT_EQ(GURL("http://foo.com/instant?es_sm=10&foo=foo#foo=foo"),
            GetInstantURL(profile(), 10));
}

}  // namespace chrome
