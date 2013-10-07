// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/search/search_tab_helper.h"

#include <set>

#include "base/memory/scoped_ptr.h"
#include "base/metrics/histogram.h"
#include "build/build_config.h"
#include "chrome/browser/apps/app_launcher_util.h"
#include "chrome/browser/history/most_visited_tiles_experiment.h"
#include "chrome/browser/history/top_sites.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/instant_service.h"
#include "chrome/browser/search/instant_service_factory.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/search/search_ipc_router_policy_impl.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model_utils.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_type.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/page_transition_types.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

DEFINE_WEB_CONTENTS_USER_DATA_KEY(SearchTabHelper);

namespace {

// For reporting Cacheable NTP navigations.
enum CacheableNTPLoad {
  CACHEABLE_NTP_LOAD_FAILED = 0,
  CACHEABLE_NTP_LOAD_SUCCEEDED = 1,
  CACHEABLE_NTP_LOAD_MAX = 2
};

void RecordCacheableNTPLoadHistogram(bool succeeded) {
  UMA_HISTOGRAM_ENUMERATION("InstantExtended.CacheableNTPLoad",
                            succeeded ? CACHEABLE_NTP_LOAD_SUCCEEDED :
                                CACHEABLE_NTP_LOAD_FAILED,
                            CACHEABLE_NTP_LOAD_MAX);
}

bool IsCacheableNTP(const content::WebContents* contents) {
  const content::NavigationEntry* entry =
      contents->GetController().GetActiveEntry();
  return chrome::ShouldUseCacheableNTP() &&
      chrome::NavEntryIsInstantNTP(contents, entry) &&
      entry->GetURL() != GURL(chrome::kChromeSearchLocalNtpUrl);
}

bool IsNTP(const content::WebContents* contents) {
  // We can't use WebContents::GetURL() because that uses the active entry,
  // whereas we want the visible entry.
  const content::NavigationEntry* entry =
      contents->GetController().GetVisibleEntry();
  if (entry && entry->GetVirtualURL() == GURL(chrome::kChromeUINewTabURL))
    return true;

  return chrome::IsInstantNTP(contents);
}

bool IsSearchResults(const content::WebContents* contents) {
  return !chrome::GetSearchTerms(contents).empty();
}

bool IsLocal(const content::WebContents* contents) {
  return contents &&
      contents->GetURL() == GURL(chrome::kChromeSearchLocalNtpUrl);
}

// Returns true if |contents| are rendered inside an Instant process.
bool InInstantProcess(Profile* profile,
                      const content::WebContents* contents) {
  if (!profile || !contents)
    return false;

  InstantService* instant_service =
      InstantServiceFactory::GetForProfile(profile);
  return instant_service &&
      instant_service->IsInstantProcess(
          contents->GetRenderProcessHost()->GetID());
}

// Updates the location bar to reflect |contents| Instant support state.
void UpdateLocationBar(content::WebContents* contents) {
// iOS and Android doesn't use the Instant framework.
#if !defined(OS_IOS) && !defined(OS_ANDROID)
  if (!contents)
    return;

  Browser* browser = chrome::FindBrowserWithWebContents(contents);
  if (!browser)
    return;
  browser->OnWebContentsInstantSupportDisabled(contents);
#endif
}

}  // namespace

SearchTabHelper::SearchTabHelper(content::WebContents* web_contents)
    : WebContentsObserver(web_contents),
      is_search_enabled_(chrome::IsInstantExtendedAPIEnabled()),
      user_input_in_progress_(false),
      web_contents_(web_contents),
      ipc_router_(web_contents, this,
                  make_scoped_ptr(new SearchIPCRouterPolicyImpl(web_contents))
                      .PassAs<SearchIPCRouter::Policy>()),
      instant_service_(NULL) {
  if (!is_search_enabled_)
    return;

  // Observe NOTIFICATION_NAV_ENTRY_COMMITTED events so we can reset state
  // associated with the WebContents  (such as mode, last known most visited
  // items, instant support state etc).
  registrar_.Add(
      this,
      content::NOTIFICATION_NAV_ENTRY_COMMITTED,
      content::Source<content::NavigationController>(
          &web_contents->GetController()));

  instant_service_ =
      InstantServiceFactory::GetForProfile(
          Profile::FromBrowserContext(web_contents_->GetBrowserContext()));
  if (instant_service_)
    instant_service_->AddObserver(this);
}

SearchTabHelper::~SearchTabHelper() {
  if (instant_service_)
    instant_service_->RemoveObserver(this);
}

void SearchTabHelper::InitForPreloadedNTP() {
  UpdateMode(true, true);
}

void SearchTabHelper::OmniboxEditModelChanged(bool user_input_in_progress,
                                              bool cancelling) {
  if (!is_search_enabled_)
    return;

  user_input_in_progress_ = user_input_in_progress;
  if (!user_input_in_progress && !cancelling)
    return;

  UpdateMode(false, false);
}

void SearchTabHelper::NavigationEntryUpdated() {
  if (!is_search_enabled_)
    return;

  UpdateMode(false, false);
}

