// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/search/search_tab_helper.h"

#include <set>

#include "base/memory/scoped_ptr.h"
#include "base/metrics/histogram.h"
#include "base/strings/string16.h"
#include "base/strings/string_util.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/instant_service.h"
#include "chrome/browser/search/instant_service_factory.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/ui/app_list/app_list_util.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/location_bar/location_bar.h"
#include "chrome/browser/ui/omnibox/omnibox_edit_model.h"
#include "chrome/browser/ui/omnibox/omnibox_popup_model.h"
#include "chrome/browser/ui/omnibox/omnibox_view.h"
#include "chrome/browser/ui/search/instant_search_prerenderer.h"
#include "chrome/browser/ui/search/search_ipc_router_policy_impl.h"
#include "chrome/browser/ui/search/search_tab_helper_delegate.h"
#include "chrome/browser/ui/tab_contents/core_tab_helper.h"
#include "chrome/browser/ui/webui/ntp/ntp_user_data_logger.h"
#include "chrome/common/url_constants.h"
#include "components/search/search.h"
#include "components/signin/core/browser/signin_manager.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_type.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/page_transition_types.h"
#include "content/public/common/referrer.h"
#include "grit/generated_resources.h"
#include "net/base/net_errors.h"
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
      contents->GetController().GetLastCommittedEntry();
  return chrome::NavEntryIsInstantNTP(contents, entry) &&
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
  if (!contents)
    return false;
  const content::NavigationEntry* entry =
      contents->GetController().GetVisibleEntry();
  return entry && entry->GetURL() == GURL(chrome::kChromeSearchLocalNtpUrl);
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

// Called when an NTP finishes loading. If the load start time was noted,
// calculates and logs the total load time.
void RecordNewTabLoadTime(content::WebContents* contents) {
  CoreTabHelper* core_tab_helper = CoreTabHelper::FromWebContents(contents);
  if (core_tab_helper->new_tab_start_time().is_null())
    return;

  base::TimeDelta duration =
      base::TimeTicks::Now() - core_tab_helper->new_tab_start_time();
  UMA_HISTOGRAM_TIMES("Tab.NewTabOnload", duration);
  core_tab_helper->set_new_tab_start_time(base::TimeTicks());
}

// Returns true if the user is signed in and full history sync is enabled,
// and false otherwise.
bool IsHistorySyncEnabled(Profile* profile) {
  ProfileSyncService* sync =
      ProfileSyncServiceFactory::GetInstance()->GetForProfile(profile);
  return sync &&
      sync->sync_initialized() &&
      sync->GetActiveDataTypes().Has(syncer::HISTORY_DELETE_DIRECTIVES);
}

bool OmniboxHasFocus(OmniboxView* omnibox) {
  return omnibox && omnibox->model()->has_focus();
}

}  // namespace

SearchTabHelper::SearchTabHelper(content::WebContents* web_contents)
    : WebContentsObserver(web_contents),
      is_search_enabled_(chrome::IsInstantExtendedAPIEnabled()),
      web_contents_(web_contents),
      ipc_router_(web_contents, this,
                  make_scoped_ptr(new SearchIPCRouterPolicyImpl(web_contents))
                      .PassAs<SearchIPCRouter::Policy>()),
      instant_service_(NULL),
      delegate_(NULL),
      omnibox_has_focus_fn_(&OmniboxHasFocus) {
  if (!is_search_enabled_)
    return;

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

void SearchTabHelper::OmniboxInputStateChanged() {
  if (!is_search_enabled_)
    return;

  UpdateMode(false, false);
}

void SearchTabHelper::OmniboxFocusChanged(OmniboxFocusState state,
                                          OmniboxFocusChangeReason reason) {
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_OMNIBOX_FOCUS_CHANGED,
      content::Source<SearchTabHelper>(this),
      content::NotificationService::NoDetails());

  ipc_router_.OmniboxFocusChanged(state, reason);

  // Don't send oninputstart/oninputend updates in response to focus changes
  // if there's a navigation in progress. This prevents Chrome from sending
  // a spurious oninputend when the user accepts a match in the omnibox.
  if (web_contents_->GetController().GetPendingEntry() == NULL) {
    ipc_router_.SetInputInProgress(IsInputInProgress());

    InstantSearchPrerenderer* prerenderer =
        InstantSearchPrerenderer::GetForProfile(profile());
    if (!prerenderer || !chrome::ShouldPrerenderInstantUrlOnOmniboxFocus())
      return;

    if (state == OMNIBOX_FOCUS_NONE) {
      prerenderer->Cancel();
      return;
    }

    if (!IsSearchResultsPage()) {
      prerenderer->Init(
          web_contents_->GetController().GetSessionStorageNamespaceMap(),
          web_contents_->GetContainerBounds().size());
    }
  }
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
      web_contents_->GetController().GetLastCommittedEntry();
  if (entry) {
    chrome::SetInstantSupportStateInNavigationEntry(new_state, entry);
    if (delegate_ && !instant_support)
      delegate_->OnWebContentsInstantSupportDisabled(web_contents_);
  }
}

