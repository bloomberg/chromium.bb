// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "base/command_line.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram_base.h"
#include "base/metrics/histogram_samples.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/search/instant_service.h"
#include "chrome/browser/search/instant_service_factory.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/search_engines/ui_thread_search_terms_data.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/features.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/search_test_utils.h"
#include "components/google/core/browser/google_switches.h"
#include "components/prefs/pref_service.h"
#include "components/search/search.h"
#include "components/search_engines/search_engines_switches.h"
#include "components/search_engines/template_url_service.h"
#include "components/variations/entropy_provider.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/renderer_preferences.h"
#include "url/gurl.h"

#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
#include "chrome/browser/supervised_user/supervised_user_service.h"
#include "chrome/browser/supervised_user/supervised_user_service_factory.h"
#include "chrome/browser/supervised_user/supervised_user_url_filter.h"
#endif

namespace search {

class SearchTest : public BrowserWithTestWindowTest {
 protected:
  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();
    field_trial_list_.reset(new base::FieldTrialList(
        base::MakeUnique<metrics::SHA1EntropyProvider>("42")));
    TemplateURLServiceFactory::GetInstance()->SetTestingFactoryAndUse(
        profile(), &TemplateURLServiceFactory::BuildInstanceFor);
    TemplateURLService* template_url_service =
        TemplateURLServiceFactory::GetForProfile(profile());
    search_test_utils::WaitForTemplateURLServiceToLoad(template_url_service);
    SetSearchProvider(true, false);
  }

  virtual void SetSearchProvider(bool set_ntp_url, bool insecure_ntp_url) {
    TemplateURLService* template_url_service =
        TemplateURLServiceFactory::GetForProfile(profile());
    TemplateURLData data;
    data.SetShortName(base::ASCIIToUTF16("foo.com"));
    data.SetURL("http://foo.com/url?bar={searchTerms}");
    data.instant_url = "http://foo.com/instant?"
        "{google:forceInstantResults}foo=foo#foo=foo&strk";
    if (set_ntp_url) {
      data.new_tab_url = (insecure_ntp_url ? "http" : "https") +
          std::string("://foo.com/newtab?strk");
    }
    data.alternate_urls.push_back("http://foo.com/alt#quux={searchTerms}");
    data.search_terms_replacement_key = "strk";

    TemplateURL* template_url =
        template_url_service->Add(base::MakeUnique<TemplateURL>(data));
    template_url_service->SetUserSelectedDefaultSearchProvider(template_url);
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
    data.SetShortName(base::ASCIIToUTF16("foo.com"));
    data.SetURL("http://foo.com/url?bar={searchTerms}");
    data.instant_url = (has_search_term_replacement_key ?
        kInstantURLWithStrk : kInstantURLNoStrk);
    data.search_terms_replacement_key = "strk";

    TemplateURL* template_url =
        template_url_service->Add(base::MakeUnique<TemplateURL>(data));
    template_url_service->SetUserSelectedDefaultSearchProvider(template_url);
  }

  bool InInstantProcess(const content::WebContents* contents) {
    InstantService* instant_service =
        InstantServiceFactory::GetForProfile(profile());
    return instant_service->IsInstantProcess(
        contents->GetRenderProcessHost()->GetID());
  }

  std::unique_ptr<base::FieldTrialList> field_trial_list_;
};

struct SearchTestCase {
  const char* url;
  bool expected_result;
  const char* comment;
};

