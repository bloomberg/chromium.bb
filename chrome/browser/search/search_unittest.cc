// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram_base.h"
#include "base/metrics/histogram_samples.h"
#include "base/metrics/statistics_recorder.h"
#include "base/prefs/pref_service.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/search_engines/search_terms_data.h"
#include "chrome/browser/search_engines/template_url_service.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/metrics/entropy_provider.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"

namespace chrome {

TEST(EmbeddedSearchFieldTrialTest, GetFieldTrialInfo) {
  FieldTrialFlags flags;
  uint64 group_number = 0;
  const uint64 ZERO = 0;

  EXPECT_FALSE(GetFieldTrialInfo(std::string(), &flags, &group_number));
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
  EXPECT_EQ("dogs",
            GetStringValueForFlagWithDefault("cat", std::string(), flags));
  EXPECT_EQ("default",
            GetStringValueForFlagWithDefault("moose", "default", flags));

  group_number = 0;
  flags.clear();
  EXPECT_FALSE(GetFieldTrialInfo(
      "Group77 bar:1 baz:7 cat:dogs DISABLED", &flags, &group_number));
  EXPECT_EQ(ZERO, group_number);
  EXPECT_EQ(ZERO, flags.size());
}

class InstantExtendedAPIEnabledTest : public testing::Test {
 public:
  InstantExtendedAPIEnabledTest() : histogram_(NULL) {
  }
 protected:
  virtual void SetUp() {
    field_trial_list_.reset(new base::FieldTrialList(
        new metrics::SHA1EntropyProvider("42")));
    base::StatisticsRecorder::Initialize();
    ResetInstantExtendedOptInStateGateForTest();
    previous_metrics_count_.resize(INSTANT_EXTENDED_OPT_IN_STATE_ENUM_COUNT, 0);
    base::HistogramBase* histogram = GetHistogram();
    if (histogram) {
      scoped_ptr<base::HistogramSamples> samples(histogram->SnapshotSamples());
      if (samples.get()) {
        for (int state = INSTANT_EXTENDED_NOT_SET;
             state < INSTANT_EXTENDED_OPT_IN_STATE_ENUM_COUNT; ++state) {
          previous_metrics_count_[state] = samples->GetCount(state);
        }
      }
    }
  }

  virtual CommandLine* GetCommandLine() const {
    return CommandLine::ForCurrentProcess();
  }

  void ValidateMetrics(base::HistogramBase::Sample value) {
    base::HistogramBase* histogram = GetHistogram();
    if (histogram) {
      scoped_ptr<base::HistogramSamples> samples(histogram->SnapshotSamples());
      if (samples.get()) {
        for (int state = INSTANT_EXTENDED_NOT_SET;
             state < INSTANT_EXTENDED_OPT_IN_STATE_ENUM_COUNT; ++state) {
          if (state == value) {
            EXPECT_EQ(previous_metrics_count_[state] + 1,
                      samples->GetCount(state));
          } else {
            EXPECT_EQ(previous_metrics_count_[state], samples->GetCount(state));
          }
        }
      }
    }
  }

