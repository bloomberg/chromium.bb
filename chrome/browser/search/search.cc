// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search/search.h"

#include <stddef.h>
#include <string>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_macros.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/search_engines/ui_thread_search_terms_data.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/search/search_urls.h"
#include "chrome/common/url_constants.h"
#include "components/google/core/browser/google_util.h"
#include "components/prefs/pref_service.h"
#include "components/search/search.h"
#include "components/search_engines/search_engine_type.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_frame_host.h"
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

#if !defined(OS_ANDROID)
#include "chrome/browser/search/instant_service.h"
#include "chrome/browser/search/instant_service_factory.h"
#endif

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

const TemplateURL* GetDefaultSearchProviderTemplateURL(Profile* profile) {
  if (profile) {
    TemplateURLService* template_url_service =
        TemplateURLServiceFactory::GetForProfile(profile);
    if (template_url_service)
      return template_url_service->GetDefaultSearchProvider();
  }
  return nullptr;
}

// Returns true if |url| matches the NTP URL or the URL of the NTP's associated
// service worker.
bool IsNTPOrServiceWorkerURL(const GURL& url, Profile* profile) {
  if (!url.is_valid())
    return false;

  const GURL new_tab_url(GetNewTabPageURL(profile));
  return new_tab_url.is_valid() && (MatchesOriginAndPath(url, new_tab_url) ||
                                    IsMatchingServiceWorker(url, new_tab_url));
}

bool IsURLAllowedForSupervisedUser(const GURL& url, Profile* profile) {
#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
  // If this isn't a supervised user, skip the URL filter check, since it can be
  // fairly expensive.
  if (!profile->IsSupervised())
    return true;
  SupervisedUserService* supervised_user_service =
      SupervisedUserServiceFactory::GetForProfile(profile);
  SupervisedUserURLFilter* url_filter = supervised_user_service->GetURLFilter();
  if (url_filter->GetFilteringBehaviorForURL(url) ==
          SupervisedUserURLFilter::BLOCK) {
    return false;
  }
#endif
  return true;
}

bool ShouldShowLocalNewTab(Profile* profile) {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  return command_line->HasSwitch(switches::kForceLocalNtp) ||
         (base::FeatureList::IsEnabled(features::kUseGoogleLocalNtp) &&
          profile && DefaultSearchProviderIsGoogle(profile));
}

bool ShouldDelayRemoteNTP(const GURL& search_provider_url, Profile* profile) {
#if defined(OS_CHROMEOS)
  // On Chrome OS, if the session hasn't merged yet, we need to avoid loading
  // the remote NTP because that will trigger showing the merge session throttle
  // interstitial page, which can show for 5+ seconds. crbug.com/591530.
  if (merge_session_throttling_utils::ShouldDelayUrl(search_provider_url) &&
      merge_session_throttling_utils::IsSessionRestorePending(profile)) {
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
    // Incognito has its own New Tab.
    if (profile->IsOffTheRecord())
      return NewTabURLDetails(GURL(), NEW_TAB_URL_INCOGNITO);

    const GURL local_url(chrome::kChromeSearchLocalNtpUrl);

    if (ShouldShowLocalNewTab(profile))
      return NewTabURLDetails(local_url, NEW_TAB_URL_VALID);

    const TemplateURL* template_url =
        GetDefaultSearchProviderTemplateURL(profile);
    if (!profile || !template_url)
      return NewTabURLDetails(local_url, NEW_TAB_URL_BAD);

    GURL search_provider_url(template_url->new_tab_url_ref().ReplaceSearchTerms(
        TemplateURLRef::SearchTermsArgs(base::string16()),
        UIThreadSearchTermsData(profile)));

    if (ShouldDelayRemoteNTP(search_provider_url, profile))
      return NewTabURLDetails(local_url, NEW_TAB_URL_VALID);

    if (!search_provider_url.is_valid())
      return NewTabURLDetails(local_url, NEW_TAB_URL_NOT_SET);
    if (!search_provider_url.SchemeIsCryptographic())
      return NewTabURLDetails(local_url, NEW_TAB_URL_INSECURE);
    if (!IsURLAllowedForSupervisedUser(search_provider_url, profile))
      return NewTabURLDetails(local_url, NEW_TAB_URL_BLOCKED);

    return NewTabURLDetails(search_provider_url, NEW_TAB_URL_VALID);
  }

  const GURL url;
  const NewTabURLState state;
};

}  // namespace

bool ShouldAssignURLToInstantRenderer(const GURL& url, Profile* profile) {
  return url.is_valid() && profile && IsInstantExtendedAPIEnabled() &&
         (url.SchemeIs(chrome::kChromeSearchScheme) ||
          IsNTPOrServiceWorkerURL(url, profile));
}

bool IsRenderedInInstantProcess(const content::WebContents* contents,
                                Profile* profile) {
#if defined(OS_ANDROID)
  return false;
#else
  const content::RenderProcessHost* process_host =
      contents->GetMainFrame()->GetProcess();
  if (!process_host)
    return false;

  const InstantService* instant_service =
      InstantServiceFactory::GetForProfile(profile);
  if (!instant_service)
    return false;

  return instant_service->IsInstantProcess(process_host->GetID());
#endif
}

bool ShouldUseProcessPerSiteForInstantURL(const GURL& url, Profile* profile) {
  return ShouldAssignURLToInstantRenderer(url, profile) &&
         (url.host_piece() == chrome::kChromeSearchLocalNtpHost ||
          url.host_piece() == chrome::kChromeSearchRemoteNtpHost);
}

bool DefaultSearchProviderIsGoogle(Profile* profile) {
  return DefaultSearchProviderIsGoogle(
      TemplateURLServiceFactory::GetForProfile(profile));
}

bool DefaultSearchProviderIsGoogle(
    const TemplateURLService* template_url_service) {
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

bool IsNTPURL(const GURL& url, Profile* profile) {
  if (!url.is_valid())
    return false;

  if (!IsInstantExtendedAPIEnabled())
    return url == chrome::kChromeUINewTabURL;

  // TODO(treib,sfiera): Tolerate query params when detecting local NTPs.
  return profile && (IsNTPOrServiceWorkerURL(url, profile) ||
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

  // TODO(treib,sfiera): Tolerate query params when detecting local NTPs.
  if (url == chrome::kChromeSearchLocalNtpUrl)
    return true;

  GURL new_tab_url(GetNewTabPageURL(profile));
  return new_tab_url.is_valid() && MatchesOriginAndPath(url, new_tab_url);
}

bool IsSuggestPrefEnabled(Profile* profile) {
  return profile && !profile->IsOffTheRecord() && profile->GetPrefs() &&
         profile->GetPrefs()->GetBoolean(prefs::kSearchSuggestEnabled);
}

GURL GetNewTabPageURL(Profile* profile) {
  return NewTabURLDetails::ForProfile(profile).url;
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
      (MatchesOriginAndPath(url, details.url) ||
       IsMatchingServiceWorker(url, details.url))) {
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
