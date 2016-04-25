// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search/search.h"

#include <stddef.h>

#include "base/command_line.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/search/instant_service.h"
#include "chrome/browser/search/instant_service_factory.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/search_engines/ui_thread_search_terms_data.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_instant_controller.h"
#include "chrome/browser/ui/search/instant_search_prerenderer.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/search_urls.h"
#include "chrome/common/url_constants.h"
#include "components/google/core/browser/google_util.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/search/search.h"
#include "components/search_engines/template_url_service.h"
#include "components/sessions/core/serialized_navigation_entry.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"

#if defined(ENABLE_SUPERVISED_USERS)
#include "chrome/browser/supervised_user/supervised_user_service.h"
#include "chrome/browser/supervised_user/supervised_user_service_factory.h"
#include "chrome/browser/supervised_user/supervised_user_url_filter.h"
#endif

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/login/signin/merge_session_throttling_utils.h"
#endif  // defined(OS_CHROMEOS)

namespace search {

namespace {

const char kPrefetchSearchResultsOnSRP[] = "prefetch_results_srp";
const char kPrerenderInstantUrlOnOmniboxFocus[] =
    "prerender_instant_url_on_omnibox_focus";

// Controls whether to use the alternate Instant search base URL. This allows
// experimentation of Instant search.
const char kUseAltInstantURL[] = "use_alternate_instant_url";
const char kUseSearchPathForInstant[] = "use_search_path_for_instant";
const char kAltInstantURLPath[] = "search";
const char kAltInstantURLQueryParams[] = "&qbp=1";

const char kShouldShowGoogleLocalNTPFlagName[] = "google_local_ntp";

// Status of the New Tab URL for the default Search provider. NOTE: Used in a
// UMA histogram so values should only be added at the end and not reordered.
enum NewTabURLState {
  // Valid URL that should be used.
  NEW_TAB_URL_VALID = 0,

  // Corrupt state (e.g. no profile or template url).
  NEW_TAB_URL_BAD = 1,

  // URL should not be used because in incognito window.
  NEW_TAB_URL_INCOGNITO = 2,

  // No New Tab URL set for provider.
  NEW_TAB_URL_NOT_SET = 3,

  // URL is not secure.
  NEW_TAB_URL_INSECURE = 4,

  // URL should not be used because Suggest is disabled.
  // Not used anymore, see crbug.com/340424.
  // NEW_TAB_URL_SUGGEST_OFF = 5,

  // URL should not be used because it is blocked for a supervised user.
  NEW_TAB_URL_BLOCKED = 6,

