// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search/search.h"

#include "base/command_line.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram.h"
#include "base/prefs/pref_service.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/google/google_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/search/instant_service.h"
#include "chrome/browser/search/instant_service_factory.h"
#include "chrome/browser/search_engines/template_url_service.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_instant_controller.h"
#include "chrome/browser/ui/browser_iterator.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/search_urls.h"
#include "chrome/common/url_constants.h"
#include "components/sessions/serialized_navigation_entry.h"
#include "components/user_prefs/pref_registry_syncable.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

#if defined(ENABLE_MANAGED_USERS)
#include "chrome/browser/managed_mode/managed_mode_url_filter.h"
#include "chrome/browser/managed_mode/managed_user_service.h"
#include "chrome/browser/managed_mode/managed_user_service_factory.h"
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
const uint64 kEmbeddedPageVersionDisabled = 0;
#if defined(OS_IOS) || defined(OS_ANDROID)
const uint64 kEmbeddedPageVersionDefault = 1;
#else
const uint64 kEmbeddedPageVersionDefault = 2;
#endif

// The staleness timeout can be set (in seconds) via this config.
const char kStalePageTimeoutFlagName[] = "stale";
const int kStalePageTimeoutDefault = 3 * 3600;  // 3 hours.

const char kHideVerbatimFlagName[] = "hide_verbatim";
const char kUseRemoteNTPOnStartupFlagName[] = "use_remote_ntp_on_startup";
const char kShowNtpFlagName[] = "show_ntp";
const char kRecentTabsOnNTPFlagName[] = "show_recent_tabs";
const char kUseCacheableNTP[] = "use_cacheable_ntp";
const char kPrefetchSearchResultsOnSRP[] = "prefetch_results_srp";
const char kSuppressInstantExtendedOnSRPFlagName[] = "suppress_on_srp";

// Constants for the field trial name and group prefix.
// Note in M30 and below this field trial was named "InstantExtended" and in
// M31 was renamed to EmbeddedSearch for clarity and cleanliness.  Since we
// can't easilly sync up Finch configs with the pushing of this change to
// Dev & Canary, for now the code accepts both names.
// TODO(dcblack): Remove the InstantExtended name once M31 hits the Beta
// channel.
const char kInstantExtendedFieldTrialName[] = "InstantExtended";
const char kEmbeddedSearchFieldTrialName[] = "EmbeddedSearch";
const char kGroupNumberPrefix[] = "Group";

// If the field trial's group name ends with this string its configuration will
// be ignored and Instant Extended will not be enabled by default.
const char kDisablingSuffix[] = "DISABLED";

// Remember if we reported metrics about opt-in/out state.
bool instant_extended_opt_in_state_gate = false;

// Used to set the Instant support state of the Navigation entry.
const char kInstantSupportStateKey[] = "instant_support_state";

const char kInstantSupportEnabled[] = "Instant support enabled";
const char kInstantSupportDisabled[] = "Instant support disabled";
const char kInstantSupportUnknown[] = "Instant support unknown";

InstantSupportState StringToInstantSupportState(const string16& value) {
  if (value == ASCIIToUTF16(kInstantSupportEnabled))
    return INSTANT_SUPPORT_YES;
  else if (value == ASCIIToUTF16(kInstantSupportDisabled))
    return INSTANT_SUPPORT_NO;
  else
    return INSTANT_SUPPORT_UNKNOWN;
}

string16 InstantSupportStateToString(InstantSupportState state) {
  switch (state) {
    case INSTANT_SUPPORT_NO:
      return ASCIIToUTF16(kInstantSupportDisabled);
    case INSTANT_SUPPORT_YES:
      return ASCIIToUTF16(kInstantSupportEnabled);
    case INSTANT_SUPPORT_UNKNOWN:
      return ASCIIToUTF16(kInstantSupportUnknown);
  }
  return ASCIIToUTF16(kInstantSupportUnknown);
}

