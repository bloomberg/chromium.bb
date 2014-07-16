// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search/search.h"

#include "base/command_line.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram.h"
#include "base/prefs/pref_service.h"
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
#include "chrome/browser/ui/browser_iterator.h"
#include "chrome/browser/ui/search/instant_search_prerenderer.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/search_urls.h"
#include "chrome/common/url_constants.h"
#include "components/google/core/browser/google_util.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/search_engines/template_url_service.h"
#include "components/sessions/serialized_navigation_entry.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

#if defined(ENABLE_MANAGED_USERS)
#include "chrome/browser/supervised_user/supervised_user_service.h"
#include "chrome/browser/supervised_user/supervised_user_service_factory.h"
#include "chrome/browser/supervised_user/supervised_user_url_filter.h"
#endif

namespace chrome {

namespace {

// Configuration options for Embedded Search.
// EmbeddedSearch field trials are named in such a way that we can parse out
// the experiment configuration from the trial's group name in order to give
// us maximum flexability in running experiments.
// Field trial groups should be named things like "Group7 espv:2 instant:1".
// The first token is always GroupN for some integer N, followed by a
// space-delimited list of key:value pairs which correspond to these flags:
const char kEmbeddedPageVersionFlagName[] = "espv";
#if defined(OS_IOS)
const uint64 kEmbeddedPageVersionDefault = 1;
#elif defined(OS_ANDROID)
const uint64 kEmbeddedPageVersionDefault = 1;
// Use this variant to enable EmbeddedSearch SearchBox API in the results page.
const uint64 kEmbeddedSearchEnabledVersion = 2;
#else
const uint64 kEmbeddedPageVersionDefault = 2;
#endif

const char kHideVerbatimFlagName[] = "hide_verbatim";
const char kPrefetchSearchResultsOnSRP[] = "prefetch_results_srp";
const char kAllowPrefetchNonDefaultMatch[] = "allow_prefetch_non_default_match";
const char kPrerenderInstantUrlOnOmniboxFocus[] =
    "prerender_instant_url_on_omnibox_focus";

#if defined(OS_ANDROID)
const char kPrefetchSearchResultsFlagName[] = "prefetch_results";

// Controls whether to reuse prerendered Instant Search base page to commit any
// search query.
const char kReuseInstantSearchBasePage[] = "reuse_instant_search_base_page";
#endif

// Controls whether to use the alternate Instant search base URL. This allows
// experimentation of Instant search.
const char kUseAltInstantURL[] = "use_alternate_instant_url";
const char kAltInstantURLPath[] = "search";
const char kAltInstantURLQueryParams[] = "&qbp=1";

const char kDisplaySearchButtonFlagName[] = "display_search_button";
const char kOriginChipFlagName[] = "origin_chip";
#if !defined(OS_IOS) && !defined(OS_ANDROID)
const char kEnableQueryExtractionFlagName[] = "query_extraction";
#endif
const char kShouldShowGoogleLocalNTPFlagName[] = "google_local_ntp";

// Constants for the field trial name and group prefix.
// Note in M30 and below this field trial was named "InstantExtended" and in
// M31 was renamed to EmbeddedSearch for clarity and cleanliness.  Since we
// can't easilly sync up Finch configs with the pushing of this change to
// Dev & Canary, for now the code accepts both names.
// TODO(dcblack): Remove the InstantExtended name once M31 hits the Beta
// channel.
const char kInstantExtendedFieldTrialName[] = "InstantExtended";
const char kEmbeddedSearchFieldTrialName[] = "EmbeddedSearch";

// If the field trial's group name ends with this string its configuration will
// be ignored and Instant Extended will not be enabled by default.
const char kDisablingSuffix[] = "DISABLED";

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
                          int start_margin,
                          bool append_extra_query_params,
                          bool force_instant_results) {
  TemplateURLRef::SearchTermsArgs search_terms_args =
      TemplateURLRef::SearchTermsArgs(base::string16());
  search_terms_args.omnibox_start_margin = start_margin;
  search_terms_args.append_extra_query_params = append_extra_query_params;
  search_terms_args.force_instant_results = force_instant_results;
  return GURL(ref.ReplaceSearchTerms(search_terms_args, search_terms_data));
}

bool MatchesAnySearchURL(const GURL& url,
                         TemplateURL* template_url,
                         const SearchTermsData& search_terms_data) {
  GURL search_url =
      TemplateURLRefToGURL(template_url->url_ref(), search_terms_data,
                           kDisableStartMargin, false, false);
  if (search_url.is_valid() &&
      search::MatchesOriginAndPath(url, search_url))
    return true;

  // "URLCount() - 1" because we already tested url_ref above.
  for (size_t i = 0; i < template_url->URLCount() - 1; ++i) {
    TemplateURLRef ref(template_url, i);
    search_url = TemplateURLRefToGURL(ref, search_terms_data,
                                      kDisableStartMargin, false, false);
    if (search_url.is_valid() &&
        search::MatchesOriginAndPath(url, search_url))
      return true;
  }

  return false;
}



// |url| should either have a secure scheme or have a non-HTTPS base URL that
// the user specified using --google-base-url. (This allows testers to use
// --google-base-url to point at non-HTTPS servers, which eases testing.)
bool IsSuitableURLForInstant(const GURL& url, const TemplateURL* template_url) {
  return template_url->HasSearchTermsReplacementKey(url) &&
      (url.SchemeIsSecure() ||
       google_util::StartsWithCommandLineGoogleBaseURL(url));
}

// Returns true if |url| can be used as an Instant URL for |profile|.
bool IsInstantURL(const GURL& url, Profile* profile) {
  if (!IsInstantExtendedAPIEnabled())
    return false;

  if (!url.is_valid())
    return false;

  const GURL new_tab_url(GetNewTabPageURL(profile));
  if (new_tab_url.is_valid() &&
      search::MatchesOriginAndPath(url, new_tab_url))
    return true;

  TemplateURL* template_url = GetDefaultSearchProviderTemplateURL(profile);
  if (!template_url)
    return false;

  if (!IsSuitableURLForInstant(url, template_url))
    return false;

  const TemplateURLRef& instant_url_ref = template_url->instant_url_ref();
  UIThreadSearchTermsData search_terms_data(profile);
  const GURL instant_url = TemplateURLRefToGURL(
      instant_url_ref, search_terms_data, kDisableStartMargin, false, false);
  if (!instant_url.is_valid())
    return false;

  if (search::MatchesOriginAndPath(url, instant_url))
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
#if defined(ENABLE_MANAGED_USERS)
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
  if (!new_tab_url.SchemeIsSecure())
    return NEW_TAB_URL_INSECURE;
  if (!IsURLAllowedForSupervisedUser(new_tab_url, profile))
    return NEW_TAB_URL_BLOCKED;
  return NEW_TAB_URL_VALID;
}

// Used to look up the URL to use for the New Tab page. Also tracks how we
// arrived at that URL so it can be logged with UMA.
struct NewTabURLDetails {
  NewTabURLDetails(const GURL& url, NewTabURLState state)
      : url(url), state(state) {}