  NEW_TAB_URL_MAX
};

// Used to set the Instant support state of the Navigation entry.
const char kInstantSupportStateKey[] = "instant_support_state";

const char kInstantSupportEnabled[] = "Instant support enabled";
const char kInstantSupportDisabled[] = "Instant support disabled";
const char kInstantSupportUnknown[] = "Instant support unknown";

InstantSupportState StringToInstantSupportState(const base::string16& value) {
  if (value == base::ASCIIToUTF16(kInstantSupportEnabled))
    return INSTANT_SUPPORT_YES;
  else if (value == base::ASCIIToUTF16(kInstantSupportDisabled))
    return INSTANT_SUPPORT_NO;
  else
    return INSTANT_SUPPORT_UNKNOWN;
}

base::string16 InstantSupportStateToString(InstantSupportState state) {
  switch (state) {
    case INSTANT_SUPPORT_NO:
      return base::ASCIIToUTF16(kInstantSupportDisabled);
    case INSTANT_SUPPORT_YES:
      return base::ASCIIToUTF16(kInstantSupportEnabled);
    case INSTANT_SUPPORT_UNKNOWN:
      return base::ASCIIToUTF16(kInstantSupportUnknown);
  }
  return base::ASCIIToUTF16(kInstantSupportUnknown);
}

TemplateURL* GetDefaultSearchProviderTemplateURL(Profile* profile) {
  if (profile) {
    TemplateURLService* template_url_service =
        TemplateURLServiceFactory::GetForProfile(profile);
    if (template_url_service)
      return template_url_service->GetDefaultSearchProvider();
  }
  return NULL;
}

GURL TemplateURLRefToGURL(const TemplateURLRef& ref,
                          const SearchTermsData& search_terms_data,
                          bool append_extra_query_params,
                          bool force_instant_results) {
  TemplateURLRef::SearchTermsArgs search_terms_args =
      TemplateURLRef::SearchTermsArgs(base::string16());
  search_terms_args.append_extra_query_params = append_extra_query_params;
  search_terms_args.force_instant_results = force_instant_results;
  return GURL(ref.ReplaceSearchTerms(search_terms_args, search_terms_data));
}

bool MatchesAnySearchURL(const GURL& url,
                         TemplateURL* template_url,
                         const SearchTermsData& search_terms_data) {
  for (const TemplateURLRef& ref : template_url->url_refs()) {
    GURL search_url =
        TemplateURLRefToGURL(ref, search_terms_data, false, false);
    if (search_url.is_valid() && MatchesOriginAndPath(url, search_url))
      return true;
  }
  return false;
}

// Returns true if |url| can be used as an Instant URL for |profile|.
bool IsInstantURL(const GURL& url, Profile* profile) {
  if (!IsInstantExtendedAPIEnabled())
    return false;

  if (!url.is_valid())
    return false;

  const GURL new_tab_url(GetNewTabPageURL(profile));
  if (new_tab_url.is_valid() && MatchesOriginAndPath(url, new_tab_url))
    return true;

  TemplateURL* template_url = GetDefaultSearchProviderTemplateURL(profile);
  if (!template_url)
    return false;

  if (!IsSuitableURLForInstant(url, template_url))
    return false;

  const TemplateURLRef& instant_url_ref = template_url->instant_url_ref();
  UIThreadSearchTermsData search_terms_data(profile);
  const GURL instant_url = TemplateURLRefToGURL(
      instant_url_ref, search_terms_data, false, false);
  if (!instant_url.is_valid())
    return false;

  if (MatchesOriginAndPath(url, instant_url))
    return true;

  return IsQueryExtractionEnabled() &&
      MatchesAnySearchURL(url, template_url, search_terms_data);
}

base::string16 GetSearchTermsImpl(const content::WebContents* contents,
                                  const content::NavigationEntry* entry) {
  if (!contents || !IsQueryExtractionEnabled())
    return base::string16();

  // For security reasons, don't extract search terms if the page is not being
  // rendered in the privileged Instant renderer process. This is to protect
  // against a malicious page somehow scripting the search results page and
  // faking search terms in the URL. Random pages can't get into the Instant
  // renderer and scripting doesn't work cross-process, so if the page is in
  // the Instant process, we know it isn't being exploited.
  Profile* profile = Profile::FromBrowserContext(contents->GetBrowserContext());
  if (IsInstantExtendedAPIEnabled() &&
      !IsRenderedInInstantProcess(contents, profile) &&
      ((entry == contents->GetController().GetLastCommittedEntry()) ||
       !ShouldAssignURLToInstantRenderer(entry->GetURL(), profile)))
    return base::string16();

  // Check to see if search terms have already been extracted.
  base::string16 search_terms = GetSearchTermsFromNavigationEntry(entry);
  if (!search_terms.empty())
    return search_terms;

  if (!IsQueryExtractionAllowedForURL(profile, entry->GetVirtualURL()))
    return base::string16();

  // Otherwise, extract from the URL.
  return ExtractSearchTermsFromURL(profile, entry->GetVirtualURL());
}

bool IsURLAllowedForSupervisedUser(const GURL& url, Profile* profile) {
#if defined(ENABLE_SUPERVISED_USERS)
  SupervisedUserService* supervised_user_service =
      SupervisedUserServiceFactory::GetForProfile(profile);
  SupervisedUserURLFilter* url_filter =
      supervised_user_service->GetURLFilterForUIThread();
  if (url_filter->GetFilteringBehaviorForURL(url) ==
          SupervisedUserURLFilter::BLOCK) {
    return false;
  }
#endif
  return true;
}

// Returns whether |new_tab_url| can be used as a URL for the New Tab page.
// NEW_TAB_URL_VALID means a valid URL; other enum values imply an invalid URL.
NewTabURLState IsValidNewTabURL(Profile* profile, const GURL& new_tab_url) {
  if (profile->IsOffTheRecord())
    return NEW_TAB_URL_INCOGNITO;
  if (!new_tab_url.is_valid())
    return NEW_TAB_URL_NOT_SET;
  if (!new_tab_url.SchemeIsCryptographic())
    return NEW_TAB_URL_INSECURE;
  if (!IsURLAllowedForSupervisedUser(new_tab_url, profile))
    return NEW_TAB_URL_BLOCKED;
  return NEW_TAB_URL_VALID;
}

// On Chrome OS, if the session hasn't merged yet, we need to avoid loading the
// remote NTP because that will trigger showing the merge session throttle
// interstitial page, which can show for 5+ seconds. crbug.com/591530.
bool ShouldShowLocalNewTab(const GURL& url, Profile* profile) {
#if defined(OS_CHROMEOS)
  if (merge_session_throttling_utils::ShouldDelayRequestForProfile(profile) &&
      merge_session_throttling_utils::ShouldDelayUrl(url)) {
    return true;
  }
#endif  // defined(OS_CHROMEOS)
  return false;
}

// Used to look up the URL to use for the New Tab page. Also tracks how we
// arrived at that URL so it can be logged with UMA.
struct NewTabURLDetails {
  NewTabURLDetails(const GURL& url, NewTabURLState state)
      : url(url), state(state) {}