TemplateURL* GetDefaultSearchProviderTemplateURL(Profile* profile) {
  TemplateURLService* template_url_service =
      TemplateURLServiceFactory::GetForProfile(profile);
  if (template_url_service)
    return template_url_service->GetDefaultSearchProvider();
  return NULL;
}

GURL TemplateURLRefToGURL(const TemplateURLRef& ref,
                          int start_margin,
                          bool append_extra_query_params,
                          bool force_instant_results) {
  TemplateURLRef::SearchTermsArgs search_terms_args =
      TemplateURLRef::SearchTermsArgs(string16());
  search_terms_args.omnibox_start_margin = start_margin;
  search_terms_args.append_extra_query_params = append_extra_query_params;
  search_terms_args.force_instant_results = force_instant_results;
  return GURL(ref.ReplaceSearchTerms(search_terms_args));
}

bool MatchesAnySearchURL(const GURL& url, TemplateURL* template_url) {
  GURL search_url =
      TemplateURLRefToGURL(template_url->url_ref(), kDisableStartMargin, false,
                           false);
  if (search_url.is_valid() &&
      search::MatchesOriginAndPath(url, search_url))
    return true;

  // "URLCount() - 1" because we already tested url_ref above.
  for (size_t i = 0; i < template_url->URLCount() - 1; ++i) {
    TemplateURLRef ref(template_url, i);
    search_url = TemplateURLRefToGURL(ref, kDisableStartMargin, false, false);
    if (search_url.is_valid() &&
        search::MatchesOriginAndPath(url, search_url))
      return true;
  }

  return false;
}

void RecordInstantExtendedOptInState() {
  if (instant_extended_opt_in_state_gate)
    return;

  instant_extended_opt_in_state_gate = true;
  OptInState state = INSTANT_EXTENDED_NOT_SET;
  const CommandLine* command_line = CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kDisableInstantExtendedAPI))
    state = INSTANT_EXTENDED_OPT_OUT;
  else if (command_line->HasSwitch(switches::kEnableInstantExtendedAPI))
    state = INSTANT_EXTENDED_OPT_IN;

  UMA_HISTOGRAM_ENUMERATION("InstantExtended.NewOptInState", state,
                            INSTANT_EXTENDED_OPT_IN_STATE_ENUM_COUNT);
}

// Returns true if |contents| is rendered inside the Instant process for
// |profile|.
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

// Returns true if |url| passes some basic checks that must succeed for it to be
// usable as an instant URL:
// (1) It contains the search terms replacement key of |template_url|, which is
//     expected to be the TemplateURL* for the default search provider.
// (2) Either it has a secure scheme, or else the user has manually specified a
//     --google-base-url and it uses that base URL.  (This allows testers to use
//     --google-base-url to point at non-HTTPS servers, which eases testing.)
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
  const GURL instant_url =
      TemplateURLRefToGURL(instant_url_ref, kDisableStartMargin, false, false);
  if (!instant_url.is_valid())
    return false;

  if (search::MatchesOriginAndPath(url, instant_url))
    return true;

  return !ShouldSuppressInstantExtendedOnSRP() &&
      MatchesAnySearchURL(url, template_url);
}

string16 GetSearchTermsImpl(const content::WebContents* contents,
                            const content::NavigationEntry* entry) {
  if (!contents || !IsQueryExtractionEnabled())
    return string16();

  // For security reasons, don't extract search terms if the page is not being
  // rendered in the privileged Instant renderer process. This is to protect
  // against a malicious page somehow scripting the search results page and
  // faking search terms in the URL. Random pages can't get into the Instant
  // renderer and scripting doesn't work cross-process, so if the page is in
  // the Instant process, we know it isn't being exploited.
  // Since iOS and Android doesn't use the instant framework, these checks are
  // disabled for the two platforms.
  Profile* profile = Profile::FromBrowserContext(contents->GetBrowserContext());
#if !defined(OS_IOS) && !defined(OS_ANDROID)
  if (!IsRenderedInInstantProcess(contents, profile) &&
      ((entry == contents->GetController().GetLastCommittedEntry()) ||
       !ShouldAssignURLToInstantRenderer(entry->GetURL(), profile)))
    return string16();
#endif  // !defined(OS_IOS) && !defined(OS_ANDROID)
  // Check to see if search terms have already been extracted.
  string16 search_terms = GetSearchTermsFromNavigationEntry(entry);
  if (!search_terms.empty())
    return search_terms;

  // Otherwise, extract from the URL.
  return GetSearchTermsFromURL(profile, entry->GetVirtualURL());
}

