// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram_base.h"
#include "base/metrics/histogram_samples.h"
#include "base/metrics/statistics_recorder.h"
#include "base/prefs/pref_service.h"
#include "chrome/browser/managed_mode/managed_mode_url_filter.h"
#include "chrome/browser/managed_mode/managed_user_service.h"
#include "chrome/browser/managed_mode/managed_user_service_factory.h"
#include "chrome/browser/search/instant_service.h"
#include "chrome/browser/search/instant_service_factory.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/search_engines/search_terms_data.h"
#include "chrome/browser/search_engines/template_url_service.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/variations/entropy_provider.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/renderer_preferences.h"
#include "url/gurl.h"

namespace chrome {

class EmbeddedSearchFieldTrialTest : public testing::Test {
 protected:
  virtual void SetUp() {
    field_trial_list_.reset(new base::FieldTrialList(
        new metrics::SHA1EntropyProvider("42")));
    base::StatisticsRecorder::Initialize();
    ResetInstantExtendedOptInStateGateForTest();
  }

 private:
  scoped_ptr<base::FieldTrialList> field_trial_list_;
};

TEST_F(EmbeddedSearchFieldTrialTest, GetFieldTrialInfoEmptyAndValid) {
  FieldTrialFlags flags;
  uint64 group_number = 0;

  EXPECT_TRUE(GetFieldTrialInfo(&flags, &group_number));
  EXPECT_EQ(0ul, group_number);
  EXPECT_EQ(0ul, flags.size());

  ASSERT_TRUE(base::FieldTrialList::CreateFieldTrial(
      "InstantExtended", "Group77"));
  EXPECT_TRUE(GetFieldTrialInfo(&flags, &group_number));
  EXPECT_EQ(77ul, group_number);
  EXPECT_EQ(0ul, flags.size());
}

TEST_F(EmbeddedSearchFieldTrialTest, GetFieldTrialInfoInvalidNumber) {
  FieldTrialFlags flags;
  uint64 group_number = 0;

  ASSERT_TRUE(base::FieldTrialList::CreateFieldTrial(
      "InstantExtended", "Group77.2"));
  EXPECT_FALSE(GetFieldTrialInfo(&flags, &group_number));
  EXPECT_EQ(0ul, group_number);
  EXPECT_EQ(0ul, flags.size());
}

TEST_F(EmbeddedSearchFieldTrialTest, GetFieldTrialInfoInvalidName) {
  FieldTrialFlags flags;
  uint64 group_number = 0;

  ASSERT_TRUE(base::FieldTrialList::CreateFieldTrial(
      "InstantExtended", "Invalid77"));
  EXPECT_TRUE(GetFieldTrialInfo(&flags, &group_number));
  EXPECT_EQ(0ul, group_number);
  EXPECT_EQ(0ul, flags.size());
}

TEST_F(EmbeddedSearchFieldTrialTest, GetFieldTrialInfoValidGroup) {
  FieldTrialFlags flags;
  uint64 group_number = 0;

  ASSERT_TRUE(base::FieldTrialList::CreateFieldTrial(
      "InstantExtended", "Group77"));
  EXPECT_TRUE(GetFieldTrialInfo(&flags, &group_number));
  EXPECT_EQ(77ul, group_number);
  EXPECT_EQ(0ul, flags.size());
}

TEST_F(EmbeddedSearchFieldTrialTest, GetFieldTrialInfoValidFlag) {
  FieldTrialFlags flags;
  uint64 group_number = 0;

  EXPECT_EQ(9999ul, GetUInt64ValueForFlagWithDefault("foo", 9999, flags));
  ASSERT_TRUE(base::FieldTrialList::CreateFieldTrial(
      "InstantExtended", "Group77 foo:6"));
  EXPECT_TRUE(GetFieldTrialInfo(&flags, &group_number));
  EXPECT_EQ(77ul, group_number);
  EXPECT_EQ(1ul, flags.size());
  EXPECT_EQ(6ul, GetUInt64ValueForFlagWithDefault("foo", 9999, flags));
}

TEST_F(EmbeddedSearchFieldTrialTest, GetFieldTrialInfoNewName) {
  FieldTrialFlags flags;
  uint64 group_number = 0;

  EXPECT_EQ(9999ul, GetUInt64ValueForFlagWithDefault("foo", 9999, flags));
  ASSERT_TRUE(base::FieldTrialList::CreateFieldTrial(
      "EmbeddedSearch", "Group77 foo:6"));
  EXPECT_TRUE(GetFieldTrialInfo(&flags, &group_number));
  EXPECT_EQ(77ul, group_number);
  EXPECT_EQ(1ul, flags.size());
  EXPECT_EQ(6ul, GetUInt64ValueForFlagWithDefault("foo", 9999, flags));
}

TEST_F(EmbeddedSearchFieldTrialTest, GetFieldTrialInfoNewNameOverridesOld) {
  FieldTrialFlags flags;
  uint64 group_number = 0;

  EXPECT_EQ(9999ul, GetUInt64ValueForFlagWithDefault("foo", 9999, flags));
  ASSERT_TRUE(base::FieldTrialList::CreateFieldTrial(
      "EmbeddedSearch", "Group77 foo:6"));
  ASSERT_TRUE(base::FieldTrialList::CreateFieldTrial(
      "InstantExtended", "Group78 foo:5"));
  EXPECT_TRUE(GetFieldTrialInfo(&flags, &group_number));
  EXPECT_EQ(77ul, group_number);
  EXPECT_EQ(1ul, flags.size());
  EXPECT_EQ(6ul, GetUInt64ValueForFlagWithDefault("foo", 9999, flags));
}

TEST_F(EmbeddedSearchFieldTrialTest, GetFieldTrialInfoLotsOfFlags) {
  FieldTrialFlags flags;
  uint64 group_number = 0;

  ASSERT_TRUE(base::FieldTrialList::CreateFieldTrial(
      "InstantExtended", "Group77 bar:1 baz:7 cat:dogs"));
  EXPECT_TRUE(GetFieldTrialInfo(&flags, &group_number));
  EXPECT_EQ(77ul, group_number);
  EXPECT_EQ(3ul, flags.size());
  EXPECT_EQ(true, GetBoolValueForFlagWithDefault("bar", false, flags));
  EXPECT_EQ(7ul, GetUInt64ValueForFlagWithDefault("baz", 0, flags));
  EXPECT_EQ("dogs",
            GetStringValueForFlagWithDefault("cat", std::string(), flags));
  EXPECT_EQ("default",
            GetStringValueForFlagWithDefault("moose", "default", flags));
}

TEST_F(EmbeddedSearchFieldTrialTest, GetFieldTrialInfoDisabled) {
  FieldTrialFlags flags;
  uint64 group_number = 0;

  ASSERT_TRUE(base::FieldTrialList::CreateFieldTrial(
      "InstantExtended", "Group77 bar:1 baz:7 cat:dogs DISABLED"));
  EXPECT_FALSE(GetFieldTrialInfo(&flags, &group_number));
  EXPECT_EQ(0ul, group_number);
  EXPECT_EQ(0ul, flags.size());
}

TEST_F(EmbeddedSearchFieldTrialTest, GetFieldTrialInfoControlFlags) {
  FieldTrialFlags flags;
  uint64 group_number = 0;

  ASSERT_TRUE(base::FieldTrialList::CreateFieldTrial(
      "InstantExtended", "Control77 bar:1 baz:7 cat:dogs"));
  EXPECT_TRUE(GetFieldTrialInfo(&flags, &group_number));
  EXPECT_EQ(0ul, group_number);
  EXPECT_EQ(3ul, flags.size());
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

typedef InstantExtendedAPIEnabledTest ShouldHideTopVerbatimTest;

TEST_F(ShouldHideTopVerbatimTest, DoNotHideByDefault) {
  ASSERT_TRUE(base::FieldTrialList::CreateFieldTrial(
      "InstantExtended", "Control"));
  EXPECT_FALSE(ShouldHideTopVerbatimMatch());
}

TEST_F(ShouldHideTopVerbatimTest, DoNotHideInInstantExtended) {
  ASSERT_TRUE(base::FieldTrialList::CreateFieldTrial(
      "InstantExtended", "Group1"));
  EXPECT_FALSE(ShouldHideTopVerbatimMatch());
}

TEST_F(ShouldHideTopVerbatimTest, EnableByFlagInInstantExtended) {
  ASSERT_TRUE(base::FieldTrialList::CreateFieldTrial(
      "InstantExtended", "Group1 hide_verbatim:1"));
  EXPECT_TRUE(ShouldHideTopVerbatimMatch());
}

TEST_F(ShouldHideTopVerbatimTest, EnableByFlagOutsideInstantExtended) {
  ASSERT_TRUE(base::FieldTrialList::CreateFieldTrial(
      "InstantExtended", "Controll1 hide_verbatim:1"));
  EXPECT_TRUE(ShouldHideTopVerbatimMatch());
}

TEST_F(ShouldHideTopVerbatimTest, DisableByFlag) {
  ASSERT_TRUE(base::FieldTrialList::CreateFieldTrial(
      "InstantExtended", "Group1 hide_verbatim:0"));
  EXPECT_FALSE(ShouldHideTopVerbatimMatch());
}

typedef InstantExtendedAPIEnabledTest ShouldSuppressInstantExtendedOnSRPTest;

TEST_F(ShouldSuppressInstantExtendedOnSRPTest, NotSet) {
  ASSERT_TRUE(base::FieldTrialList::CreateFieldTrial(
      "InstantExtended", "Group1 espv:2"));
  EXPECT_FALSE(ShouldSuppressInstantExtendedOnSRP());
  EXPECT_TRUE(IsInstantExtendedAPIEnabled());
  EXPECT_TRUE(IsQueryExtractionEnabled());
  EXPECT_EQ(2ul, EmbeddedSearchPageVersion());
}

TEST_F(ShouldSuppressInstantExtendedOnSRPTest, NotSuppressOnSRP) {
  ASSERT_TRUE(base::FieldTrialList::CreateFieldTrial(
      "InstantExtended", "Group1 espv:2 suppress_on_srp:0"));
  EXPECT_FALSE(ShouldSuppressInstantExtendedOnSRP());
  EXPECT_TRUE(IsInstantExtendedAPIEnabled());
  EXPECT_TRUE(IsQueryExtractionEnabled());
  EXPECT_EQ(2ul, EmbeddedSearchPageVersion());
}

TEST_F(ShouldSuppressInstantExtendedOnSRPTest, SuppressOnSRP) {
  ASSERT_TRUE(base::FieldTrialList::CreateFieldTrial(
      "InstantExtended", "Group1 espv:2 suppress_on_srp:1"));
  EXPECT_TRUE(ShouldSuppressInstantExtendedOnSRP());
  EXPECT_TRUE(IsInstantExtendedAPIEnabled());
  EXPECT_FALSE(IsQueryExtractionEnabled());
  EXPECT_EQ(2ul, EmbeddedSearchPageVersion());
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
    SetSearchProvider(true, false);
  }

  void SetSearchProvider(bool set_ntp_url, bool insecure_ntp_url) {
    TemplateURLService* template_url_service =
        TemplateURLServiceFactory::GetForProfile(profile());
    TemplateURLData data;
    data.SetURL("http://foo.com/url?bar={searchTerms}");
    data.instant_url = "http://foo.com/instant?"
        "{google:omniboxStartMarginParameter}{google:forceInstantResults}"
        "foo=foo#foo=foo&strk";
    if (set_ntp_url) {
      data.new_tab_url = (insecure_ntp_url ? "http" : "https") +
          std::string("://foo.com/newtab?strk");
    }
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

  bool InInstantProcess(const content::WebContents* contents) {
    InstantService* instant_service =
        InstantServiceFactory::GetForProfile(profile());
    return instant_service->IsInstantProcess(
        contents->GetRenderProcessHost()->GetID());
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

TEST_F(SearchTest, ShouldAssignURLToInstantRendererExtendedEnabledNotOnSRP) {
  ASSERT_TRUE(base::FieldTrialList::CreateFieldTrial(
      "InstantExtended", "Group1 espv:2 suppress_on_srp:1"));

  const SearchTestCase kTestCases[] = {
    {chrome::kChromeSearchLocalNtpUrl, true,  ""},
    {"https://foo.com/instant?strk",   true,  ""},
    {"https://foo.com/instant#strk",   true,  ""},
    {"https://foo.com/instant?strk=0", true,  ""},
    {"https://foo.com/url?strk",       false, "Disabled on SRP"},
    {"https://foo.com/alt?strk",       false, "Disabled ON SRP"},
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

// Each test case represents a navigation to |start_url| followed by a
// navigation to |end_url|. We will check whether each navigation lands in an
// Instant process, and also whether the navigation from start to end re-uses
// the same SiteInstance (and hence the same RenderViewHost, etc.).
const struct ProcessIsolationTestCase {
  const char* description;
  const char* start_url;
  bool start_in_instant_process;
  const char* end_url;
  bool end_in_instant_process;
  bool same_site_instance;
} kProcessIsolationTestCases[] = {
  {"Local NTP -> SRP",
   "chrome-search://local-ntp",       true,
   "https://foo.com/url?strk",        true,   false },
  {"Local NTP -> Regular",
   "chrome-search://local-ntp",       true,
   "https://foo.com/other",           false,  false },
  {"Remote NTP -> SRP",
   "https://foo.com/instant?strk",    true,
   "https://foo.com/url?strk",        true,   false },
  {"Remote NTP -> Regular",
   "https://foo.com/instant?strk",    true,
   "https://foo.com/other",           false,  false },
  {"SRP -> SRP",
   "https://foo.com/url?strk",        true,
   "https://foo.com/url?strk",        true,   true  },
  {"SRP -> Regular",
   "https://foo.com/url?strk",        true,
   "https://foo.com/other",           false,  false },
  {"Regular -> SRP",
   "https://foo.com/other",           false,
   "https://foo.com/url?strk",        true,   false },
};

TEST_F(SearchTest, ProcessIsolation) {
  EnableInstantExtendedAPIForTesting();

  for (size_t i = 0; i < arraysize(kProcessIsolationTestCases); ++i) {
    const ProcessIsolationTestCase& test = kProcessIsolationTestCases[i];
    AddTab(browser(), GURL("chrome://blank"));
    const content::WebContents* contents =
        browser()->tab_strip_model()->GetActiveWebContents();

    // Navigate to start URL.
    NavigateAndCommitActiveTab(GURL(test.start_url));
    EXPECT_EQ(test.start_in_instant_process, InInstantProcess(contents))
        << test.description;

    // Save state.
    const scoped_refptr<content::SiteInstance> start_site_instance =
        contents->GetSiteInstance();
    const content::RenderProcessHost* start_rph =
        contents->GetRenderProcessHost();
    const content::RenderViewHost* start_rvh =
        contents->GetRenderViewHost();

    // Navigate to end URL.
    NavigateAndCommitActiveTab(GURL(test.end_url));
    EXPECT_EQ(test.end_in_instant_process, InInstantProcess(contents))
        << test.description;

    EXPECT_EQ(test.same_site_instance,
              start_site_instance == contents->GetSiteInstance())
        << test.description;
    EXPECT_EQ(test.same_site_instance,
              start_rvh == contents->GetRenderViewHost())
        << test.description;
    EXPECT_EQ(test.same_site_instance,
              start_rph == contents->GetRenderProcessHost())
        << test.description;
  }
}

TEST_F(SearchTest, ProcessIsolation_RendererInitiated) {
  EnableInstantExtendedAPIForTesting();

  for (size_t i = 0; i < arraysize(kProcessIsolationTestCases); ++i) {
    const ProcessIsolationTestCase& test = kProcessIsolationTestCases[i];
    AddTab(browser(), GURL("chrome://blank"));
    content::WebContents* contents =
        browser()->tab_strip_model()->GetActiveWebContents();

    // Navigate to start URL.
    NavigateAndCommitActiveTab(GURL(test.start_url));
    EXPECT_EQ(test.start_in_instant_process, InInstantProcess(contents))
        << test.description;

    // Save state.
    const scoped_refptr<content::SiteInstance> start_site_instance =
        contents->GetSiteInstance();
    const content::RenderProcessHost* start_rph =
        contents->GetRenderProcessHost();
    const content::RenderViewHost* start_rvh =
        contents->GetRenderViewHost();

    // Navigate to end URL via a renderer-initiated navigation.
    content::NavigationController* controller = &contents->GetController();
    content::NavigationController::LoadURLParams load_params(
        GURL(test.end_url));
    load_params.is_renderer_initiated = true;
    load_params.transition_type = content::PAGE_TRANSITION_LINK;

    controller->LoadURLWithParams(load_params);
    CommitPendingLoad(controller);
    EXPECT_EQ(test.end_in_instant_process, InInstantProcess(contents))
        << test.description;

    EXPECT_EQ(test.same_site_instance,
              start_site_instance == contents->GetSiteInstance())
        << test.description;
    EXPECT_EQ(test.same_site_instance,
              start_rvh == contents->GetRenderViewHost())
        << test.description;
    EXPECT_EQ(test.same_site_instance,
              start_rph == contents->GetRenderProcessHost())
        << test.description;
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

TEST_F(SearchTest, InstantCacheableNTPNavigationEntry) {
  EnableInstantExtendedAPIForTesting();
  ASSERT_TRUE(base::FieldTrialList::CreateFieldTrial("InstantExtended",
      "Group1 use_cacheable_ntp:1"));

  AddTab(browser(), GURL("chrome://blank"));
  content::WebContents* contents =
        browser()->tab_strip_model()->GetWebContentsAt(0);
  content::NavigationController& controller = contents->GetController();
  // Local NTP.
  NavigateAndCommitActiveTab(GURL(chrome::kChromeSearchLocalNtpUrl));
  EXPECT_TRUE(NavEntryIsInstantNTP(contents,
                                   controller.GetLastCommittedEntry()));
  // Instant page is not cacheable NTP.
  NavigateAndCommitActiveTab(GetInstantURL(profile(), kDisableStartMargin,
                                           false));
  EXPECT_FALSE(NavEntryIsInstantNTP(contents,
                                    controller.GetLastCommittedEntry()));
  // Test Cacheable NTP
  NavigateAndCommitActiveTab(chrome::GetNewTabPageURL(profile()));
  EXPECT_TRUE(NavEntryIsInstantNTP(contents,
                                   controller.GetLastCommittedEntry()));
}

TEST_F(SearchTest, UseLocalNTPInIncognito) {
  EnableInstantExtendedAPIForTesting();
  ASSERT_TRUE(base::FieldTrialList::CreateFieldTrial("InstantExtended",
      "Group1 use_cacheable_ntp:1"));
  EXPECT_EQ(GURL(), chrome::GetNewTabPageURL(
      profile()->GetOffTheRecordProfile()));
}

TEST_F(SearchTest, UseLocalNTPIfNTPURLIsInsecure) {
  EnableInstantExtendedAPIForTesting();
  ASSERT_TRUE(base::FieldTrialList::CreateFieldTrial("InstantExtended",
      "Group1 use_cacheable_ntp:1"));
  // Set an insecure new tab page URL and verify that it's ignored.
  SetSearchProvider(true, true);
  EXPECT_EQ(GURL(chrome::kChromeSearchLocalNtpUrl),
            chrome::GetNewTabPageURL(profile()));
}

TEST_F(SearchTest, UseLocalNTPIfNTPURLIsNotSet) {
  EnableInstantExtendedAPIForTesting();
  ASSERT_TRUE(base::FieldTrialList::CreateFieldTrial("InstantExtended",
      "Group1 use_cacheable_ntp:1"));
  // Set an insecure new tab page URL and verify that it's ignored.
  SetSearchProvider(false, true);
  EXPECT_EQ(GURL(chrome::kChromeSearchLocalNtpUrl),
            chrome::GetNewTabPageURL(profile()));
}

TEST_F(SearchTest, UseLocalNTPIfNTPURLIsBlockedForSupervisedUser) {
  EnableInstantExtendedAPIForTesting();
  ASSERT_TRUE(base::FieldTrialList::CreateFieldTrial("InstantExtended",
      "Group1 use_cacheable_ntp:1"));

  // Block access to foo.com in the URL filter.
  ManagedUserService* managed_user_service =
      ManagedUserServiceFactory::GetForProfile(profile());
  ManagedModeURLFilter* url_filter =
      managed_user_service->GetURLFilterForUIThread();
  std::map<std::string, bool> hosts;
  hosts["foo.com"] = false;
  url_filter->SetManualHosts(&hosts);

  EXPECT_EQ(GURL(chrome::kChromeSearchLocalNtpUrl),
            chrome::GetNewTabPageURL(profile()));
  EXPECT_EQ(GURL(), GetInstantURL(profile(), kDisableStartMargin, false));
}

TEST_F(SearchTest, GetInstantURLExtendedEnabled) {
  // Instant is disabled, so no Instant URL.
  EXPECT_EQ(GURL(), GetInstantURL(profile(), kDisableStartMargin, false));

  // Enable Instant. Still no Instant URL because "strk" is missing.
  EnableInstantExtendedAPIForTesting();
  SetDefaultInstantTemplateUrl(false);
  EXPECT_EQ(GURL(), GetInstantURL(profile(), kDisableStartMargin, false));

  // Set an Instant URL with a valid search terms replacement key.
  SetDefaultInstantTemplateUrl(true);

  // Now there should be a valid Instant URL. Note the HTTPS "upgrade".
  EXPECT_EQ(GURL("https://foo.com/instant?foo=foo#foo=foo&strk"),
            GetInstantURL(profile(), kDisableStartMargin, false));

  // Enable suggest. No difference.
  profile()->GetPrefs()->SetBoolean(prefs::kSearchSuggestEnabled, true);
  EXPECT_EQ(GURL("https://foo.com/instant?foo=foo#foo=foo&strk"),
            GetInstantURL(profile(), kDisableStartMargin, false));

  // Disable suggest. No Instant URL.
  profile()->GetPrefs()->SetBoolean(prefs::kSearchSuggestEnabled, false);
  EXPECT_EQ(GURL(), GetInstantURL(profile(), kDisableStartMargin, false));
}

TEST_F(SearchTest, StartMarginCGI) {
  // Instant is disabled, so no Instant URL.
  EXPECT_EQ(GURL(), GetInstantURL(profile(), kDisableStartMargin, false));

  // Enable Instant. No margin.
  EnableInstantExtendedAPIForTesting();
  profile()->GetPrefs()->SetBoolean(prefs::kSearchSuggestEnabled, true);

  EXPECT_EQ(GURL("https://foo.com/instant?foo=foo#foo=foo&strk"),
            GetInstantURL(profile(), kDisableStartMargin, false));

  // With start margin.
  EXPECT_EQ(GURL("https://foo.com/instant?es_sm=10&foo=foo#foo=foo&strk"),
            GetInstantURL(profile(), 10, false));
}

TEST_F(SearchTest, InstantSearchEnabledCGI) {
  EnableInstantExtendedAPIForTesting();

  // Disable Instant Search.
  // Make sure {google:forceInstantResults} is not set in the Instant URL.
  EXPECT_EQ(GURL("https://foo.com/instant?foo=foo#foo=foo&strk"),
            GetInstantURL(profile(), kDisableStartMargin, false));

  // Enable Instant Search.
  // Make sure {google:forceInstantResults} is set in the Instant URL.
  EXPECT_EQ(GURL("https://foo.com/instant?ion=1&foo=foo#foo=foo&strk"),
            GetInstantURL(profile(), kDisableStartMargin, true));
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
  GURL instant_url(GetInstantURL(profile(), kDisableStartMargin, false));
  ASSERT_TRUE(instant_url.is_valid());
  EXPECT_EQ("https://www.foo.com/webhp?strk", instant_url.spec());

  // However, if the Google base URL is specified on the command line, the
  // instant URL should just use it, even if it's HTTP.
  UIThreadSearchTermsData::SetGoogleBaseURL(std::string());
  CommandLine::ForCurrentProcess()->AppendSwitchASCII(switches::kGoogleBaseURL,
                                                      "http://www.bar.com/");
  instant_url = GetInstantURL(profile(), kDisableStartMargin, false);
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
  instant_url = GetInstantURL(profile(), kDisableStartMargin, false);
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

TEST_F(SearchTest, ShouldShowInstantNTP_DisabledByUseCacheableNTPFinchFlag) {
  EnableInstantExtendedAPIForTesting();
  ASSERT_TRUE(base::FieldTrialList::CreateFieldTrial("InstantExtended",
      "Group1 use_cacheable_ntp:1"));
  EXPECT_FALSE(ShouldShowInstantNTP());
}

TEST_F(SearchTest, ShouldUseCacheableNTP_Default) {
  EnableInstantExtendedAPIForTesting();
  EXPECT_FALSE(ShouldUseCacheableNTP());
}

TEST_F(SearchTest, ShouldUseCacheableNTP_EnabledViaFinch) {
  EnableInstantExtendedAPIForTesting();
  ASSERT_TRUE(base::FieldTrialList::CreateFieldTrial("InstantExtended",
      "Group1 use_cacheable_ntp:1"));
  EXPECT_TRUE(ShouldUseCacheableNTP());
}

TEST_F(SearchTest, ShouldUseCacheableNTP_EnabledViaCommandLine) {
  EnableInstantExtendedAPIForTesting();
  CommandLine::ForCurrentProcess()->
      AppendSwitch(switches::kUseCacheableNewTabPage);
  ASSERT_TRUE(base::FieldTrialList::CreateFieldTrial("InstantExtended",
      "Group1 use_cacheable_ntp:0"));
  EXPECT_TRUE(ShouldUseCacheableNTP());
}

TEST_F(SearchTest, IsNTPURL) {
  GURL invalid_url;
  GURL ntp_url(chrome::kChromeUINewTabURL);
  GURL local_ntp_url(GetLocalInstantURL(profile()));

  EXPECT_FALSE(chrome::IsNTPURL(invalid_url, profile()));
  EXPECT_FALSE(chrome::IsNTPURL(local_ntp_url, profile()));

  EXPECT_TRUE(chrome::IsNTPURL(ntp_url, NULL));
  EXPECT_FALSE(chrome::IsNTPURL(local_ntp_url, NULL));

  // Enable Instant. No margin.
  EnableInstantExtendedAPIForTesting();
  profile()->GetPrefs()->SetBoolean(prefs::kSearchSuggestEnabled, true);
  GURL remote_ntp_url(GetInstantURL(profile(), kDisableStartMargin, false));
  GURL search_url_with_search_terms("https://foo.com/url?strk&bar=abc");
  GURL search_url_without_search_terms("https://foo.com/url?strk&bar");

  EXPECT_FALSE(chrome::IsNTPURL(ntp_url, profile()));
  EXPECT_TRUE(chrome::IsNTPURL(local_ntp_url, profile()));
  EXPECT_TRUE(chrome::IsNTPURL(remote_ntp_url, profile()));
  EXPECT_FALSE(chrome::IsNTPURL(search_url_with_search_terms, profile()));
  EXPECT_TRUE(chrome::IsNTPURL(search_url_without_search_terms, profile()));

  EXPECT_FALSE(chrome::IsNTPURL(ntp_url, NULL));
  EXPECT_FALSE(chrome::IsNTPURL(local_ntp_url, NULL));
  EXPECT_FALSE(chrome::IsNTPURL(remote_ntp_url, NULL));
  EXPECT_FALSE(chrome::IsNTPURL(search_url_with_search_terms, NULL));
  EXPECT_FALSE(chrome::IsNTPURL(search_url_without_search_terms, NULL));
}

TEST_F(SearchTest, GetSearchURLs) {
  std::vector<GURL> search_urls = GetSearchURLs(profile());
  EXPECT_EQ(2U, search_urls.size());
  EXPECT_EQ("http://foo.com/alt#quux=", search_urls[0].spec());
  EXPECT_EQ("http://foo.com/url?bar=", search_urls[1].spec());
}

}  // namespace chrome