  static NewTabURLDetails ForProfile(Profile* profile) {
    const GURL local_url(chrome::kChromeSearchLocalNtpUrl);
    TemplateURL* template_url = GetDefaultSearchProviderTemplateURL(profile);
    if (!profile || !template_url)
      return NewTabURLDetails(local_url, NEW_TAB_URL_BAD);

    GURL search_provider_url = TemplateURLRefToGURL(
        template_url->new_tab_url_ref(), UIThreadSearchTermsData(profile),
        kDisableStartMargin, false, false);
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

// Negative start-margin values prevent the "es_sm" parameter from being used.
const int kDisableStartMargin = -1;

bool IsInstantExtendedAPIEnabled() {
#if defined(OS_IOS)
  return false;
#elif defined(OS_ANDROID)
  return EmbeddedSearchPageVersion() == kEmbeddedSearchEnabledVersion;
#else
  return true;
#endif  // defined(OS_IOS)
}

// Determine what embedded search page version to request from the user's
// default search provider. If 0, the embedded search UI should not be enabled.
uint64 EmbeddedSearchPageVersion() {
#if defined(OS_ANDROID)
  if (CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableEmbeddedSearchAPI)) {
    return kEmbeddedSearchEnabledVersion;
  }
#endif

  FieldTrialFlags flags;
  if (GetFieldTrialInfo(&flags)) {
    return GetUInt64ValueForFlagWithDefault(kEmbeddedPageVersionFlagName,
                                            kEmbeddedPageVersionDefault,
                                            flags);
  }
  return kEmbeddedPageVersionDefault;
}

std::string InstantExtendedEnabledParam(bool for_search) {
  if (for_search && !chrome::IsQueryExtractionEnabled())
    return std::string();
  return std::string(google_util::kInstantExtendedAPIParam) + "=" +
      base::Uint64ToString(EmbeddedSearchPageVersion()) + "&";
}

std::string ForceInstantResultsParam(bool for_prerender) {
  return (for_prerender || !IsInstantExtendedAPIEnabled()) ?
      "ion=1&" : std::string();
}

bool IsQueryExtractionEnabled() {
#if defined(OS_IOS) || defined(OS_ANDROID)
  return true;
#else
  if (!IsInstantExtendedAPIEnabled())
    return false;

  const CommandLine* command_line = CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kEnableQueryExtraction))
    return true;