 private:
  base::HistogramBase* GetHistogram() {
    if (!histogram_) {
      histogram_ = base::StatisticsRecorder::FindHistogram(
          "InstantExtended.OptInState");
    }
    return histogram_;
  }
  base::HistogramBase* histogram_;
  scoped_ptr<base::FieldTrialList> field_trial_list_;
  std::vector<int> previous_metrics_count_;
};

TEST_F(InstantExtendedAPIEnabledTest, EnabledViaCommandLineFlag) {
  GetCommandLine()->AppendSwitch(switches::kEnableInstantExtendedAPI);
  EXPECT_TRUE(IsInstantExtendedAPIEnabled());
#if defined(OS_IOS) || defined(OS_ANDROID)
  EXPECT_EQ(1ul, EmbeddedSearchPageVersion());
#else
  EXPECT_EQ(2ul, EmbeddedSearchPageVersion());
#endif
  ValidateMetrics(INSTANT_EXTENDED_OPT_IN);
}

TEST_F(InstantExtendedAPIEnabledTest, EnabledViaFinchFlag) {
  ASSERT_TRUE(base::FieldTrialList::CreateFieldTrial("InstantExtended",
                                                     "Group1 espv:42"));
  EXPECT_TRUE(IsInstantExtendedAPIEnabled());
  EXPECT_EQ(42ul, EmbeddedSearchPageVersion());
  ValidateMetrics(INSTANT_EXTENDED_NOT_SET);
}

TEST_F(InstantExtendedAPIEnabledTest, DisabledViaCommandLineFlag) {
  GetCommandLine()->AppendSwitch(switches::kDisableInstantExtendedAPI);
  ASSERT_TRUE(base::FieldTrialList::CreateFieldTrial("InstantExtended",
                                                     "Group1 espv:2"));
  EXPECT_FALSE(IsInstantExtendedAPIEnabled());
  EXPECT_EQ(0ul, EmbeddedSearchPageVersion());
  ValidateMetrics(INSTANT_EXTENDED_OPT_OUT);
}

class SearchTest : public BrowserWithTestWindowTest {
 protected:
  virtual void SetUp() OVERRIDE {
    BrowserWithTestWindowTest::SetUp();
    field_trial_list_.reset(new base::FieldTrialList(
        new metrics::SHA1EntropyProvider("42")));
    TemplateURLServiceFactory::GetInstance()->SetTestingFactoryAndUse(
        profile(), &TemplateURLServiceFactory::BuildInstanceFor);
    TemplateURLService* template_url_service =
        TemplateURLServiceFactory::GetForProfile(profile());
    ui_test_utils::WaitForTemplateURLServiceToLoad(template_url_service);
    SetSearchProvider();
  }

  void SetSearchProvider() {
    TemplateURLService* template_url_service =
        TemplateURLServiceFactory::GetForProfile(profile());
    TemplateURLData data;
    data.SetURL("http://foo.com/url?bar={searchTerms}");
    data.instant_url = "http://foo.com/instant?"
        "{google:omniboxStartMarginParameter}foo=foo#foo=foo&strk";
    data.alternate_urls.push_back("http://foo.com/alt#quux={searchTerms}");
    data.search_terms_replacement_key = "strk";

    TemplateURL* template_url = new TemplateURL(profile(), data);
    // Takes ownership of |template_url|.
    template_url_service->Add(template_url);
    template_url_service->SetDefaultSearchProvider(template_url);
  }

  // Build an Instant URL with or without a valid search terms replacement key
  // as per |has_search_term_replacement_key|. Set that URL as the instant URL
  // for the default search provider.
  void SetDefaultInstantTemplateUrl(bool has_search_term_replacement_key) {
    TemplateURLService* template_url_service =
        TemplateURLServiceFactory::GetForProfile(profile());

    static const char kInstantURLWithStrk[] =
        "http://foo.com/instant?foo=foo#foo=foo&strk";
    static const char kInstantURLNoStrk[] =
        "http://foo.com/instant?foo=foo#foo=foo";

    TemplateURLData data;
    data.SetURL("http://foo.com/url?bar={searchTerms}");
    data.instant_url = (has_search_term_replacement_key ?
        kInstantURLWithStrk : kInstantURLNoStrk);
    data.search_terms_replacement_key = "strk";

    TemplateURL* template_url = new TemplateURL(profile(), data);
    // Takes ownership of |template_url|.
    template_url_service->Add(template_url);
    template_url_service->SetDefaultSearchProvider(template_url);
  }