TEST_F(SearchTest, ShouldAssignURLToInstantRendererExtendedEnabled) {
  const SearchTestCase kTestCases[] = {
    {chrome::kChromeSearchLocalNtpUrl, true,  ""},
    {"https://foo.com/instant?strk",   true,  ""},
    {"https://foo.com/instant#strk",   true,  ""},
    {"https://foo.com/instant?strk=0", true,  ""},
    {"https://foo.com/url?strk",       false, ""},
    {"https://foo.com/alt?strk",       false, ""},
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
      "EmbeddedSearch", "Group1 espv:2 suppress_on_srp:1"));

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
  const SearchTestCase kTestCases[] = {
    {"chrome-search://local-ntp",      true,  "Local NTP"},
    {"chrome-search://remote-ntp",     true,  "Remote NTP"},
    {"invalid-scheme://local-ntp",     false, "Invalid Local NTP URL"},
    {"invalid-scheme://online-ntp",    false, "Invalid Online NTP URL"},
    {"chrome-search://foo.com",        false, "Search result page"},
    {"https://foo.com/instant?strk",   false,  ""},
    {"https://foo.com/instant#strk",   false,  ""},
    {"https://foo.com/instant?strk=0", false,  ""},
    {"https://foo.com/url?strk",       false,  ""},
    {"https://foo.com/alt?strk",       false,  ""},
    {"https://foo.com:80/instant",     false,  "HTTPS with port"},
    {"http://foo.com/instant",         false,  "Non-HTTPS"},
    {"http://foo.com/instant?strk",    false,  "Non-HTTPS"},
    {"http://foo.com/instant?strk=1",  false,  "Non-HTTPS"},
    {"http://foo.com:443/instant",     false,  "Non-HTTPS"},
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
   "https://foo.com/url?strk",        false,  false },
  {"Local NTP -> Regular",
   "chrome-search://local-ntp",       true,
   "https://foo.com/other",           false,  false },
  {"Remote NTP -> SRP",
   "https://foo.com/newtab?strk",     true,
   "https://foo.com/url?strk",        false,  false },
  {"Remote NTP -> Regular",
   "https://foo.com/newtab?strk",     true,
   "https://foo.com/other",           false,  false },
  {"SRP -> SRP",
   "https://foo.com/url?strk",        false,
   "https://foo.com/url?strk",        false,  true  },
  {"SRP -> Regular",
   "https://foo.com/url?strk",        false,
   "https://foo.com/other",           false,  true },
  {"Regular -> SRP",
   "https://foo.com/other",           false,
   "https://foo.com/url?strk",        false,  true },
};

TEST_F(SearchTest, ProcessIsolation) {
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
              start_site_instance.get() == contents->GetSiteInstance())
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
    load_params.transition_type = ui::PAGE_TRANSITION_LINK;

    controller->LoadURLWithParams(load_params);
    CommitPendingLoad(controller);
    EXPECT_EQ(test.end_in_instant_process, InInstantProcess(contents))
        << test.description;

    EXPECT_EQ(test.same_site_instance,
              start_site_instance.get() == contents->GetSiteInstance())
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
  {"https://foo.com/instant?strk",         false, "Valid Instant URL"},
  {"https://foo.com/instant#strk",         false, "Valid Instant URL"},
  {"https://foo.com/url?strk",             false, "Valid search URL"},
  {"https://foo.com/url#strk",             false, "Valid search URL"},
  {"https://foo.com/alt?strk",             false, "Valid alternative URL"},
  {"https://foo.com/alt#strk",             false, "Valid alternative URL"},
  {"https://foo.com/url?strk&bar=",        false, "No query terms"},
  {"https://foo.com/url?strk&q=abc",       false, "No query terms key"},
  {"https://foo.com/url?strk#bar=abc",     false, "Query terms key in ref"},
  {"https://foo.com/url?strk&bar=abc",     false, "Has query terms"},
  {"http://foo.com/instant?strk=1",        false, "Insecure URL"},
  {"https://foo.com/instant",              false, "No search term replacement"},
  {"chrome://blank/",                      false, "Chrome scheme"},
  {"chrome-search://foo",                  false, "Chrome-search scheme"},
  {"https://bar.com/instant?strk=1",       false, "Random non-search page"},
  {chrome::kChromeSearchLocalNtpUrl,       true,  "Local new tab page"},
  {"https://foo.com/newtab?strk",          true,  "New tab URL"},
  {"http://foo.com/newtab?strk",           false, "Insecure New tab URL"},
};