  static NewTabURLDetails ForProfile(Profile* profile) {
    const GURL local_url(chrome::kChromeSearchLocalNtpUrl);

    base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
    if (command_line->HasSwitch(switches::kForceLocalNtp))
      return NewTabURLDetails(local_url, NEW_TAB_URL_VALID);

    TemplateURL* template_url = GetDefaultSearchProviderTemplateURL(profile);
    if (!profile || !template_url)
      return NewTabURLDetails(local_url, NEW_TAB_URL_BAD);

    GURL search_provider_url = TemplateURLRefToGURL(
        template_url->new_tab_url_ref(), UIThreadSearchTermsData(profile),
        false, false);

    if (ShouldShowLocalNewTab(search_provider_url, profile))
      return NewTabURLDetails(local_url, NEW_TAB_URL_VALID);

    NewTabURLState state = IsValidNewTabURL(profile, search_provider_url);
    switch (state) {
      case NEW_TAB_URL_VALID:
        // We can use the search provider's page.
        return NewTabURLDetails(search_provider_url, state);
      case NEW_TAB_URL_INCOGNITO:
        // Incognito has its own New Tab.
        return NewTabURLDetails(GURL(), state);
      default:
        // Use the local New Tab otherwise.
        return NewTabURLDetails(local_url, state);
    }
  }