void SearchTabHelper::InstantSupportChanged(bool instant_support) {
  if (!is_search_enabled_)
    return;

  InstantSupportState new_state = instant_support ? INSTANT_SUPPORT_YES :
      INSTANT_SUPPORT_NO;

  model_.SetInstantSupportState(new_state);

  content::NavigationEntry* entry =
      web_contents_->GetController().GetVisibleEntry();
  if (entry) {
    chrome::SetInstantSupportStateInNavigationEntry(new_state, entry);
    if (!instant_support)
      UpdateLocationBar(web_contents_);
  }
}

bool SearchTabHelper::SupportsInstant() const {
  return model_.instant_support() == INSTANT_SUPPORT_YES;
}

void SearchTabHelper::SetSuggestionToPrefetch(
    const InstantSuggestion& suggestion) {
  ipc_router_.SetSuggestionToPrefetch(suggestion);
}

void SearchTabHelper::Submit(const string16& text) {
  DCHECK(!chrome::IsInstantNTP(web_contents_));
  ipc_router_.Submit(text);
}

void SearchTabHelper::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK_EQ(content::NOTIFICATION_NAV_ENTRY_COMMITTED, type);
  content::LoadCommittedDetails* load_details =
      content::Details<content::LoadCommittedDetails>(details).ptr();
  if (!load_details->is_main_frame)
    return;

  // TODO(kmadhusu): Set the page initial states (such as omnibox margin, etc)
  // from here. Please refer to crbug.com/247517 for more details.
  if (chrome::ShouldAssignURLToInstantRenderer(web_contents_->GetURL(),
                                               profile())) {
    ipc_router_.SetDisplayInstantResults();
  }

  UpdateMode(true, false);

  content::NavigationEntry* entry =
      web_contents_->GetController().GetVisibleEntry();
  DCHECK(entry);

  // Already determined the instant support state for this page, do not reset
  // the instant support state.
  //
  // When we get a navigation entry committed event, there seem to be two ways
  // to tell whether the navigation was "in-page". Ideally, when
  // LoadCommittedDetails::is_in_page is true, we should have
  // LoadCommittedDetails::type to be NAVIGATION_TYPE_IN_PAGE. Unfortunately,
  // they are different in some cases. To workaround this bug, we are checking
  // (is_in_page || type == NAVIGATION_TYPE_IN_PAGE). Please refer to
  // crbug.com/251330 for more details.
  if (load_details->is_in_page ||
      load_details->type == content::NAVIGATION_TYPE_IN_PAGE) {
    // When an "in-page" navigation happens, we will not receive a
    // DidFinishLoad() event. Therefore, we will not determine the Instant
    // support for the navigated page. So, copy over the Instant support from
    // the previous entry. If the page does not support Instant, update the
    // location bar from here to turn off search terms replacement.
    chrome::SetInstantSupportStateInNavigationEntry(model_.instant_support(),
                                                    entry);
    if (model_.instant_support() == INSTANT_SUPPORT_NO)
      UpdateLocationBar(web_contents_);
    return;
  }

  model_.SetInstantSupportState(INSTANT_SUPPORT_UNKNOWN);
  model_.SetVoiceSearchSupported(false);
  chrome::SetInstantSupportStateInNavigationEntry(model_.instant_support(),
                                                  entry);
}

void SearchTabHelper::RenderViewCreated(
    content::RenderViewHost* render_view_host) {
  ipc_router_.SetPromoInformation(IsAppLauncherEnabled());
}

void SearchTabHelper::DidNavigateMainFrame(
    const content::LoadCommittedDetails& details,
    const content::FrameNavigateParams& params) {
  if (IsCacheableNTP(web_contents_)) {
    if (details.http_status_code == 204 || details.http_status_code >= 400) {
      RedirectToLocalNTP();
      RecordCacheableNTPLoadHistogram(false);
      return;
    }
    RecordCacheableNTPLoadHistogram(true);
  }

  // Always set the title on the new tab page to be the one from our UI
  // resources. Normally, we set the title when we begin a NTP load, but it can
  // get reset in several places (like when you press Reload). This check
  // ensures that the title is properly set to the string defined by the Chrome
  // UI language (rather than the server language) in all cases.
  //
  // We only override the title when it's nonempty to allow the page to set the
  // title if it really wants. An empty title means to use the default. There's
  // also a race condition between this code and the page's SetTitle call which
  // this rule avoids.
  content::NavigationEntry* entry =
      web_contents_->GetController().GetActiveEntry();
  if (entry && entry->GetTitle().empty() &&
      (entry->GetVirtualURL() == GURL(chrome::kChromeUINewTabURL) ||
       chrome::NavEntryIsInstantNTP(web_contents_, entry))) {
    entry->SetTitle(l10n_util::GetStringUTF16(IDS_NEW_TAB_TITLE));
  }
}