bool SearchTabHelper::SupportsInstant() const {
  return model_.instant_support() == INSTANT_SUPPORT_YES;
}

void SearchTabHelper::SetSuggestionToPrefetch(
    const InstantSuggestion& suggestion) {
  ipc_router_.SetSuggestionToPrefetch(suggestion);
}

void SearchTabHelper::Submit(const base::string16& text) {
  ipc_router_.Submit(text);
}

void SearchTabHelper::OnTabActivated() {
  ipc_router_.OnTabActivated();

  OmniboxView* omnibox_view = GetOmniboxView();
  if (chrome::ShouldPrerenderInstantUrlOnOmniboxFocus() &&
      omnibox_has_focus_fn_(omnibox_view)) {
    InstantSearchPrerenderer* prerenderer =
        InstantSearchPrerenderer::GetForProfile(profile());
    if (prerenderer && !IsSearchResultsPage()) {
      prerenderer->Init(
          web_contents_->GetController().GetSessionStorageNamespaceMap(),
          web_contents_->GetContainerBounds().size());
    }
  }
}

void SearchTabHelper::OnTabDeactivated() {
  ipc_router_.OnTabDeactivated();
}

void SearchTabHelper::ToggleVoiceSearch() {
  ipc_router_.ToggleVoiceSearch();
}

bool SearchTabHelper::IsSearchResultsPage() {
  return model_.mode().is_origin_search();
}

void SearchTabHelper::RenderViewCreated(
    content::RenderViewHost* render_view_host) {
  ipc_router_.SetPromoInformation(IsAppLauncherEnabled());
}

void SearchTabHelper::DidStartNavigationToPendingEntry(
    const GURL& url,
    content::NavigationController::ReloadType /* reload_type */) {
  if (chrome::IsNTPURL(url, profile())) {
    // Set the title on any pending entry corresponding to the NTP. This
    // prevents any flickering of the tab title.
    content::NavigationEntry* entry =
        web_contents_->GetController().GetPendingEntry();
    if (entry)
      entry->SetTitle(l10n_util::GetStringUTF16(IDS_NEW_TAB_TITLE));
  }
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
      web_contents_->GetController().GetLastCommittedEntry();
  if (entry && entry->GetTitle().empty() &&
      (entry->GetVirtualURL() == GURL(chrome::kChromeUINewTabURL) ||
       chrome::NavEntryIsInstantNTP(web_contents_, entry))) {
    entry->SetTitle(l10n_util::GetStringUTF16(IDS_NEW_TAB_TITLE));
  }
}

void SearchTabHelper::DidFailProvisionalLoad(
    content::RenderFrameHost* render_frame_host,
    const GURL& validated_url,
    int error_code,
    const base::string16& /* error_description */) {
  // If error_code is ERR_ABORTED means that the user has canceled this
  // navigation so it shouldn't be redirected.
  if (!render_frame_host->GetParent() && error_code != net::ERR_ABORTED &&
      validated_url != GURL(chrome::kChromeSearchLocalNtpUrl) &&
      chrome::IsNTPURL(validated_url, profile())) {
    RedirectToLocalNTP();
    RecordCacheableNTPLoadHistogram(false);
  }
}