  FieldTrialFlags flags;
  return GetFieldTrialInfo(&flags) && GetBoolValueForFlagWithDefault(
      kEnableQueryExtractionFlagName, false, flags);
#endif  // defined(OS_IOS) || defined(OS_ANDROID)
}

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

  if (entry->GetURL() == GetLocalInstantURL(profile))
    return true;

  GURL new_tab_url(GetNewTabPageURL(profile));
  return new_tab_url.is_valid() &&
      search::MatchesOriginAndPath(entry->GetURL(), new_tab_url);
}

bool IsSuggestPrefEnabled(Profile* profile) {
  return profile && !profile->IsOffTheRecord() && profile->GetPrefs() &&
         profile->GetPrefs()->GetBoolean(prefs::kSearchSuggestEnabled);
}

GURL GetInstantURL(Profile* profile, int start_margin,
                   bool force_instant_results) {
  if (!IsInstantExtendedAPIEnabled() || !IsSuggestPrefEnabled(profile))
    return GURL();

  TemplateURL* template_url = GetDefaultSearchProviderTemplateURL(profile);
  if (!template_url)
    return GURL();

  GURL instant_url = TemplateURLRefToGURL(
      template_url->instant_url_ref(), UIThreadSearchTermsData(profile),
      start_margin, true, force_instant_results);
  if (!instant_url.is_valid() ||
      !template_url->HasSearchTermsReplacementKey(instant_url))
    return GURL();

  // Extended mode requires HTTPS.  Force it unless the base URL was overridden
  // on the command line, in which case we allow HTTP (see comments on
  // IsSuitableURLForInstant()).
  if (!instant_url.SchemeIsSecure() &&
      !google_util::StartsWithCommandLineGoogleBaseURL(instant_url)) {
    GURL::Replacements replacements;
    const std::string secure_scheme(url::kHttpsScheme);
    replacements.SetSchemeStr(secure_scheme);
    instant_url = instant_url.ReplaceComponents(replacements);
  }

  if (!IsURLAllowedForSupervisedUser(instant_url, profile))
    return GURL();

  if (ShouldUseAltInstantURL()) {
    GURL::Replacements replacements;
    const std::string path(kAltInstantURLPath);
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
  for (size_t i = 0; i < template_url->URLCount(); ++i) {
    TemplateURLRef ref(template_url, i);
    result.push_back(TemplateURLRefToGURL(ref, UIThreadSearchTermsData(profile),
                                          kDisableStartMargin, false, false));
  }
  return result;
}

GURL GetNewTabPageURL(Profile* profile) {
  return NewTabURLDetails::ForProfile(profile).url;
}

GURL GetSearchResultPrefetchBaseURL(Profile* profile) {
  return ShouldPrefetchSearchResults() ?
      GetInstantURL(profile, kDisableStartMargin, true) : GURL();
}

bool ShouldPrefetchSearchResults() {
  if (!IsInstantExtendedAPIEnabled())
    return false;

#if defined(OS_ANDROID)
  if (CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kPrefetchSearchResults)) {
    return true;
  }

  FieldTrialFlags flags;
  return GetFieldTrialInfo(&flags) && GetBoolValueForFlagWithDefault(
      kPrefetchSearchResultsFlagName, false, flags);
#else
  return true;
#endif
}