TEST_F(SearchTest, InstantNTPExtendedEnabled) {
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

TEST_F(SearchTest, InstantNTPCustomNavigationEntry) {
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
                                         ui::PAGE_TRANSITION_LINK,
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
  AddTab(browser(), GURL("chrome://blank"));
  content::WebContents* contents =
        browser()->tab_strip_model()->GetWebContentsAt(0);
  content::NavigationController& controller = contents->GetController();
  // Local NTP.
  NavigateAndCommitActiveTab(GURL(chrome::kChromeSearchLocalNtpUrl));
  EXPECT_TRUE(NavEntryIsInstantNTP(contents,
                                   controller.GetLastCommittedEntry()));
  // Instant page is not cacheable NTP.
  NavigateAndCommitActiveTab(GetInstantURL(profile(), false));
  EXPECT_FALSE(NavEntryIsInstantNTP(contents,
                                    controller.GetLastCommittedEntry()));
  // Test Cacheable NTP
  NavigateAndCommitActiveTab(GetNewTabPageURL(profile()));
  EXPECT_TRUE(NavEntryIsInstantNTP(contents,
                                   controller.GetLastCommittedEntry()));
}

TEST_F(SearchTest, InstantCacheableNTPNavigationEntryNewProfile) {
  SetSearchProvider(false, false);
  AddTab(browser(), GURL(chrome::kChromeUINewTabURL));
  content::WebContents* contents =
        browser()->tab_strip_model()->GetWebContentsAt(0);
  content::NavigationController& controller = contents->GetController();
  // Test virtual url chrome://newtab  for first NTP of a new profile
  EXPECT_TRUE(NavEntryIsInstantNTP(contents,
                                   controller.GetLastCommittedEntry()));
  // The new_tab_url gets set after the first NTP is visible.
  SetSearchProvider(true, false);
  EXPECT_TRUE(NavEntryIsInstantNTP(contents,
                                   controller.GetLastCommittedEntry()));
}

TEST_F(SearchTest, NoRewriteInIncognito) {
  profile()->ForceIncognito(true);
  EXPECT_EQ(GURL(), GetNewTabPageURL(profile()));
  GURL new_tab_url(chrome::kChromeUINewTabURL);
  EXPECT_FALSE(HandleNewTabURLRewrite(&new_tab_url, profile()));
  EXPECT_EQ(GURL(chrome::kChromeUINewTabURL), new_tab_url);
}

TEST_F(SearchTest, UseLocalNTPIfNTPURLIsInsecure) {
  // Set an insecure new tab page URL and verify that it's ignored.
  SetSearchProvider(true, true);
  EXPECT_EQ(GURL(chrome::kChromeSearchLocalNtpUrl),
            GetNewTabPageURL(profile()));
  GURL new_tab_url(chrome::kChromeUINewTabURL);
  EXPECT_TRUE(HandleNewTabURLRewrite(&new_tab_url, profile()));
  EXPECT_EQ(GURL(chrome::kChromeSearchLocalNtpUrl), new_tab_url);
}

TEST_F(SearchTest, UseLocalNTPIfNTPURLIsNotSet) {
  // Set an insecure new tab page URL and verify that it's ignored.
  SetSearchProvider(false, true);
  EXPECT_EQ(GURL(chrome::kChromeSearchLocalNtpUrl),
            GetNewTabPageURL(profile()));
  GURL new_tab_url(chrome::kChromeUINewTabURL);
  EXPECT_TRUE(HandleNewTabURLRewrite(&new_tab_url, profile()));
  EXPECT_EQ(GURL(chrome::kChromeSearchLocalNtpUrl), new_tab_url);
}

#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
TEST_F(SearchTest, UseLocalNTPIfNTPURLIsBlockedForSupervisedUser) {
  // Block access to foo.com in the URL filter.
  SupervisedUserService* supervised_user_service =
      SupervisedUserServiceFactory::GetForProfile(profile());
  SupervisedUserURLFilter* url_filter = supervised_user_service->GetURLFilter();
  std::map<std::string, bool> hosts;
  hosts["foo.com"] = false;
  url_filter->SetManualHosts(std::move(hosts));

  EXPECT_EQ(GURL(chrome::kChromeSearchLocalNtpUrl),
            GetNewTabPageURL(profile()));
  GURL new_tab_url(chrome::kChromeUINewTabURL);
  EXPECT_TRUE(HandleNewTabURLRewrite(&new_tab_url, profile()));
  EXPECT_EQ(GURL(chrome::kChromeSearchLocalNtpUrl), new_tab_url);
  EXPECT_EQ(GURL(), GetInstantURL(profile(), false));
}
#endif

TEST_F(SearchTest, GetInstantURL) {
  // No Instant URL because "strk" is missing.
  SetDefaultInstantTemplateUrl(false);
  EXPECT_EQ(GURL(), GetInstantURL(profile(), false));

  // Set an Instant URL with a valid search terms replacement key.
  SetDefaultInstantTemplateUrl(true);

  // Now there should be a valid Instant URL. Note the HTTPS "upgrade".
  EXPECT_EQ(GURL("https://foo.com/instant?foo=foo#foo=foo&strk"),
            GetInstantURL(profile(), false));

  // Enable suggest. No difference.
  profile()->GetPrefs()->SetBoolean(prefs::kSearchSuggestEnabled, true);
  EXPECT_EQ(GURL("https://foo.com/instant?foo=foo#foo=foo&strk"),
            GetInstantURL(profile(), false));

  // Disable suggest. No Instant URL.
  profile()->GetPrefs()->SetBoolean(prefs::kSearchSuggestEnabled, false);
  EXPECT_EQ(GURL(), GetInstantURL(profile(), false));
}

TEST_F(SearchTest, InstantSearchEnabledCGI) {
  // Disable Instant Search.
  // Make sure {google:forceInstantResults} is not set in the Instant URL.
  EXPECT_EQ(GURL("https://foo.com/instant?foo=foo#foo=foo&strk"),
            GetInstantURL(profile(), false));

  // Enable Instant Search.
  // Make sure {google:forceInstantResults} is set in the Instant URL.
  EXPECT_EQ(GURL("https://foo.com/instant?ion=1&foo=foo#foo=foo&strk"),
            GetInstantURL(profile(), true));
}

TEST_F(SearchTest, CommandLineOverrides) {
  TemplateURLService* template_url_service =
      TemplateURLServiceFactory::GetForProfile(profile());
  TemplateURLData data;
  data.SetShortName(base::ASCIIToUTF16("Google"));
  data.SetURL("{google:baseURL}search?q={searchTerms}");
  data.instant_url = "{google:baseURL}webhp?strk";
  data.search_terms_replacement_key = "strk";
  TemplateURL* template_url =
      template_url_service->Add(base::MakeUnique<TemplateURL>(data));
  template_url_service->SetUserSelectedDefaultSearchProvider(template_url);

  // By default, Instant Extended forces the instant URL to be HTTPS, so even if
  // we set a Google base URL that is HTTP, we should get an HTTPS URL.
  UIThreadSearchTermsData::SetGoogleBaseURL("http://www.foo.com/");
  GURL instant_url(GetInstantURL(profile(), false));
  ASSERT_TRUE(instant_url.is_valid());
  EXPECT_EQ("https://www.foo.com/webhp?strk", instant_url.spec());

  // However, if the Google base URL is specified on the command line, the
  // instant URL should just use it, even if it's HTTP.
  UIThreadSearchTermsData::SetGoogleBaseURL(std::string());
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kGoogleBaseURL, "http://www.bar.com/");
  instant_url = GetInstantURL(profile(), false);
  ASSERT_TRUE(instant_url.is_valid());
  EXPECT_EQ("http://www.bar.com/webhp?strk", instant_url.spec());

  // If we specify extra search query params, they should be inserted into the
  // query portion of the instant URL.
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kExtraSearchQueryParams, "a=b");
  instant_url = GetInstantURL(profile(), false);
  ASSERT_TRUE(instant_url.is_valid());
  EXPECT_EQ("http://www.bar.com/webhp?a=b&strk", instant_url.spec());
}