  GURL url;
  NewTabURLState state;
};

}  // namespace

base::string16 ExtractSearchTermsFromURL(Profile* profile, const GURL& url) {
  if (url.is_valid() && url == GetSearchResultPrefetchBaseURL(profile)) {
    // InstantSearchPrerenderer has the search query for the Instant search base
    // page.
    InstantSearchPrerenderer* prerenderer =
        InstantSearchPrerenderer::GetForProfile(profile);
    // TODO(kmadhusu): Remove this CHECK after the investigation of
    // crbug.com/367204.
    CHECK(prerenderer);
    return prerenderer->get_last_query();
  }

  TemplateURL* template_url = GetDefaultSearchProviderTemplateURL(profile);
  base::string16 search_terms;
  if (template_url)
    template_url->ExtractSearchTermsFromURL(
        url, UIThreadSearchTermsData(profile), &search_terms);
  return search_terms;
}

bool IsQueryExtractionAllowedForURL(Profile* profile, const GURL& url) {
  TemplateURL* template_url = GetDefaultSearchProviderTemplateURL(profile);
  return template_url && IsSuitableURLForInstant(url, template_url);
}

base::string16 GetSearchTermsFromNavigationEntry(
    const content::NavigationEntry* entry) {
  base::string16 search_terms;
  if (entry)
    entry->GetExtraData(sessions::kSearchTermsKey, &search_terms);
  return search_terms;
}

base::string16 GetSearchTerms(const content::WebContents* contents) {
  if (!contents)
    return base::string16();

  const content::NavigationEntry* entry =
      contents->GetController().GetVisibleEntry();
  if (!entry)
    return base::string16();

  if (IsInstantExtendedAPIEnabled()) {
    InstantSupportState state =
        GetInstantSupportStateFromNavigationEntry(*entry);
    if (state == INSTANT_SUPPORT_NO)
      return base::string16();
  }

  return GetSearchTermsImpl(contents, entry);
}

bool ShouldAssignURLToInstantRenderer(const GURL& url, Profile* profile) {
  return url.is_valid() &&
         profile &&
         IsInstantExtendedAPIEnabled() &&
         (url.SchemeIs(chrome::kChromeSearchScheme) ||
          IsInstantURL(url, profile));
}

bool IsRenderedInInstantProcess(const content::WebContents* contents,
                                Profile* profile) {
  const content::RenderProcessHost* process_host =
      contents->GetRenderProcessHost();
  if (!process_host)
    return false;

  const InstantService* instant_service =
      InstantServiceFactory::GetForProfile(profile);
  if (!instant_service)
    return false;

  return instant_service->IsInstantProcess(process_host->GetID());
}

bool ShouldUseProcessPerSiteForInstantURL(const GURL& url, Profile* profile) {
  return ShouldAssignURLToInstantRenderer(url, profile) &&
      (url.host() == chrome::kChromeSearchLocalNtpHost ||
       url.host() == chrome::kChromeSearchRemoteNtpHost);
}

bool IsNTPURL(const GURL& url, Profile* profile) {
  if (!url.is_valid())
    return false;

  if (!IsInstantExtendedAPIEnabled())
    return url == GURL(chrome::kChromeUINewTabURL);

  const base::string16 search_terms = ExtractSearchTermsFromURL(profile, url);
  return profile &&
      ((IsInstantURL(url, profile) && search_terms.empty()) ||
       url == GURL(chrome::kChromeSearchLocalNtpUrl));
}

bool IsInstantNTP(const content::WebContents* contents) {
  if (!contents)
    return false;

  return NavEntryIsInstantNTP(contents,
                              contents->GetController().GetVisibleEntry());
}

bool NavEntryIsInstantNTP(const content::WebContents* contents,
                          const content::NavigationEntry* entry) {
  if (!contents || !entry || !IsInstantExtendedAPIEnabled())
    return false;

  Profile* profile = Profile::FromBrowserContext(contents->GetBrowserContext());
  if (!IsRenderedInInstantProcess(contents, profile))
    return false;

  if (entry->GetURL() == GURL(chrome::kChromeSearchLocalNtpUrl))
    return true;

  GURL new_tab_url(GetNewTabPageURL(profile));
  return new_tab_url.is_valid() &&
         MatchesOriginAndPath(entry->GetURL(), new_tab_url);
}

bool IsSuggestPrefEnabled(Profile* profile) {
  return profile && !profile->IsOffTheRecord() && profile->GetPrefs() &&
         profile->GetPrefs()->GetBoolean(prefs::kSearchSuggestEnabled);
}

GURL GetInstantURL(Profile* profile, bool force_instant_results) {
  if (!IsInstantExtendedAPIEnabled() || !IsSuggestPrefEnabled(profile))
    return GURL();

  TemplateURL* template_url = GetDefaultSearchProviderTemplateURL(profile);
  if (!template_url)
    return GURL();

  GURL instant_url = TemplateURLRefToGURL(
      template_url->instant_url_ref(), UIThreadSearchTermsData(profile),
      true, force_instant_results);
  if (!instant_url.is_valid() ||
      !template_url->HasSearchTermsReplacementKey(instant_url))
    return GURL();

  // Extended mode requires HTTPS.  Force it unless the base URL was overridden
  // on the command line, in which case we allow HTTP (see comments on
  // IsSuitableURLForInstant()).
  if (!instant_url.SchemeIsCryptographic() &&
      !google_util::StartsWithCommandLineGoogleBaseURL(instant_url)) {
    GURL::Replacements replacements;
    replacements.SetSchemeStr(url::kHttpsScheme);
    instant_url = instant_url.ReplaceComponents(replacements);
  }

  if (!IsURLAllowedForSupervisedUser(instant_url, profile))
    return GURL();

  if (ShouldUseAltInstantURL()) {
    GURL::Replacements replacements;
      const std::string path(
          ShouldUseSearchPathForInstant() ? kAltInstantURLPath : std::string());
    if (!path.empty())
      replacements.SetPathStr(path);
    const std::string query(
        instant_url.query() + std::string(kAltInstantURLQueryParams));
    replacements.SetQueryStr(query);
    instant_url = instant_url.ReplaceComponents(replacements);
  }
  return instant_url;
}

// Returns URLs associated with the default search engine for |profile|.
std::vector<GURL> GetSearchURLs(Profile* profile) {
  std::vector<GURL> result;
  TemplateURL* template_url = GetDefaultSearchProviderTemplateURL(profile);
  if (!template_url)
    return result;
  for (const TemplateURLRef& ref : template_url->url_refs()) {
    result.push_back(TemplateURLRefToGURL(ref, UIThreadSearchTermsData(profile),
                                          false, false));
  }
  return result;
}

GURL GetNewTabPageURL(Profile* profile) {
  return NewTabURLDetails::ForProfile(profile).url;
}

GURL GetSearchResultPrefetchBaseURL(Profile* profile) {
  return ShouldPrefetchSearchResults() ? GetInstantURL(profile, true) : GURL();
}

bool ShouldPrerenderInstantUrlOnOmniboxFocus() {
  if (!ShouldPrefetchSearchResults())
    return false;

  FieldTrialFlags flags;
  return GetFieldTrialInfo(&flags) && GetBoolValueForFlagWithDefault(
      kPrerenderInstantUrlOnOmniboxFocus, false, flags);
}

bool ShouldShowGoogleLocalNTP() {
  FieldTrialFlags flags;
  return !GetFieldTrialInfo(&flags) || GetBoolValueForFlagWithDefault(
      kShouldShowGoogleLocalNTPFlagName, true, flags);
}

GURL GetEffectiveURLForInstant(const GURL& url, Profile* profile) {
  CHECK(ShouldAssignURLToInstantRenderer(url, profile))
      << "Error granting Instant access.";

  if (url.SchemeIs(chrome::kChromeSearchScheme))
    return url;

  GURL effective_url(url);

  // Replace the scheme with "chrome-search:".
  url::Replacements<char> replacements;
  std::string search_scheme(chrome::kChromeSearchScheme);
  replacements.SetScheme(search_scheme.data(),
                         url::Component(0, search_scheme.length()));

  // If this is the URL for a server-provided NTP, replace the host with
  // "remote-ntp".
  std::string remote_ntp_host(chrome::kChromeSearchRemoteNtpHost);
  NewTabURLDetails details = NewTabURLDetails::ForProfile(profile);
  if (details.state == NEW_TAB_URL_VALID &&
      MatchesOriginAndPath(url, details.url)) {
    replacements.SetHost(remote_ntp_host.c_str(),
                         url::Component(0, remote_ntp_host.length()));
  }

  effective_url = effective_url.ReplaceComponents(replacements);
  return effective_url;
}

bool HandleNewTabURLRewrite(GURL* url,
                            content::BrowserContext* browser_context) {
  if (!IsInstantExtendedAPIEnabled())
    return false;

  if (!url->SchemeIs(content::kChromeUIScheme) ||
      url->host() != chrome::kChromeUINewTabHost)
    return false;

  Profile* profile = Profile::FromBrowserContext(browser_context);
  NewTabURLDetails details(NewTabURLDetails::ForProfile(profile));
  UMA_HISTOGRAM_ENUMERATION("NewTabPage.URLState",
                            details.state, NEW_TAB_URL_MAX);
  if (details.url.is_valid()) {
    *url = details.url;
    return true;
  }
  return false;
}

bool HandleNewTabURLReverseRewrite(GURL* url,
                                   content::BrowserContext* browser_context) {
  if (!IsInstantExtendedAPIEnabled())
    return false;

  // Do nothing in incognito.
  Profile* profile = Profile::FromBrowserContext(browser_context);
  if (profile && profile->IsOffTheRecord())
    return false;

  if (MatchesOriginAndPath(GURL(chrome::kChromeSearchLocalNtpUrl), *url)) {
    *url = GURL(chrome::kChromeUINewTabURL);
    return true;
  }

  GURL new_tab_url(GetNewTabPageURL(profile));
  if (new_tab_url.is_valid() && MatchesOriginAndPath(new_tab_url, *url)) {
    *url = GURL(chrome::kChromeUINewTabURL);
    return true;
  }

  return false;
}

void SetInstantSupportStateInNavigationEntry(InstantSupportState state,
                                             content::NavigationEntry* entry) {
  if (!entry)
    return;

  entry->SetExtraData(kInstantSupportStateKey,
                      InstantSupportStateToString(state));
}

InstantSupportState GetInstantSupportStateFromNavigationEntry(
    const content::NavigationEntry& entry) {
  base::string16 value;
  if (!entry.GetExtraData(kInstantSupportStateKey, &value))
    return INSTANT_SUPPORT_UNKNOWN;

  return StringToInstantSupportState(value);
}

bool ShouldPrefetchSearchResultsOnSRP() {
  FieldTrialFlags flags;
  return GetFieldTrialInfo(&flags) && GetBoolValueForFlagWithDefault(
      kPrefetchSearchResultsOnSRP, false, flags);
}

bool ShouldUseAltInstantURL() {
  FieldTrialFlags flags;
  return GetFieldTrialInfo(&flags) && GetBoolValueForFlagWithDefault(
      kUseAltInstantURL, false, flags);
}

bool ShouldUseSearchPathForInstant() {
  FieldTrialFlags flags;
  return GetFieldTrialInfo(&flags) && GetBoolValueForFlagWithDefault(
      kUseSearchPathForInstant, false, flags);
}

}  // namespace search