bool IsURLAllowedForSupervisedUser(const GURL& url, Profile* profile) {
#if defined(ENABLE_MANAGED_USERS)
  ManagedUserService* managed_user_service =
      ManagedUserServiceFactory::GetForProfile(profile);
  ManagedModeURLFilter* url_filter =
      managed_user_service->GetURLFilterForUIThread();
  if (url_filter->GetFilteringBehaviorForURL(url) ==
          ManagedModeURLFilter::BLOCK) {
    return false;
  }
#endif
  return true;
}

}  // namespace

// Negative start-margin values prevent the "es_sm" parameter from being used.
const int kDisableStartMargin = -1;

bool IsInstantExtendedAPIEnabled() {
#if defined(OS_IOS) || defined(OS_ANDROID)
  return false;
#else
  RecordInstantExtendedOptInState();
  return EmbeddedSearchPageVersion() != kEmbeddedPageVersionDisabled;
#endif  // defined(OS_IOS) || defined(OS_ANDROID)
}

// Determine what embedded search page version to request from the user's
// default search provider. If 0, the embedded search UI should not be enabled.
uint64 EmbeddedSearchPageVersion() {
  RecordInstantExtendedOptInState();

  // Check the command-line/about:flags setting first, which should have
  // precedence and allows the trial to not be reported (if it's never queried).
  const CommandLine* command_line = CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kDisableInstantExtendedAPI))
    return kEmbeddedPageVersionDisabled;
  if (command_line->HasSwitch(switches::kEnableInstantExtendedAPI)) {
    // The user has set the about:flags switch to Enabled - give the default
    // UI version.
    return kEmbeddedPageVersionDefault;
  }

  FieldTrialFlags flags;
  uint64 group_num = 0;
  if (GetFieldTrialInfo(&flags, &group_num)) {
    if (group_num == 0)
      return kEmbeddedPageVersionDisabled;
    return GetUInt64ValueForFlagWithDefault(kEmbeddedPageVersionFlagName,
                                            kEmbeddedPageVersionDefault,
                                            flags);
  }
  return kEmbeddedPageVersionDisabled;
}

bool IsQueryExtractionEnabled() {
  return EmbeddedSearchPageVersion() != kEmbeddedPageVersionDisabled &&
      !ShouldSuppressInstantExtendedOnSRP();
}

string16 GetSearchTermsFromURL(Profile* profile, const GURL& url) {
  string16 search_terms;
  TemplateURL* template_url = GetDefaultSearchProviderTemplateURL(profile);
  if (template_url && IsSuitableURLForInstant(url, template_url))
    template_url->ExtractSearchTermsFromURL(url, &search_terms);
  return search_terms;
}

string16 GetSearchTermsFromNavigationEntry(
    const content::NavigationEntry* entry) {
  string16 search_terms;
  if (entry)
    entry->GetExtraData(sessions::kSearchTermsKey, &search_terms);
  return search_terms;
}

string16 GetSearchTerms(const content::WebContents* contents) {
  if (!contents)
    return string16();

  const content::NavigationEntry* entry =
      contents->GetController().GetVisibleEntry();
  if (!entry)
    return string16();

#if !defined(OS_IOS) && !defined(OS_ANDROID)
  // iOS and Android doesn't use the Instant framework, disable this check for
  // the two platforms.
  InstantSupportState state = GetInstantSupportStateFromNavigationEntry(*entry);
  if (state == INSTANT_SUPPORT_NO)
    return string16();
#endif  // !defined(OS_IOS) && !defined(OS_ANDROID)

  return GetSearchTermsImpl(contents, entry);
}