TEST_F(SearchTest, IsNTPURL) {
  GURL invalid_url;
  GURL ntp_url(chrome::kChromeUINewTabURL);
  GURL local_ntp_url(chrome::kChromeSearchLocalNtpUrl);

  EXPECT_FALSE(IsNTPURL(invalid_url, profile()));
  // No margin.
  profile()->GetPrefs()->SetBoolean(prefs::kSearchSuggestEnabled, true);
  GURL remote_ntp_url(GetInstantURL(profile(), false));
  GURL remote_ntp_service_worker_url("https://foo.com/newtab-serviceworker.js");
  GURL search_url_with_search_terms("https://foo.com/url?strk&bar=abc");
  GURL search_url_without_search_terms("https://foo.com/url?strk&bar");

  EXPECT_FALSE(IsNTPURL(ntp_url, profile()));
  EXPECT_TRUE(IsNTPURL(local_ntp_url, profile()));
  EXPECT_TRUE(IsNTPURL(remote_ntp_url, profile()));
  EXPECT_TRUE(IsNTPURL(remote_ntp_service_worker_url, profile()));
  EXPECT_FALSE(IsNTPURL(search_url_with_search_terms, profile()));
  EXPECT_FALSE(IsNTPURL(search_url_without_search_terms, profile()));

  EXPECT_FALSE(IsNTPURL(ntp_url, NULL));
  EXPECT_FALSE(IsNTPURL(local_ntp_url, NULL));
  EXPECT_FALSE(IsNTPURL(remote_ntp_url, NULL));
  EXPECT_FALSE(IsNTPURL(remote_ntp_service_worker_url, NULL));
  EXPECT_FALSE(IsNTPURL(search_url_with_search_terms, NULL));
  EXPECT_FALSE(IsNTPURL(search_url_without_search_terms, NULL));
}

TEST_F(SearchTest, GetSearchURLs) {
  std::vector<GURL> search_urls = GetSearchURLs(profile());
  EXPECT_EQ(2U, search_urls.size());
  EXPECT_EQ("http://foo.com/alt#quux=", search_urls[0].spec());
  EXPECT_EQ("http://foo.com/url?bar=", search_urls[1].spec());
}