  scoped_ptr<base::FieldTrialList> field_trial_list_;
};

struct SearchTestCase {
  const char* url;
  bool expected_result;
  const char* comment;
};

TEST_F(SearchTest, ShouldAssignURLToInstantRendererExtendedDisabled) {
  DisableInstantExtendedAPIForTesting();

  const SearchTestCase kTestCases[] = {
    {"chrome-search://foo/bar",                 false,  ""},
    {"http://foo.com/instant",                  false,  ""},
    {"http://foo.com/instant?foo=bar",          false,  ""},
    {"https://foo.com/instant",                 false,  ""},
    {"https://foo.com/instant#foo=bar",         false,  ""},
    {"HtTpS://fOo.CoM/instant",                 false,  ""},
    {"http://foo.com:80/instant",               false,  ""},
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

TEST_F(SearchTest, ShouldUseProcessPerSiteForInstantURL) {
  EnableInstantExtendedAPIForTesting();

  const SearchTestCase kTestCases[] = {
    {"chrome-search://local-ntp",      true,  "Local NTP"},
    {"chrome-search://online-ntp",     true,  "Online NTP"},
    {"invalid-scheme://local-ntp",     false, "Invalid Local NTP URL"},
    {"invalid-scheme://online-ntp",    false, "Invalid Online NTP URL"},
    {"chrome-search://foo.com",        false, "Search result page"},
    {"https://foo.com/instant?strk",   false,  ""},
    {"https://foo.com/instant#strk",   false,  ""},
    {"https://foo.com/instant?strk=0", false,  ""},
    {"https://foo.com/url?strk",       false,  ""},
    {"https://foo.com/alt?strk",       false,  ""},
    {"http://foo.com/instant",         false,  "Non-HTTPS"},
    {"http://foo.com/instant?strk",    false,  "Non-HTTPS"},
    {"http://foo.com/instant?strk=1",  false,  "Non-HTTPS"},
    {"https://foo.com/instant",        false,  "No search terms replacement"},
    {"https://foo.com/?strk",          false,  "Non-exact path"},
  };

  for (size_t i = 0; i < arraysize(kTestCases); ++i) {
    const SearchTestCase& test = kTestCases[i];
    EXPECT_EQ(test.expected_result,
              ShouldUseProcessPerSiteForInstantURL(GURL(test.url), profile()))
        << test.url << " " << test.comment;
  }
}

struct PrivilegedURLTestCase {
  bool add_as_alternate_url;
  const char* input_url;
  const char* privileged_url;
  bool different_site_instance;
  const char* comment;
};

TEST_F(SearchTest, GetPrivilegedURLForInstant) {
  EnableInstantExtendedAPIForTesting();

  const PrivilegedURLTestCase kTestCases[] = {
    { false,  // don't append input_url as alternate URL.
      chrome::kChromeSearchLocalNtpUrl,  // input URL.
      chrome::kChromeSearchLocalNtpUrl,  // expected privileged URL.
      true,  // expected different SiteInstance.
      "local NTP" },
    { false,  // don't append input_url as alternate URL.
      "https://foo.com/instant?strk",
      "chrome-search://online-ntp/instant?strk",
      true,  // expected different SiteInstance.
      "Valid Instant NTP" },
    { false,  // don't append input_url as alternate URL.
      "https://foo.com/instant?strk&more=junk",
      "chrome-search://online-ntp/instant?strk&more=junk",
      true,  // expected different SiteInstance.
      "Valid Instant NTP with extra query" },
    { false,  // don't append input_url as alternate URL.
      "https://foo.com/url?strk",
      "chrome-search://foo.com/url?strk",
      false,  // expected same SiteInstance.
      "Search URL" },
    { true,  // append input_url as alternate URL.
      "https://notfoo.com/instant?strk",
      "chrome-search://notfoo.com/instant?strk",
      true,  // expected different SiteInstance.
      "Invalid host in URL" },
    { true,  // append input_url as alternate URL.
      "https://foo.com/webhp?strk",
      "chrome-search://foo.com/webhp?strk",
      false,  // expected same SiteInstance.
      "Invalid path in URL" },
    { true,  // append input_url as alternate URL.
      "https://foo.com/search?strk",
      "chrome-search://foo.com/search?strk",
      false,  // expected same SiteInstance.
      "Invalid path in URL" },
  };

  // GetPrivilegedURLForInstant expects ShouldAssignURLToInstantRenderer to
  // be true, and the latter expects chrome-search: scheme or IsInstantURL to be
  // true.  To force IsInstantURL to return true, add the input_url of each
  // PrivilegedURLTestCase as the alternate URL for the default template URL.
  const char* kSearchURL = "https://foo.com/url?strk";
  TemplateURLService* template_url_service =
      TemplateURLServiceFactory::GetForProfile(profile());
  TemplateURLData data;
  data.SetURL(kSearchURL);
  data.instant_url = "http://foo.com/instant?strk";
  data.search_terms_replacement_key = "strk";
  for (size_t i = 0; i < arraysize(kTestCases); ++i) {
    const PrivilegedURLTestCase& test = kTestCases[i];
    if (test.add_as_alternate_url)
      data.alternate_urls.push_back(test.input_url);
  }
  TemplateURL* template_url = new TemplateURL(profile(), data);
  // Takes ownership of |template_url|.
  template_url_service->Add(template_url);
  template_url_service->SetDefaultSearchProvider(template_url);

  AddTab(browser(), GURL("chrome://blank"));
  for (size_t i = 0; i < arraysize(kTestCases); ++i) {
    const PrivilegedURLTestCase& test = kTestCases[i];

    // Verify GetPrivilegedURLForInstant.
    EXPECT_EQ(GURL(test.privileged_url),
              GetPrivilegedURLForInstant(GURL(test.input_url), profile()))
        << test.input_url << " " << test.comment;

    // Verify that navigating from input_url to search URL results in same or
    // different SiteInstance.
    // First, navigate to input_url.
    NavigateAndCommitActiveTab(GURL(test.input_url));
    content::WebContents* contents =
        browser()->tab_strip_model()->GetWebContentsAt(0);
    // Cache the SiteInstance, RenderViewHost and RenderProcessHost for
    // input_url.
    const scoped_refptr<content::SiteInstance> prev_site_instance =
        contents->GetSiteInstance();
    const content::RenderViewHost* prev_rvh = contents->GetRenderViewHost();
    const content::RenderProcessHost* prev_rph =
        contents->GetRenderProcessHost();
    // Then, navigate to search URL.
    NavigateAndCommitActiveTab(GURL(kSearchURL));
    EXPECT_EQ(test.different_site_instance,
              contents->GetSiteInstance() != prev_site_instance)
        << test.input_url << " " << test.comment;
    EXPECT_EQ(test.different_site_instance,
              contents->GetRenderViewHost() != prev_rvh)
        << test.input_url << " " << test.comment;
    EXPECT_EQ(test.different_site_instance,
              contents->GetRenderProcessHost() != prev_rph)
        << test.input_url << " " << test.comment;
  }
}

const SearchTestCase kInstantNTPTestCases[] = {
  {"https://foo.com/instant?strk",         true,  "Valid Instant URL"},
  {"https://foo.com/instant#strk",         true,  "Valid Instant URL"},
  {"https://foo.com/url?strk",             true,  "Valid search URL"},
  {"https://foo.com/url#strk",             true,  "Valid search URL"},
  {"https://foo.com/alt?strk",             true,  "Valid alternative URL"},
  {"https://foo.com/alt#strk",             true,  "Valid alternative URL"},
  {"https://foo.com/url?strk&bar=",        true,  "No query terms"},
  {"https://foo.com/url?strk&q=abc",       true,  "No query terms key"},
  {"https://foo.com/url?strk#bar=abc",     true,  "Query terms key in ref"},
  {"https://foo.com/url?strk&bar=abc",     false, "Has query terms"},
  {"http://foo.com/instant?strk=1",        false, "Insecure URL"},
  {"https://foo.com/instant",              false, "No search term replacement"},
  {"chrome://blank/",                      false, "Chrome scheme"},
  {"chrome-search://foo",                  false, "Chrome-search scheme"},
  {chrome::kChromeSearchLocalNtpUrl,       true,  "Local new tab page"},
  {"https://bar.com/instant?strk=1",       false, "Random non-search page"},
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

TEST_F(SearchTest, GetInstantURLExtendedEnabled) {
  // Instant is disabled, so no Instant URL.
  EXPECT_EQ(GURL(), GetInstantURL(profile(), kDisableStartMargin));

  // Enable Instant. Still no Instant URL because "strk" is missing.
  EnableInstantExtendedAPIForTesting();
  SetDefaultInstantTemplateUrl(false);
  EXPECT_EQ(GURL(), GetInstantURL(profile(), kDisableStartMargin));

  // Set an Instant URL with a valid search terms replacement key.
  SetDefaultInstantTemplateUrl(true);

  // Now there should be a valid Instant URL. Note the HTTPS "upgrade".
  EXPECT_EQ(GURL("https://foo.com/instant?foo=foo#foo=foo&strk"),
            GetInstantURL(profile(), kDisableStartMargin));

  // Enable suggest. No difference.
  profile()->GetPrefs()->SetBoolean(prefs::kSearchSuggestEnabled, true);
  EXPECT_EQ(GURL("https://foo.com/instant?foo=foo#foo=foo&strk"),
            GetInstantURL(profile(), kDisableStartMargin));

  // Disable suggest. No Instant URL.
  profile()->GetPrefs()->SetBoolean(prefs::kSearchSuggestEnabled, false);
  EXPECT_EQ(GURL(), GetInstantURL(profile(), kDisableStartMargin));
}

TEST_F(SearchTest, StartMarginCGI) {
  // Instant is disabled, so no Instant URL.
  EXPECT_EQ(GURL(), GetInstantURL(profile(), kDisableStartMargin));

  // Enable Instant. No margin.
  EnableInstantExtendedAPIForTesting();
  profile()->GetPrefs()->SetBoolean(prefs::kSearchSuggestEnabled, true);

  EXPECT_EQ(GURL("https://foo.com/instant?foo=foo#foo=foo&strk"),
            GetInstantURL(profile(), kDisableStartMargin));

  // With start margin.
  EXPECT_EQ(GURL("https://foo.com/instant?es_sm=10&foo=foo#foo=foo&strk"),
            GetInstantURL(profile(), 10));
}

TEST_F(SearchTest, CommandLineOverrides) {
  EnableInstantExtendedAPIForTesting();

  GURL local_instant_url(GetLocalInstantURL(profile()));
  EXPECT_EQ(GURL(chrome::kChromeSearchLocalNtpUrl), local_instant_url);

  TemplateURLService* template_url_service =
      TemplateURLServiceFactory::GetForProfile(profile());
  TemplateURLData data;
  data.SetURL("{google:baseURL}search?q={searchTerms}");
  data.instant_url = "{google:baseURL}webhp?strk";
  data.search_terms_replacement_key = "strk";
  TemplateURL* template_url = new TemplateURL(profile(), data);
  // Takes ownership of |template_url|.
  template_url_service->Add(template_url);
  template_url_service->SetDefaultSearchProvider(template_url);

  // By default, Instant Extended forces the instant URL to be HTTPS, so even if
  // we set a Google base URL that is HTTP, we should get an HTTPS URL.
  UIThreadSearchTermsData::SetGoogleBaseURL("http://www.foo.com/");
  GURL instant_url(GetInstantURL(profile(), kDisableStartMargin));
  ASSERT_TRUE(instant_url.is_valid());
  EXPECT_EQ("https://www.foo.com/webhp?strk", instant_url.spec());

  // However, if the Google base URL is specified on the command line, the
  // instant URL should just use it, even if it's HTTP.
  UIThreadSearchTermsData::SetGoogleBaseURL(std::string());
  CommandLine::ForCurrentProcess()->AppendSwitchASCII(switches::kGoogleBaseURL,
                                                      "http://www.bar.com/");
  instant_url = GetInstantURL(profile(), kDisableStartMargin);
  ASSERT_TRUE(instant_url.is_valid());
  EXPECT_EQ("http://www.bar.com/webhp?strk", instant_url.spec());

  // Similarly, setting a Google base URL on the command line should allow you
  // to get the Google version of the local NTP, even though search provider's
  // URL doesn't contain "google".
  local_instant_url = GetLocalInstantURL(profile());
  EXPECT_EQ(GURL(chrome::kChromeSearchLocalNtpUrl), local_instant_url);

  // If we specify extra search query params, they should be inserted into the
  // query portion of the instant URL.
  CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kExtraSearchQueryParams, "a=b");
  instant_url = GetInstantURL(profile(), kDisableStartMargin);
  ASSERT_TRUE(instant_url.is_valid());
  EXPECT_EQ("http://www.bar.com/webhp?a=b&strk", instant_url.spec());
}

TEST_F(SearchTest, ShouldShowInstantNTP_Default) {
  EnableInstantExtendedAPIForTesting();
  EXPECT_TRUE(ShouldShowInstantNTP());
}

TEST_F(SearchTest, ShouldShowInstantNTP_DisabledViaFinch) {
  EnableInstantExtendedAPIForTesting();
  ASSERT_TRUE(base::FieldTrialList::CreateFieldTrial("InstantExtended",
                                                     "Group1 show_ntp:0"));
  EXPECT_FALSE(ShouldShowInstantNTP());
}

TEST_F(SearchTest, ShouldShowInstantNTP_DisabledByInstantNewTabURLSwitch) {
  EnableInstantExtendedAPIForTesting();
  CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kInstantNewTabURL, "http://example.com/newtab");
  EXPECT_FALSE(ShouldShowInstantNTP());
}

}  // namespace chrome