void SearchTabHelper::DidFailProvisionalLoad(
    int64 /* frame_id */,
    bool is_main_frame,
    const GURL& /* validated_url */,
    int /* error_code */,
    const string16& /* error_description */,
    content::RenderViewHost* /* render_view_host */) {
  if (is_main_frame && IsCacheableNTP(web_contents_)) {
    RedirectToLocalNTP();
    RecordCacheableNTPLoadHistogram(false);
  }
}

void SearchTabHelper::DidFinishLoad(
    int64 /* frame_id */,
    const GURL&  /* validated_url */,
    bool is_main_frame,
    content::RenderViewHost* /* render_view_host */) {
  if (is_main_frame)
    DetermineIfPageSupportsInstant();
}

void SearchTabHelper::OnInstantSupportDetermined(bool supports_instant) {
  InstantSupportChanged(supports_instant);
}

void SearchTabHelper::OnSetVoiceSearchSupport(bool supports_voice_search) {
  model_.SetVoiceSearchSupported(supports_voice_search);
}

void SearchTabHelper::ThemeInfoChanged(const ThemeBackgroundInfo& theme_info) {
  ipc_router_.SendThemeBackgroundInfo(theme_info);
}

void SearchTabHelper::MostVisitedItemsChanged(
    const std::vector<InstantMostVisitedItem>& items) {
  std::vector<InstantMostVisitedItem> items_copy(items);
  MaybeRemoveMostVisitedItems(&items_copy);
  ipc_router_.SendMostVisitedItems(items_copy);
}

void SearchTabHelper::MaybeRemoveMostVisitedItems(
    std::vector<InstantMostVisitedItem>* items) {
// The code below uses APIs not available on Android and the experiment should
// not run there.
#if !defined(OS_ANDROID)
  if (!history::MostVisitedTilesExperiment::IsDontShowOpenURLsEnabled())
    return;

  Profile* profile =
      Profile::FromBrowserContext(web_contents_->GetBrowserContext());
  if (!profile)
    return;

  Browser* browser = chrome::FindBrowserWithProfile(profile,
                                                    chrome::GetActiveDesktop());
  if (!browser)
    return;

  TabStripModel* tab_strip_model = browser->tab_strip_model();
  history::TopSites* top_sites = profile->GetTopSites();
  if (!tab_strip_model || !top_sites) {
    NOTREACHED();
    return;
  }

  std::set<std::string> open_urls;
  chrome::GetOpenUrls(*tab_strip_model, *top_sites, &open_urls);
  history::MostVisitedTilesExperiment::RemoveItemsMatchingOpenTabs(
      open_urls, items);
#endif
}

void SearchTabHelper::OnDeleteMostVisitedItem(const GURL& url) {
  DCHECK(!url.is_empty());
  if (instant_service_)
    instant_service_->DeleteMostVisitedItem(url);
}

void SearchTabHelper::OnUndoMostVisitedDeletion(const GURL& url) {
  DCHECK(!url.is_empty());
  if (instant_service_)
    instant_service_->UndoMostVisitedDeletion(url);
}

void SearchTabHelper::OnUndoAllMostVisitedDeletions() {
  if (instant_service_)
    instant_service_->UndoAllMostVisitedDeletions();
}

void SearchTabHelper::UpdateMode(bool update_origin, bool is_preloaded_ntp) {
  SearchMode::Type type = SearchMode::MODE_DEFAULT;
  SearchMode::Origin origin = SearchMode::ORIGIN_DEFAULT;
  if (IsNTP(web_contents_) || is_preloaded_ntp) {
    type = SearchMode::MODE_NTP;
    origin = SearchMode::ORIGIN_NTP;
  } else if (IsSearchResults(web_contents_)) {
    type = SearchMode::MODE_SEARCH_RESULTS;
    origin = SearchMode::ORIGIN_SEARCH;
  }
  if (!update_origin)
    origin = model_.mode().origin;
  if (user_input_in_progress_)
    type = SearchMode::MODE_SEARCH_SUGGESTIONS;
  model_.SetMode(SearchMode(type, origin));
}

void SearchTabHelper::DetermineIfPageSupportsInstant() {
  if (!InInstantProcess(profile(), web_contents_)) {
    // The page is not in the Instant process. This page does not support
    // instant. If we send an IPC message to a page that is not in the Instant
    // process, it will never receive it and will never respond. Therefore,
    // return immediately.
    InstantSupportChanged(false);
  } else if (IsLocal(web_contents_)) {
    // Local pages always support Instant.
    InstantSupportChanged(true);
  } else {
    ipc_router_.DetermineIfPageSupportsInstant();
  }
}

Profile* SearchTabHelper::profile() const {
  return Profile::FromBrowserContext(web_contents_->GetBrowserContext());
}

void SearchTabHelper::RedirectToLocalNTP() {
  // Extra parentheses to declare a variable.
  content::NavigationController::LoadURLParams load_params(
      (GURL(chrome::kChromeSearchLocalNtpUrl)));
  load_params.referrer = content::Referrer();
  load_params.transition_type = content::PAGE_TRANSITION_SERVER_REDIRECT;
  // Don't push a history entry.
  load_params.should_replace_current_entry = true;
  web_contents_->GetController().LoadURLWithParams(load_params);
}