TEST_F(SearchTest, GetSearchResultPrefetchBaseURL) {
  EXPECT_EQ(GURL("https://foo.com/instant?ion=1&foo=foo#foo=foo&strk"),
            GetSearchResultPrefetchBaseURL(profile()));
}

struct ExtractSearchTermsTestCase {
  const char* url;
  const char* expected_result;
  const char* comment;
};

TEST_F(SearchTest, ExtractSearchTermsFromURL) {
  const ExtractSearchTermsTestCase kTestCases[] = {
    {chrome::kChromeSearchLocalNtpUrl,           "",    "NTP url"},
    {"https://foo.com/instant?strk",             "",    "Invalid search url"},
    {"https://foo.com/instant#strk",             "",    "Invalid search url"},
    {"https://foo.com/alt#quux=foo",             "foo", "Valid search url"},
    {"https://foo.com/alt#quux=foo&strk",        "foo", "Valid search url"}
  };

  for (size_t i = 0; i < arraysize(kTestCases); ++i) {
    const ExtractSearchTermsTestCase& test = kTestCases[i];
    EXPECT_EQ(test.expected_result,
              base::UTF16ToASCII(
                  ExtractSearchTermsFromURL(profile(), GURL(test.url))))
        << test.url << " " << test.comment;
  }
}

// Regression test for https://crbug.com/605720: Set up a search provider backed
// by localhost on a specific port, like browsertests do.  The chrome-search://
// URLs generated in this mode should not have ports.
TEST_F(SearchTest, SearchProviderWithPort) {
  TemplateURLService* template_url_service =
      TemplateURLServiceFactory::GetForProfile(profile());
  TemplateURLData data;
  data.SetShortName(base::ASCIIToUTF16("localhost"));
  data.SetURL("https://[::1]:1993/url?bar={searchTerms}");
  data.instant_url =
      "https://[::1]:1993/instant?"
      "{google:forceInstantResults}foo=foo#foo=foo&strk";
  data.new_tab_url = "https://[::1]:1993/newtab?strk";
  data.alternate_urls.push_back("https://[::1]:1993/alt#quux={searchTerms}");
  data.search_terms_replacement_key = "strk";

  TemplateURL* template_url =
      template_url_service->Add(base::MakeUnique<TemplateURL>(data));
  template_url_service->SetUserSelectedDefaultSearchProvider(template_url);

  EXPECT_TRUE(ShouldAssignURLToInstantRenderer(
      GURL("https://[::1]:1993/newtab?lala"), profile()));
  EXPECT_FALSE(ShouldAssignURLToInstantRenderer(
      GURL("https://[::1]:1992/newtab?lala"), profile()));
  EXPECT_EQ(GURL("chrome-search://remote-ntp/newtab?lala"),
            GetEffectiveURLForInstant(GURL("https://[::1]:1993/newtab?lala"),
                                      profile()));
  EXPECT_EQ(GURL("chrome-search://[::1]/instant?strk"),
            GetEffectiveURLForInstant(GURL("https://[::1]:1993/instant?strk"),
                                      profile()));
  EXPECT_FALSE(ShouldAssignURLToInstantRenderer(
      GURL("https://[::1]:1993/unregistered-path?strk"), profile()));
}

class SearchURLTest : public SearchTest {
 protected:
  void SetSearchProvider(bool set_ntp_url, bool insecure_ntp_url) override {
    TemplateURLService* template_url_service =
        TemplateURLServiceFactory::GetForProfile(profile());
    TemplateURLData data;
    data.SetShortName(base::ASCIIToUTF16("Google"));
    data.SetURL("{google:baseURL}search?"
                "{google:instantExtendedEnabledParameter}q={searchTerms}");
    data.search_terms_replacement_key = "espv";
    template_url_ =
        template_url_service->Add(base::MakeUnique<TemplateURL>(data));
    template_url_service->SetUserSelectedDefaultSearchProvider(template_url_);
  }

  TemplateURL* template_url_;
};

TEST_F(SearchURLTest, QueryExtractionDisabled) {
  UIThreadSearchTermsData::SetGoogleBaseURL("http://www.google.com/");
  TemplateURLRef::SearchTermsArgs search_terms_args(base::ASCIIToUTF16("foo"));
  GURL result(template_url_->url_ref().ReplaceSearchTerms(
      search_terms_args, UIThreadSearchTermsData(profile())));
  ASSERT_TRUE(result.is_valid());
  // Query extraction is disabled. Make sure
  // {google:instantExtendedEnabledParameter} is not set in the search URL.
  EXPECT_EQ("http://www.google.com/search?q=foo", result.spec());
}

}  // namespace search