bool ShouldAssignURLToInstantRenderer(const GURL& url, Profile* profile) {
  return url.is_valid() &&
         profile &&
         IsInstantExtendedAPIEnabled() &&
         (url.SchemeIs(chrome::kChromeSearchScheme) ||
          IsInstantURL(url, profile));
}

bool ShouldUseProcessPerSiteForInstantURL(const GURL& url, Profile* profile) {
  return ShouldAssignURLToInstantRenderer(url, profile) &&
      (url.host() == chrome::kChromeSearchLocalNtpHost ||
       url.host() == chrome::kChromeSearchOnlineNtpHost);
}

bool IsNTPURL(const GURL& url, Profile* profile) {
  if (!url.is_valid())
    return false;

  if (!IsInstantExtendedAPIEnabled())
    return url == GURL(chrome::kChromeUINewTabURL);

  return profile &&
      ((IsInstantURL(url, profile) &&
        GetSearchTermsFromURL(profile, url).empty()) ||
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

  if (entry->GetVirtualURL() == GetLocalInstantURL(profile))
    return true;

  if (ShouldUseCacheableNTP()) {
    GURL new_tab_url(GetNewTabPageURL(profile));
    return new_tab_url.is_valid() &&
        search::MatchesOriginAndPath(entry->GetURL(), new_tab_url);
  }

  return IsInstantURL(entry->GetVirtualURL(), profile) &&
      GetSearchTermsImpl(contents, entry).empty();
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

  GURL instant_url =
      TemplateURLRefToGURL(template_url->instant_url_ref(), start_margin, true,
                           force_instant_results);
  if (!instant_url.is_valid() ||
      !template_url->HasSearchTermsReplacementKey(instant_url))
    return GURL();

  // Extended mode requires HTTPS.  Force it unless the base URL was overridden
  // on the command line, in which case we allow HTTP (see comments on
  // IsSuitableURLForInstant()).
  if (!instant_url.SchemeIsSecure() &&
      !google_util::StartsWithCommandLineGoogleBaseURL(instant_url)) {
    GURL::Replacements replacements;
    const std::string secure_scheme(content::kHttpsScheme);
    replacements.SetSchemeStr(secure_scheme);
    instant_url = instant_url.ReplaceComponents(replacements);
  }

  if (!IsURLAllowedForSupervisedUser(instant_url, profile))
    return GURL();

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
    result.push_back(TemplateURLRefToGURL(ref, kDisableStartMargin, false,
                                          false));
  }
  return result;
}

GURL GetNewTabPageURL(Profile* profile) {
  if (!ShouldUseCacheableNTP())
    return GURL();

  if (!profile || profile->IsOffTheRecord())
    return GURL();

  if (!IsSuggestPrefEnabled(profile))
    return GURL(chrome::kChromeSearchLocalNtpUrl);

  TemplateURL* template_url = GetDefaultSearchProviderTemplateURL(profile);
  if (!template_url)
    return GURL(chrome::kChromeSearchLocalNtpUrl);

  GURL url(TemplateURLRefToGURL(template_url->new_tab_url_ref(),
                                kDisableStartMargin, false, false));
  if (!url.is_valid() || !url.SchemeIsSecure())
    return GURL(chrome::kChromeSearchLocalNtpUrl);

  if (!IsURLAllowedForSupervisedUser(url, profile))
    return GURL(chrome::kChromeSearchLocalNtpUrl);

  return url;
}

GURL GetLocalInstantURL(Profile* profile) {
  return GURL(chrome::kChromeSearchLocalNtpUrl);
}