bool ShouldAllowPrefetchNonDefaultMatch() {
  if (!ShouldPrefetchSearchResults())
    return false;

  FieldTrialFlags flags;
  return GetFieldTrialInfo(&flags) && GetBoolValueForFlagWithDefault(
      kAllowPrefetchNonDefaultMatch, false, flags);
}

bool ShouldPrerenderInstantUrlOnOmniboxFocus() {
  if (!ShouldPrefetchSearchResults())
    return false;

  FieldTrialFlags flags;
  return GetFieldTrialInfo(&flags) && GetBoolValueForFlagWithDefault(
      kPrerenderInstantUrlOnOmniboxFocus, false, flags);
}

bool ShouldReuseInstantSearchBasePage() {
  if (!ShouldPrefetchSearchResults())
    return false;

#if defined(OS_ANDROID)
  FieldTrialFlags flags;
  return GetFieldTrialInfo(&flags) && GetBoolValueForFlagWithDefault(
      kReuseInstantSearchBasePage, false, flags);
#else
  return true;
#endif
}

GURL GetLocalInstantURL(Profile* profile) {
  return GURL(chrome::kChromeSearchLocalNtpUrl);
}

bool ShouldHideTopVerbatimMatch() {
  FieldTrialFlags flags;
  return GetFieldTrialInfo(&flags) && GetBoolValueForFlagWithDefault(
      kHideVerbatimFlagName, false, flags);
}

DisplaySearchButtonConditions GetDisplaySearchButtonConditions() {
  const CommandLine* cl = CommandLine::ForCurrentProcess();
  if (cl->HasSwitch(switches::kDisableSearchButtonInOmnibox))
    return DISPLAY_SEARCH_BUTTON_NEVER;
  if (cl->HasSwitch(switches::kEnableSearchButtonInOmniboxForStr))
    return DISPLAY_SEARCH_BUTTON_FOR_STR;
  if (cl->HasSwitch(switches::kEnableSearchButtonInOmniboxForStrOrIip))
    return DISPLAY_SEARCH_BUTTON_FOR_STR_OR_IIP;
  if (cl->HasSwitch(switches::kEnableSearchButtonInOmniboxAlways))
    return DISPLAY_SEARCH_BUTTON_ALWAYS;

  FieldTrialFlags flags;
  if (!GetFieldTrialInfo(&flags))
    return DISPLAY_SEARCH_BUTTON_NEVER;
  uint64 value =
      GetUInt64ValueForFlagWithDefault(kDisplaySearchButtonFlagName, 0, flags);
  return (value < DISPLAY_SEARCH_BUTTON_NUM_VALUES) ?
      static_cast<DisplaySearchButtonConditions>(value) :
      DISPLAY_SEARCH_BUTTON_NEVER;
}

bool ShouldDisplayOriginChip() {
  return GetOriginChipCondition() != ORIGIN_CHIP_DISABLED;
}

