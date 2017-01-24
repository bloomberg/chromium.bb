// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search/search.h"

#include <stddef.h>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram_macros.h"
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
#include "chrome/common/features.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/search/search_urls.h"
#include "chrome/common/url_constants.h"
#include "components/google/core/browser/google_util.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/search/search.h"
#include "components/search_engines/search_engine_type.h"
#include "components/search_engines/template_url_service.h"
#include "components/sessions/core/serialized_navigation_entry.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"

#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
#include "chrome/browser/supervised_user/supervised_user_service.h"
#include "chrome/browser/supervised_user/supervised_user_service_factory.h"
#include "chrome/browser/supervised_user/supervised_user_url_filter.h"
#endif

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/login/signin/merge_session_throttling_utils.h"
#endif  // defined(OS_CHROMEOS)

namespace search {

namespace {

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

base::Feature kUseGoogleLocalNtp {
  "UseGoogleLocalNtp", base::FEATURE_DISABLED_BY_DEFAULT
};

TemplateURL* GetDefaultSearchProviderTemplateURL(Profile* profile) {
  if (profile) {
    TemplateURLService* template_url_service =
        TemplateURLServiceFactory::GetForProfile(profile);
    if (template_url_service)
      return template_url_service->GetDefaultSearchProvider();
  }
  return NULL;
}

bool DefaultSearchProviderIsGoogle(Profile* profile) {
  TemplateURLService* template_url_service =
      TemplateURLServiceFactory::GetForProfile(profile);
  if (!template_url_service)
    return false;
  const TemplateURL* default_provider =
      template_url_service->GetDefaultSearchProvider();
  if (!default_provider)
    return false;
  return default_provider->GetEngineType(
             template_url_service->search_terms_data()) ==
         SearchEngineType::SEARCH_ENGINE_GOOGLE;
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

// |url| should either have a secure scheme or have a non-HTTPS base URL that
// the user specified using --google-base-url. (This allows testers to use
// --google-base-url to point at non-HTTPS servers, which eases testing.)
bool IsSuitableURLForInstant(const GURL& url, const TemplateURL* template_url) {
  return template_url->HasSearchTermsReplacementKey(url) &&
         (url.SchemeIsCryptographic() ||
          google_util::StartsWithCommandLineGoogleBaseURL(url));
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

  return MatchesOriginAndPath(url, instant_url);
}

bool IsURLAllowedForSupervisedUser(const GURL& url, Profile* profile) {
#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
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

bool ShouldShowLocalNewTab(const GURL& url, Profile* profile) {
#if defined(OS_CHROMEOS)
  // On Chrome OS, if the session hasn't merged yet, we need to avoid loading
  // the remote NTP because that will trigger showing the merge session throttle
  // interstitial page, which can show for 5+ seconds. crbug.com/591530.
  if (merge_session_throttling_utils::ShouldDelayUrl(url) &&
      merge_session_throttling_utils::IsSessionRestorePending(profile)) {
    return true;
  }
#endif  // defined(OS_CHROMEOS)

  if (!profile->IsOffTheRecord() &&
      base::FeatureList::IsEnabled(kUseGoogleLocalNtp) &&
      DefaultSearchProviderIsGoogle(profile)) {
    return true;
  }

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
         (url.host_piece() == chrome::kChromeSearchLocalNtpHost ||
          url.host_piece() == chrome::kChromeSearchRemoteNtpHost);
}

bool IsNTPURL(const GURL& url, Profile* profile) {
  if (!url.is_valid())
    return false;

  if (!IsInstantExtendedAPIEnabled())
    return url == chrome::kChromeUINewTabURL;

  const base::string16 search_terms = ExtractSearchTermsFromURL(profile, url);
  return profile && ((IsInstantURL(url, profile) && search_terms.empty()) ||
                     url == chrome::kChromeSearchLocalNtpUrl);
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

  return IsInstantNTPURL(entry->GetURL(), profile);
}

bool IsInstantNTPURL(const GURL& url, Profile* profile) {
  if (!IsInstantExtendedAPIEnabled())
    return false;

  if (url == chrome::kChromeSearchLocalNtpUrl)
    return true;

  GURL new_tab_url(GetNewTabPageURL(profile));
  return new_tab_url.is_valid() && MatchesOriginAndPath(url, new_tab_url);
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
  return GetInstantURL(profile, true);
}

GURL GetEffectiveURLForInstant(const GURL& url, Profile* profile) {
  CHECK(ShouldAssignURLToInstantRenderer(url, profile))
      << "Error granting Instant access.";

  if (url.SchemeIs(chrome::kChromeSearchScheme))
    return url;

  // Replace the scheme with "chrome-search:", and clear the port, since
  // chrome-search is a scheme without port.
  url::Replacements<char> replacements;
  std::string search_scheme(chrome::kChromeSearchScheme);
  replacements.SetScheme(search_scheme.data(),
                         url::Component(0, search_scheme.length()));
  replacements.ClearPort();

  // If this is the URL for a server-provided NTP, replace the host with
  // "remote-ntp".
  std::string remote_ntp_host(chrome::kChromeSearchRemoteNtpHost);
  NewTabURLDetails details = NewTabURLDetails::ForProfile(profile);
  if (details.state == NEW_TAB_URL_VALID &&
      MatchesOriginAndPath(url, details.url)) {
    replacements.SetHost(remote_ntp_host.c_str(),
                         url::Component(0, remote_ntp_host.length()));
  }

  return url.ReplaceComponents(replacements);
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

  if (IsInstantNTPURL(*url, profile)) {
    *url = GURL(chrome::kChromeUINewTabURL);
    return true;
  }

  return false;
}

}  // namespace search