bool ShouldPreferRemoteNTPOnStartup() {
  // Check the command-line/about:flags setting first, which should have
  // precedence and allows the trial to not be reported (if it's never queried).
  const CommandLine* command_line = CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kDisableInstantExtendedAPI) ||
      command_line->HasSwitch(switches::kEnableLocalFirstLoadNTP)) {
    return false;
  }
  if (command_line->HasSwitch(switches::kDisableLocalFirstLoadNTP))
    return true;

  FieldTrialFlags flags;
  if (GetFieldTrialInfo(&flags, NULL)) {
    return GetBoolValueForFlagWithDefault(kUseRemoteNTPOnStartupFlagName, true,
                                          flags);
  }
  return false;
}

bool ShouldHideTopVerbatimMatch() {
  FieldTrialFlags flags;
  if (GetFieldTrialInfo(&flags, NULL)) {
    return GetBoolValueForFlagWithDefault(kHideVerbatimFlagName, false, flags);
  }
  return false;
}

bool ShouldUseCacheableNTP() {
  const CommandLine* command_line = CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kUseCacheableNewTabPage))
    return true;

  FieldTrialFlags flags;
  if (GetFieldTrialInfo(&flags, NULL)) {
    return GetBoolValueForFlagWithDefault(kUseCacheableNTP, false, flags);
  }
  return false;
}

bool ShouldShowInstantNTP() {
  // If using the cacheable NTP, load the NTP directly instead of preloading its
  // contents using InstantNTP.
  if (ShouldUseCacheableNTP())
    return false;

  FieldTrialFlags flags;
  if (GetFieldTrialInfo(&flags, NULL)) {
    return GetBoolValueForFlagWithDefault(kShowNtpFlagName, true, flags);
  }
  return true;
}

bool ShouldShowRecentTabsOnNTP() {
  FieldTrialFlags flags;
  if (GetFieldTrialInfo(&flags, NULL)) {
    return GetBoolValueForFlagWithDefault(
        kRecentTabsOnNTPFlagName, false, flags);
  }

  return false;
}

bool ShouldSuppressInstantExtendedOnSRP() {
  FieldTrialFlags flags;
  if (GetFieldTrialInfo(&flags, NULL)) {
    return GetBoolValueForFlagWithDefault(
        kSuppressInstantExtendedOnSRPFlagName, false, flags);
  }

  return false;
}

GURL GetEffectiveURLForInstant(const GURL& url, Profile* profile) {
  CHECK(ShouldAssignURLToInstantRenderer(url, profile))
      << "Error granting Instant access.";

  if (url.SchemeIs(chrome::kChromeSearchScheme))
    return url;

  GURL effective_url(url);

  // Replace the scheme with "chrome-search:".
  url_canon::Replacements<char> replacements;
  std::string search_scheme(chrome::kChromeSearchScheme);
  replacements.SetScheme(search_scheme.data(),
                         url_parse::Component(0, search_scheme.length()));

  // If the URL corresponds to an online NTP, replace the host with
  // "online-ntp".
  std::string online_ntp_host(chrome::kChromeSearchOnlineNtpHost);
  TemplateURL* template_url = GetDefaultSearchProviderTemplateURL(profile);
  if (template_url) {
    const GURL instant_url = TemplateURLRefToGURL(
        template_url->instant_url_ref(), kDisableStartMargin, false, false);
    if (instant_url.is_valid() &&
        search::MatchesOriginAndPath(url, instant_url)) {
      replacements.SetHost(online_ntp_host.c_str(),
                           url_parse::Component(0, online_ntp_host.length()));
    }
  }

  effective_url = effective_url.ReplaceComponents(replacements);
  return effective_url;
}

int GetInstantLoaderStalenessTimeoutSec() {
  int timeout_sec = kStalePageTimeoutDefault;
  FieldTrialFlags flags;
  if (GetFieldTrialInfo(&flags, NULL)) {
    timeout_sec = GetUInt64ValueForFlagWithDefault(kStalePageTimeoutFlagName,
                                                   kStalePageTimeoutDefault,
                                                   flags);
  }

  // Require a minimum 5 minute timeout.
  if (timeout_sec < 0 || (timeout_sec > 0 && timeout_sec < 300))
    timeout_sec = kStalePageTimeoutDefault;

  // Randomize by upto 15% either side.
  timeout_sec = base::RandInt(timeout_sec * 0.85, timeout_sec * 1.15);

  return timeout_sec;
}

