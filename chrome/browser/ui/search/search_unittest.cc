// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url.h"
#include "chrome/browser/search_engines/template_url_service.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/search/search.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chrome {
namespace search {

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

TEST(SearchTest, ShouldAssignURLToInstantRendererImpl) {
  TemplateURLData data;
  data.SetURL("http://foo.com/url?bar={searchTerms}");
  data.instant_url = "http://foo.com/instant";
  data.alternate_urls.push_back("http://foo.com/alt#quux={searchTerms}");
  data.search_terms_replacement_key = "strk";
  TemplateURL url(NULL, data);

  struct {
    const char* const url;
    bool extended_api_enabled;
    bool expected_result;
  } kTestCases[] = {
    {"invalid URL",                     false, false},
    {"unknown://scheme/path",           false, false},
    {"chrome-search://foo/bar",         false,  true},
    {kLocalOmniboxPopupURL,             false, false},
    {kLocalOmniboxPopupURL,              true,  true},
    {"http://foo.com/instant",          false,  true},
    {"http://foo.com/instant?foo=bar",  false,  true},
    {"https://foo.com/instant",         false,  true},
    {"https://foo.com/instant#foo=bar", false,  true},
    {"HtTpS://fOo.CoM/instant",         false,  true},
    {"http://foo.com:80/instant",       false,  true},
    {"ftp://foo.com/instant",           false, false},
    {"http://sub.foo.com/instant",      false, false},
    {"http://foo.com:26/instant",       false, false},
    {"http://foo.com/instant/bar",      false, false},
    {"http://foo.com/Instant",          false, false},
    {"https://foo.com/instant?strk",     true,  true},
    {"https://foo.com/instant#strk",     true,  true},
    {"https://foo.com/instant?strk=0",   true,  true},
    {"http://foo.com/instant",           true, false},
    {"https://foo.com/instant",          true, false},
    {"http://foo.com/instant?strk=1",    true, false},
    {"https://foo.com/url?strk",         true,  true},
    {"https://foo.com/alt?strk",         true,  true},
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kTestCases); ++i) {
    EXPECT_EQ(kTestCases[i].expected_result,
              ShouldAssignURLToInstantRendererImpl(
                  GURL(kTestCases[i].url),
                  kTestCases[i].extended_api_enabled,
                  &url)) << kTestCases[i].url << " "
                         << kTestCases[i].extended_api_enabled;
  }
}

TEST(SearchTest, CoerceCommandLineURLToTemplateURL) {
  TemplateURLData data;
  data.SetURL("http://foo.com/url?bar={searchTerms}");
  data.instant_url = "http://foo.com/instant?foo=foo#foo=foo";
  data.alternate_urls.push_back("http://foo.com/alt#quux={searchTerms}");
  data.search_terms_replacement_key = "strk";
  TemplateURL url(NULL, data);

  EXPECT_EQ(
      GURL("https://foo.com/instant?bar=bar#bar=bar"),
      CoerceCommandLineURLToTemplateURL(
          GURL("http://myserver.com:9000/dev?bar=bar#bar=bar"),
          url.instant_url_ref()));
}

class InstantNTPTest : public BrowserWithTestWindowTest {
 protected:
  virtual void SetUp() OVERRIDE {
    BrowserWithTestWindowTest::SetUp();
    TemplateURLServiceFactory::GetInstance()->SetTestingFactoryAndUse(
        profile(), &TemplateURLServiceFactory::BuildInstanceFor);
    TemplateURLService* template_url_service =
        TemplateURLServiceFactory::GetForProfile(profile());
    ui_test_utils::WaitForTemplateURLServiceToLoad(template_url_service);

    TemplateURLData data;
    data.SetURL("http://foo.com/#query={searchTerms}");
    data.instant_url = "http://foo.com/instant";
    data.search_terms_replacement_key = "strk";

    TemplateURL* template_url = new TemplateURL(profile(), data);
    template_url_service->Add(template_url);
    template_url_service->SetDefaultSearchProvider(template_url);
  }
};

static const struct {
  const char* const url;
  bool expected_result;
  const char* const description;
} kInstantNTPTestCases[] = {
  {"https://foo.com/instant?strk=1",    true,  "Valid Instant URL"},
  {"https://foo.com/?strk=1",           true,  "Valid search URL"},
  {"https://foo.com/instant",           false, "No search terms replacement"},
  {"https://foo.com/?strk=1#query=abc", false, "Has query terms"},
  {"http://foo.com/instant?strk=1",     false, "Insecure URL"},
  {"chrome://blank/",                   false, "chrome schema"},
  {"chrome-search//foo",                false, "chrome-search schema"},
  {"https://bar.com/instant?strk=1",    false, "Random non-search page"},
};

TEST_F(InstantNTPTest, InstantExtendedEnabled) {
  chrome::search::EnableInstantExtendedAPIForTesting();
  AddTab(browser(), GURL("chrome://blank"));
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kInstantNTPTestCases); ++i) {
    NavigateAndCommitActiveTab(GURL(kInstantNTPTestCases[i].url));
    const content::WebContents* contents =
        browser()->tab_strip_model()->GetWebContentsAt(0);
    EXPECT_EQ(kInstantNTPTestCases[i].expected_result, IsInstantNTP(contents))
        << kInstantNTPTestCases[i].description << ": "
        << kInstantNTPTestCases[i].url;
  }
}

TEST_F(InstantNTPTest, InstantExtendedDisabled) {
  AddTab(browser(), GURL("chrome://blank"));
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kInstantNTPTestCases); ++i) {
    NavigateAndCommitActiveTab(GURL(kInstantNTPTestCases[i].url));
    const content::WebContents* contents =
        browser()->tab_strip_model()->GetWebContentsAt(0);
    EXPECT_FALSE(IsInstantNTP(contents))
        << kInstantNTPTestCases[i].description << ": "
        << kInstantNTPTestCases[i].url;
  }
}

TEST_F(InstantNTPTest, CustomNavigationEntry) {
  chrome::search::EnableInstantExtendedAPIForTesting();
  AddTab(browser(), GURL("chrome://blank"));
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kInstantNTPTestCases); ++i) {
    NavigateAndCommitActiveTab(GURL(kInstantNTPTestCases[i].url));
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
    ASSERT_FALSE(IsInstantNTP(contents));
    EXPECT_EQ(kInstantNTPTestCases[i].expected_result,
              NavEntryIsInstantNTP(contents,
                                   controller.GetLastCommittedEntry()))
        << kInstantNTPTestCases[i].description << ": "
        << kInstantNTPTestCases[i].url;
  }
}

}  // namespace search
}  // namespace chrome