OriginChipCondition GetOriginChipCondition() {
  const CommandLine* cl = CommandLine::ForCurrentProcess();
  if (cl->HasSwitch(switches::kDisableOriginChip))
    return ORIGIN_CHIP_DISABLED;
  if (cl->HasSwitch(switches::kEnableOriginChipAlways))
    return ORIGIN_CHIP_ALWAYS;
  if (cl->HasSwitch(switches::kEnableOriginChipOnSrp))
    return ORIGIN_CHIP_ON_SRP;

  FieldTrialFlags flags;
  if (!GetFieldTrialInfo(&flags))
    return ORIGIN_CHIP_DISABLED;
  uint64 value =
      GetUInt64ValueForFlagWithDefault(kOriginChipFlagName, 0, flags);
  return (value < ORIGIN_CHIP_NUM_VALUES) ?
      static_cast<OriginChipCondition>(value) : ORIGIN_CHIP_DISABLED;
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
      search::MatchesOriginAndPath(url, details.url)) {
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

  if (search::MatchesOriginAndPath(
      GURL(chrome::kChromeSearchLocalNtpUrl), *url)) {
    *url = GURL(chrome::kChromeUINewTabURL);
    return true;
  }

  GURL new_tab_url(GetNewTabPageURL(profile));
  if (new_tab_url.is_valid() &&
      search::MatchesOriginAndPath(new_tab_url, *url)) {
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

void EnableQueryExtractionForTesting() {
  CommandLine* cl = CommandLine::ForCurrentProcess();
  cl->AppendSwitch(switches::kEnableQueryExtraction);
}

bool GetFieldTrialInfo(FieldTrialFlags* flags) {
  // Get the group name.  If the EmbeddedSearch trial doesn't exist, look for
  // the older InstantExtended name.
  std::string group_name = base::FieldTrialList::FindFullName(
      kEmbeddedSearchFieldTrialName);
  if (group_name.empty()) {
    group_name = base::FieldTrialList::FindFullName(
        kInstantExtendedFieldTrialName);
  }

  if (EndsWith(group_name, kDisablingSuffix, true))
    return false;

  // We have a valid trial that isn't disabled. Extract the flags.
  std::string group_prefix(group_name);
  size_t first_space = group_name.find(" ");
  if (first_space != std::string::npos) {
    // There is a flags section of the group name. Split that out and parse it.
    group_prefix = group_name.substr(0, first_space);
    if (!base::SplitStringIntoKeyValuePairs(group_name.substr(first_space),
                                            ':', ' ', flags)) {
      // Failed to parse the flags section. Assume the whole group name is
      // invalid.
      return false;
    }
  }
  return true;
}

// Given a FieldTrialFlags object, returns the string value of the provided
// flag.
std::string GetStringValueForFlagWithDefault(const std::string& flag,
                                             const std::string& default_value,
                                             const FieldTrialFlags& flags) {
  FieldTrialFlags::const_iterator i;
  for (i = flags.begin(); i != flags.end(); i++) {
    if (i->first == flag)
      return i->second;
  }
  return default_value;
}

// Given a FieldTrialFlags object, returns the uint64 value of the provided
// flag.
uint64 GetUInt64ValueForFlagWithDefault(const std::string& flag,
                                        uint64 default_value,
                                        const FieldTrialFlags& flags) {
  uint64 value;
  std::string str_value =
      GetStringValueForFlagWithDefault(flag, std::string(), flags);
  if (base::StringToUint64(str_value, &value))
    return value;
  return default_value;
}

// Given a FieldTrialFlags object, returns the boolean value of the provided
// flag.
bool GetBoolValueForFlagWithDefault(const std::string& flag,
                                    bool default_value,
                                    const FieldTrialFlags& flags) {
  return !!GetUInt64ValueForFlagWithDefault(flag, default_value ? 1 : 0, flags);
}

bool ShouldUseAltInstantURL() {
  FieldTrialFlags flags;
  return GetFieldTrialInfo(&flags) && GetBoolValueForFlagWithDefault(
      kUseAltInstantURL, false, flags);
}

}  // namespace chrome