bool IsPreloadedInstantExtendedNTP(const content::WebContents* contents) {
  if (!IsInstantExtendedAPIEnabled())
    return false;

  ProfileManager* profile_manager = g_browser_process->profile_manager();
  if (!profile_manager)
    return false;  // The profile manager can be NULL while testing.

  const std::vector<Profile*>& profiles = profile_manager->GetLoadedProfiles();
  for (size_t i = 0; i < profiles.size(); ++i) {
    const InstantService* instant_service =
        InstantServiceFactory::GetForProfile(profiles[i]);
    if (instant_service && instant_service->GetNTPContents() == contents)
      return true;
  }
  return false;
}

bool HandleNewTabURLRewrite(GURL* url,
                            content::BrowserContext* browser_context) {
  if (!IsInstantExtendedAPIEnabled())
    return false;

  if (!url->SchemeIs(chrome::kChromeUIScheme) ||
      url->host() != chrome::kChromeUINewTabHost)
    return false;

  Profile* profile = Profile::FromBrowserContext(browser_context);
  GURL new_tab_url(GetNewTabPageURL(profile));
  if (!new_tab_url.is_valid())
    return false;

  *url = new_tab_url;
  return true;
}

bool HandleNewTabURLReverseRewrite(GURL* url,
                                   content::BrowserContext* browser_context) {
  if (!IsInstantExtendedAPIEnabled())
    return false;

  Profile* profile = Profile::FromBrowserContext(browser_context);
  GURL new_tab_url(GetNewTabPageURL(profile));
  if (!new_tab_url.is_valid() ||
      !search::MatchesOriginAndPath(new_tab_url, *url))
    return false;

  *url = GURL(chrome::kChromeUINewTabURL);
  return true;
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
  string16 value;
  if (!entry.GetExtraData(kInstantSupportStateKey, &value))
    return INSTANT_SUPPORT_UNKNOWN;

  return StringToInstantSupportState(value);
}

bool ShouldPrefetchSearchResultsOnSRP() {
  // Check the command-line/about:flags setting first, which should have
  // precedence and allows the trial to not be reported (if it's never queried).
  const CommandLine* command_line = CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kDisableInstantExtendedAPI) ||
      command_line->HasSwitch(switches::kEnableInstantExtendedAPI)) {
    return false;
  }

  FieldTrialFlags flags;
  if (GetFieldTrialInfo(&flags, NULL)) {
    return GetBoolValueForFlagWithDefault(kPrefetchSearchResultsOnSRP, false,
                                          flags);
  }
  return false;
}

void EnableInstantExtendedAPIForTesting() {
  CommandLine* cl = CommandLine::ForCurrentProcess();
  cl->AppendSwitch(switches::kEnableInstantExtendedAPI);
}

void DisableInstantExtendedAPIForTesting() {
  CommandLine* cl = CommandLine::ForCurrentProcess();
  cl->AppendSwitch(switches::kDisableInstantExtendedAPI);
}

bool GetFieldTrialInfo(FieldTrialFlags* flags,
                       uint64* group_number) {
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

  // We have a valid trial that isn't disabled.
  // First extract the flags.
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

  // Now extract the group number, making sure we get a non-zero value.
  uint64 temp_group_number = 0;
  if (StartsWithASCII(group_name, kGroupNumberPrefix, true)) {
    std::string group_suffix = group_prefix.substr(strlen(kGroupNumberPrefix));
    if (!base::StringToUint64(group_suffix, &temp_group_number))
      return false;
    if (group_number)
      *group_number = temp_group_number;
  } else {
    // Instant Extended is not enabled.
    if (group_number)
      *group_number = 0;
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

void ResetInstantExtendedOptInStateGateForTest() {
  instant_extended_opt_in_state_gate = false;
}

}  // namespace chrome