void SearchTabHelper::DidFinishLoad(content::RenderFrameHost* render_frame_host,
                                    const GURL& /* validated_url */) {
  if (!render_frame_host->GetParent()) {
    if (chrome::IsInstantNTP(web_contents_))
      RecordNewTabLoadTime(web_contents_);

    DetermineIfPageSupportsInstant();
  }
}

void SearchTabHelper::NavigationEntryCommitted(
    const content::LoadCommittedDetails& load_details) {
  if (!is_search_enabled_)
    return;

  if (!load_details.is_main_frame)
    return;

  if (chrome::ShouldAssignURLToInstantRenderer(web_contents_->GetURL(),
                                               profile())) {
    InstantService* instant_service =
        InstantServiceFactory::GetForProfile(profile());
    ipc_router_.SetOmniboxStartMargin(instant_service->omnibox_start_margin());
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
  if (load_details.is_in_page ||
      load_details.type == content::NAVIGATION_TYPE_IN_PAGE) {
    // When an "in-page" navigation happens, we will not receive a
    // DidFinishLoad() event. Therefore, we will not determine the Instant
    // support for the navigated page. So, copy over the Instant support from
    // the previous entry. If the page does not support Instant, update the
    // location bar from here to turn off search terms replacement.
    chrome::SetInstantSupportStateInNavigationEntry(model_.instant_support(),
                                                    entry);
    if (delegate_ && model_.instant_support() == INSTANT_SUPPORT_NO)
      delegate_->OnWebContentsInstantSupportDisabled(web_contents_);
    return;
  }

  model_.SetInstantSupportState(INSTANT_SUPPORT_UNKNOWN);
  model_.SetVoiceSearchSupported(false);
  chrome::SetInstantSupportStateInNavigationEntry(model_.instant_support(),
                                                  entry);

  if (InInstantProcess(profile(), web_contents_))
    ipc_router_.OnNavigationEntryCommitted();
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
  ipc_router_.SendMostVisitedItems(items);
}

void SearchTabHelper::OmniboxStartMarginChanged(int omnibox_start_margin) {
  ipc_router_.SetOmniboxStartMargin(omnibox_start_margin);
}

void SearchTabHelper::FocusOmnibox(OmniboxFocusState state) {
// TODO(kmadhusu): Move platform specific code from here and get rid of #ifdef.
#if !defined(OS_ANDROID)
  OmniboxView* omnibox = GetOmniboxView();
  if (!omnibox)
    return;

  // Do not add a default case in the switch block for the following reasons:
  // (1) Explicitly handle the new states. If new states are added in the
  // OmniboxFocusState, the compiler will warn the developer to handle the new
  // states.
  // (2) An attacker may control the renderer and sends the browser process a
  // malformed IPC. This function responds to the invalid |state| values by
  // doing nothing instead of crashing the browser process (intentional no-op).
  switch (state) {
    case OMNIBOX_FOCUS_VISIBLE:
      omnibox->SetFocus();
      omnibox->model()->SetCaretVisibility(true);
      break;
    case OMNIBOX_FOCUS_INVISIBLE:
      omnibox->SetFocus();
      omnibox->model()->SetCaretVisibility(false);
      // If the user clicked on the fakebox, any text already in the omnibox
      // should get cleared when they start typing. Selecting all the existing
      // text is a convenient way to accomplish this. It also gives a slight
      // visual cue to users who really understand selection state about what
      // will happen if they start typing.
      omnibox->SelectAll(false);
      omnibox->ShowImeIfNeeded();
      break;
    case OMNIBOX_FOCUS_NONE:
      // Remove focus only if the popup is closed. This will prevent someone
      // from changing the omnibox value and closing the popup without user
      // interaction.
      if (!omnibox->model()->popup_model()->IsOpen())
        web_contents()->Focus();
      break;
  }
#endif
}

void SearchTabHelper::NavigateToURL(const GURL& url,
                                    WindowOpenDisposition disposition,
                                    bool is_most_visited_item_url) {
  if (is_most_visited_item_url) {
    content::RecordAction(
        base::UserMetricsAction("InstantExtended.MostVisitedClicked"));
  }

  if (delegate_)
    delegate_->NavigateOnThumbnailClick(url, disposition, web_contents_);
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

void SearchTabHelper::OnLogEvent(NTPLoggingEventType event) {
// TODO(kmadhusu): Move platform specific code from here and get rid of #ifdef.
#if !defined(OS_ANDROID)
  NTPUserDataLogger::GetOrCreateFromWebContents(
      web_contents())->LogEvent(event);
#endif
}

void SearchTabHelper::OnLogMostVisitedImpression(
    int position, const base::string16& provider) {
// TODO(kmadhusu): Move platform specific code from here and get rid of #ifdef.
#if !defined(OS_ANDROID)
  NTPUserDataLogger::GetOrCreateFromWebContents(
      web_contents())->LogMostVisitedImpression(position, provider);
#endif
}

void SearchTabHelper::OnLogMostVisitedNavigation(
    int position, const base::string16& provider) {
// TODO(kmadhusu): Move platform specific code from here and get rid of #ifdef.
#if !defined(OS_ANDROID)
  NTPUserDataLogger::GetOrCreateFromWebContents(
      web_contents())->LogMostVisitedNavigation(position, provider);
#endif
}

void SearchTabHelper::PasteIntoOmnibox(const base::string16& text) {
// TODO(kmadhusu): Move platform specific code from here and get rid of #ifdef.
#if !defined(OS_ANDROID)
  OmniboxView* omnibox = GetOmniboxView();
  if (!omnibox)
    return;
  // The first case is for right click to paste, where the text is retrieved
  // from the clipboard already sanitized. The second case is needed to handle
  // drag-and-drop value and it has to be sanitazed before setting it into the
  // omnibox.
  base::string16 text_to_paste = text.empty() ? omnibox->GetClipboardText() :
      omnibox->SanitizeTextForPaste(text);

  if (text_to_paste.empty())
    return;

  if (!omnibox->model()->has_focus())
    omnibox->SetFocus();

  omnibox->OnBeforePossibleChange();
  omnibox->model()->OnPaste();
  omnibox->SetUserText(text_to_paste);
  omnibox->OnAfterPossibleChange();
#endif
}

void SearchTabHelper::OnChromeIdentityCheck(const base::string16& identity) {
  SigninManagerBase* manager = SigninManagerFactory::GetForProfile(profile());
  if (manager) {
    const base::string16 username =
        base::UTF8ToUTF16(manager->GetAuthenticatedUsername());
    // The identity check only passes if the user is syncing their history.
    // TODO(beaudoin): Change this function name and related APIs now that it's
    // checking both the identity and the user's sync state.
    bool matches = IsHistorySyncEnabled(profile()) && identity == username;
    ipc_router_.SendChromeIdentityCheckResult(identity, matches);
  }
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

  OmniboxView* omnibox = GetOmniboxView();
  if (omnibox && omnibox->model()->user_input_in_progress())
    type = SearchMode::MODE_SEARCH_SUGGESTIONS;

  SearchMode old_mode(model_.mode());
  model_.SetMode(SearchMode(type, origin));
  if (old_mode.is_ntp() != model_.mode().is_ntp()) {
    ipc_router_.SetInputInProgress(IsInputInProgress());
  }
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

bool SearchTabHelper::IsInputInProgress() const {
  OmniboxView* omnibox = GetOmniboxView();
  return !model_.mode().is_ntp() && omnibox &&
      omnibox->model()->focus_state() == OMNIBOX_FOCUS_VISIBLE;
}

OmniboxView* SearchTabHelper::GetOmniboxView() const {
  return delegate_ ? delegate_->GetOmniboxView() : NULL;
}
