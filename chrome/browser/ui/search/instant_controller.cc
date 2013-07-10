// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/search/instant_controller.h"

#include "base/metrics/histogram.h"
#include "base/prefs/pref_service.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/content_settings/content_settings_provider.h"
#include "chrome/browser/content_settings/host_content_settings_map.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/instant_service.h"
#include "chrome/browser/search/instant_service_factory.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/search_engines/search_terms_data.h"
#include "chrome/browser/search_engines/template_url_service.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/browser_instant_controller.h"
#include "chrome/browser/ui/search/instant_ntp.h"
#include "chrome/browser/ui/search/instant_tab.h"
#include "chrome/browser/ui/search/search_tab_helper.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/content_settings_types.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "components/sessions/serialized_navigation_entry.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_view.h"
#include "net/base/escape.h"
#include "net/base/network_change_notifier.h"

#if defined(TOOLKIT_VIEWS)
#include "ui/views/widget/widget.h"
#endif

namespace {

// For reporting Instant navigations.
enum InstantNavigation {
  INSTANT_NAVIGATION_LOCAL_CLICK = 0,
  INSTANT_NAVIGATION_LOCAL_SUBMIT = 1,
  INSTANT_NAVIGATION_ONLINE_CLICK = 2,
  INSTANT_NAVIGATION_ONLINE_SUBMIT = 3,
  INSTANT_NAVIGATION_NONEXTENDED = 4,
  INSTANT_NAVIGATION_MAX = 5
};

void RecordNavigationHistogram(bool is_local, bool is_click, bool is_extended) {
  InstantNavigation navigation;
  if (!is_extended) {
    navigation = INSTANT_NAVIGATION_NONEXTENDED;
  } else if (is_local) {
    navigation = is_click ? INSTANT_NAVIGATION_LOCAL_CLICK :
                            INSTANT_NAVIGATION_LOCAL_SUBMIT;
  } else {
    navigation = is_click ? INSTANT_NAVIGATION_ONLINE_CLICK :
                            INSTANT_NAVIGATION_ONLINE_SUBMIT;
  }
  UMA_HISTOGRAM_ENUMERATION("InstantExtended.InstantNavigation",
                            navigation,
                            INSTANT_NAVIGATION_MAX);
}

bool IsContentsFrom(const InstantPage* page,
                    const content::WebContents* contents) {
  return page && (page->contents() == contents);
}

// Adds a transient NavigationEntry to the supplied |contents|'s
// NavigationController if the page's URL has not already been updated with the
// supplied |search_terms|. Sets the |search_terms| on the transient entry for
// search terms extraction to work correctly.
void EnsureSearchTermsAreSet(content::WebContents* contents,
                             const string16& search_terms) {
  content::NavigationController* controller = &contents->GetController();

  // If search terms are already correct or there is already a transient entry
  // (there shouldn't be), bail out early.
  if (chrome::GetSearchTerms(contents) == search_terms ||
      controller->GetTransientEntry())
    return;

  const content::NavigationEntry* active_entry = controller->GetActiveEntry();
  content::NavigationEntry* transient = controller->CreateNavigationEntry(
      active_entry->GetURL(),
      active_entry->GetReferrer(),
      active_entry->GetTransitionType(),
      false,
      std::string(),
      contents->GetBrowserContext());
  transient->SetExtraData(sessions::kSearchTermsKey, search_terms);
  controller->SetTransientEntry(transient);

  SearchTabHelper::FromWebContents(contents)->NavigationEntryUpdated();
}

template <typename T>
void DeletePageSoon(scoped_ptr<T> page) {
  if (page->contents()) {
    base::MessageLoop::current()->DeleteSoon(
        FROM_HERE, page->ReleaseContents().release());
  }

  base::MessageLoop::current()->DeleteSoon(FROM_HERE, page.release());
}

}  // namespace

InstantController::InstantController(BrowserInstantController* browser,
                                     bool extended_enabled)
    : browser_(browser),
      extended_enabled_(extended_enabled),
      omnibox_focus_state_(OMNIBOX_FOCUS_NONE),
      omnibox_focus_change_reason_(OMNIBOX_FOCUS_CHANGE_EXPLICIT),
      omnibox_bounds_(-1, -1, 0, 0) {

  // When the InstantController lives, the InstantService should live.
  // InstantService sets up profile-level facilities such as the ThemeSource for
  // the NTP.
  // However, in some tests, browser_ may be null.
  if (browser_) {
    InstantService* instant_service = GetInstantService();
    instant_service->AddObserver(this);
  }
}

InstantController::~InstantController() {
  if (browser_) {
    InstantService* instant_service = GetInstantService();
    instant_service->RemoveObserver(this);
  }
}

scoped_ptr<content::WebContents> InstantController::ReleaseNTPContents() {
  if (!extended_enabled() || !browser_->profile() ||
      browser_->profile()->IsOffTheRecord() ||
      !chrome::ShouldShowInstantNTP())
    return scoped_ptr<content::WebContents>();

  LOG_INSTANT_DEBUG_EVENT(this, "ReleaseNTPContents");

  if (ShouldSwitchToLocalNTP())
    ResetNTP(GetLocalInstantURL());

  scoped_ptr<content::WebContents> ntp_contents = ntp_->ReleaseContents();

  // Preload a new Instant NTP.
  ResetNTP(GetInstantURL());

  return ntp_contents.Pass();
}

void InstantController::SetOmniboxBounds(const gfx::Rect& bounds) {
  if (!extended_enabled() || omnibox_bounds_ == bounds)
    return;

  omnibox_bounds_ = bounds;
  if (ntp_)
    ntp_->sender()->SetOmniboxBounds(omnibox_bounds_);
  if (instant_tab_)
    instant_tab_->sender()->SetOmniboxBounds(omnibox_bounds_);
}

void InstantController::OnDefaultSearchProviderChanged() {
  if (ntp_ && extended_enabled()) {
    ntp_.reset();
    ResetNTP(GetInstantURL());
  }
}

void InstantController::ToggleVoiceSearch() {
  if (instant_tab_)
    instant_tab_->sender()->ToggleVoiceSearch();
}

void InstantController::InstantPageLoadFailed(content::WebContents* contents) {
  if (!chrome::ShouldPreferRemoteNTPOnStartup() || !extended_enabled()) {
    // We only need to fall back on errors if we're showing the online page
    // at startup, as otherwise we fall back correctly when trying to show
    // a page that hasn't yet indicated that it supports the InstantExtended
    // API.
    return;
  }

  if (IsContentsFrom(instant_tab(), contents)) {
    // Verify we're not already on a local page and that the URL precisely
    // equals the instant_url (minus the query params, as those will be filled
    // in by template values).  This check is necessary to make sure we don't
    // inadvertently redirect to the local NTP if someone, say, reloads a SRP
    // while offline, as a committed results page still counts as an instant
    // url.  We also check to make sure there's no forward history, as if
    // someone hits the back button a lot when offline and returns to a NTP
    // we don't want to redirect and nuke their forward history stack.
    const GURL& current_url = contents->GetURL();
    if (instant_tab_->IsLocal() ||
        !chrome::MatchesOriginAndPath(GURL(GetInstantURL()), current_url) ||
        !current_url.ref().empty() ||
        contents->GetController().CanGoForward())
      return;
    LOG_INSTANT_DEBUG_EVENT(this, "InstantPageLoadFailed: instant_tab");
    RedirectToLocalNTP(contents);
  } else if (IsContentsFrom(ntp(), contents)) {
    LOG_INSTANT_DEBUG_EVENT(this, "InstantPageLoadFailed: ntp");
    bool is_local = ntp_->IsLocal();
    DeletePageSoon(ntp_.Pass());
    if (!is_local)
      ResetNTP(GetLocalInstantURL());
  } else {
    NOTREACHED();
  }
}

content::WebContents* InstantController::GetNTPContents() const {
  return ntp_ ? ntp_->contents() : NULL;
}

bool InstantController::SubmitQuery(const string16& search_terms) {
  if (extended_enabled() && instant_tab_ && instant_tab_->supports_instant() &&
      search_mode_.is_origin_search()) {
    // Use |instant_tab_| to run the query if we're already on a search results
    // page. (NOTE: in particular, we do not send the query to NTPs.)
    instant_tab_->sender()->Submit(search_terms);
    instant_tab_->contents()->GetView()->Focus();
    EnsureSearchTermsAreSet(instant_tab_->contents(), search_terms);
    return true;
  }
  return false;
}

void InstantController::OmniboxFocusChanged(
    OmniboxFocusState state,
    OmniboxFocusChangeReason reason,
    gfx::NativeView view_gaining_focus) {
  LOG_INSTANT_DEBUG_EVENT(this, base::StringPrintf(
      "OmniboxFocusChanged: %d to %d for reason %d", omnibox_focus_state_,
      state, reason));

  omnibox_focus_state_ = state;
  if (!extended_enabled() || !instant_tab_)
    return;

  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_OMNIBOX_FOCUS_CHANGED,
      content::Source<InstantController>(this),
      content::NotificationService::NoDetails());

  instant_tab_->sender()->FocusChanged(omnibox_focus_state_, reason);
  // Don't send oninputstart/oninputend updates in response to focus changes
  // if there's a navigation in progress. This prevents Chrome from sending
  // a spurious oninputend when the user accepts a match in the omnibox.
  if (instant_tab_->contents()->GetController().GetPendingEntry() == NULL)
    instant_tab_->sender()->SetInputInProgress(IsInputInProgress());
}

void InstantController::SearchModeChanged(const SearchMode& old_mode,
                                          const SearchMode& new_mode) {
  if (!extended_enabled())
    return;

  LOG_INSTANT_DEBUG_EVENT(this, base::StringPrintf(
      "SearchModeChanged: [origin:mode] %d:%d to %d:%d", old_mode.origin,
      old_mode.mode, new_mode.origin, new_mode.mode));

  search_mode_ = new_mode;
  ResetInstantTab();

  if (instant_tab_ && old_mode.is_ntp() != new_mode.is_ntp())
    instant_tab_->sender()->SetInputInProgress(IsInputInProgress());
}

void InstantController::ActiveTabChanged() {
  if (!extended_enabled())
    return;

  LOG_INSTANT_DEBUG_EVENT(this, "ActiveTabChanged");
  ResetInstantTab();
}

void InstantController::TabDeactivated(content::WebContents* contents) {
  // If user is deactivating an NTP tab, log the number of mouseovers for this
  // NTP session.
  if (chrome::IsInstantNTP(contents))
    InstantNTP::EmitMouseoverCount(contents);
}

void InstantController::ThemeInfoChanged(
    const ThemeBackgroundInfo& theme_info) {
  if (!extended_enabled())
    return;

  if (ntp_)
    ntp_->sender()->SendThemeBackgroundInfo(theme_info);
  if (instant_tab_)
    instant_tab_->sender()->SendThemeBackgroundInfo(theme_info);
}

void InstantController::LogDebugEvent(const std::string& info) const {
  DVLOG(1) << info;

  debug_events_.push_front(std::make_pair(
      base::Time::Now().ToInternalValue(), info));
  static const size_t kMaxDebugEventSize = 2000;
  if (debug_events_.size() > kMaxDebugEventSize)
    debug_events_.pop_back();
}

void InstantController::ClearDebugEvents() {
  debug_events_.clear();
}

void InstantController::MostVisitedItemsChanged(
    const std::vector<InstantMostVisitedItem>& items) {
  if (ntp_)
    ntp_->sender()->SendMostVisitedItems(items);
  if (instant_tab_)
    instant_tab_->sender()->SendMostVisitedItems(items);

  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_INSTANT_SENT_MOST_VISITED_ITEMS,
      content::Source<InstantController>(this),
      content::NotificationService::NoDetails());
}

void InstantController::DeleteMostVisitedItem(const GURL& url) {
  DCHECK(!url.is_empty());
  InstantService* instant_service = GetInstantService();
  if (!instant_service)
    return;

  instant_service->DeleteMostVisitedItem(url);
}

void InstantController::UndoMostVisitedDeletion(const GURL& url) {
  DCHECK(!url.is_empty());
  InstantService* instant_service = GetInstantService();
  if (!instant_service)
    return;

  instant_service->UndoMostVisitedDeletion(url);
}

void InstantController::UndoAllMostVisitedDeletions() {
  InstantService* instant_service = GetInstantService();
  if (!instant_service)
    return;

  instant_service->UndoAllMostVisitedDeletions();
}

Profile* InstantController::profile() const {
  return browser_->profile();
}

InstantTab* InstantController::instant_tab() const {
  return instant_tab_.get();
}

InstantNTP* InstantController::ntp() const {
  return ntp_.get();
}

void InstantController::OnNetworkChanged(
    net::NetworkChangeNotifier::ConnectionType type) {
  // Not interested in events conveying change to offline
  if (type == net::NetworkChangeNotifier::CONNECTION_NONE)
    return;
  if (!extended_enabled_)
    return;
  if (!ntp_ || ntp_->IsLocal())
    ResetNTP(GetInstantURL());
}

// TODO(shishir): We assume that the WebContent's current RenderViewHost is the
// RenderViewHost being created which is not always true. Fix this.
void InstantController::InstantPageRenderViewCreated(
    const content::WebContents* contents) {
  if (!extended_enabled())
    return;

  // Update theme info so that the page picks it up.
  InstantService* instant_service = GetInstantService();
  if (instant_service) {
    instant_service->UpdateThemeInfo();
    instant_service->UpdateMostVisitedItemsInfo();
  }

  // Ensure the searchbox API has the correct initial state.
  if (IsContentsFrom(ntp(), contents)) {
    ntp_->sender()->SetOmniboxBounds(omnibox_bounds_);
    ntp_->InitializeFonts();
    ntp_->InitializePromos();
  } else {
    NOTREACHED();
  }
}

void InstantController::InstantSupportChanged(
    InstantSupportState instant_support) {
  // Handle INSTANT_SUPPORT_YES here because InstantPage is not hooked up to the
  // active tab. Search model changed listener in InstantPage will handle other
  // cases.
  if (instant_support != INSTANT_SUPPORT_YES)
    return;

  ResetInstantTab();
}

void InstantController::InstantSupportDetermined(
    const content::WebContents* contents,
    bool supports_instant) {
  if (IsContentsFrom(instant_tab(), contents)) {
    if (!supports_instant)
      base::MessageLoop::current()->DeleteSoon(FROM_HERE,
                                               instant_tab_.release());

    content::NotificationService::current()->Notify(
        chrome::NOTIFICATION_INSTANT_TAB_SUPPORT_DETERMINED,
        content::Source<InstantController>(this),
        content::NotificationService::NoDetails());
  } else if (IsContentsFrom(ntp(), contents)) {
    if (!supports_instant) {
      bool is_local = ntp_->IsLocal();
      DeletePageSoon(ntp_.Pass());
      // Preload a local NTP in place of the broken online one.
      if (!is_local)
        ResetNTP(GetLocalInstantURL());
    }

    content::NotificationService::current()->Notify(
        chrome::NOTIFICATION_INSTANT_NTP_SUPPORT_DETERMINED,
        content::Source<InstantController>(this),
        content::NotificationService::NoDetails());

  } else {
    NOTREACHED();
  }
}

void InstantController::InstantPageRenderProcessGone(
    const content::WebContents* contents) {
  if (IsContentsFrom(ntp(), contents)) {
    DeletePageSoon(ntp_.Pass());
  } else {
    NOTREACHED();
  }
}

void InstantController::InstantPageAboutToNavigateMainFrame(
    const content::WebContents* contents,
    const GURL& url) {
  if (IsContentsFrom(instant_tab(), contents)) {
    // The Instant tab navigated.  Send it the data it needs to display
    // properly.
    UpdateInfoForInstantTab();
  } else {
    NOTREACHED();
  }
}

void InstantController::FocusOmnibox(const content::WebContents* contents,
                                     OmniboxFocusState state) {
  if (!extended_enabled())
    return;

  DCHECK(IsContentsFrom(instant_tab(), contents));

  // Do not add a default case in the switch block for the following reasons:
  // (1) Explicitly handle the new states. If new states are added in the
  // OmniboxFocusState, the compiler will warn the developer to handle the new
  // states.
  // (2) An attacker may control the renderer and sends the browser process a
  // malformed IPC. This function responds to the invalid |state| values by
  // doing nothing instead of crashing the browser process (intentional no-op).
  switch (state) {
    case OMNIBOX_FOCUS_VISIBLE:
      // TODO(samarth): re-enable this once setValue() correctly handles
      // URL-shaped queries.
      // browser_->FocusOmnibox(true);
      break;
    case OMNIBOX_FOCUS_INVISIBLE:
      browser_->FocusOmnibox(false);
      break;
    case OMNIBOX_FOCUS_NONE:
      if (omnibox_focus_state_ != OMNIBOX_FOCUS_INVISIBLE)
        contents->GetView()->Focus();
      break;
  }
}

void InstantController::NavigateToURL(const content::WebContents* contents,
                                      const GURL& url,
                                      content::PageTransition transition,
                                      WindowOpenDisposition disposition,
                                      bool is_search_type) {
  LOG_INSTANT_DEBUG_EVENT(this, base::StringPrintf(
      "NavigateToURL: url='%s'", url.spec().c_str()));

  // TODO(samarth): handle case where contents are no longer "active" (e.g. user
  // has switched tabs).
  if (!extended_enabled())
    return;

  if (transition == content::PAGE_TRANSITION_AUTO_BOOKMARK) {
    content::RecordAction(
        content::UserMetricsAction("InstantExtended.MostVisitedClicked"));
  } else {
    // Exclude navigation by Most Visited click and searches.
    if (!is_search_type)
      RecordNavigationHistogram(UsingLocalPage(), true, true);
  }
  browser_->OpenURL(url, transition, disposition);
}

std::string InstantController::GetLocalInstantURL() const {
  return chrome::GetLocalInstantURL(profile()).spec();
}

std::string InstantController::GetInstantURL() const {
  if (extended_enabled() && net::NetworkChangeNotifier::IsOffline())
    return GetLocalInstantURL();

  const GURL instant_url = chrome::GetInstantURL(profile(),
                                                 omnibox_bounds_.x());
  if (instant_url.is_valid())
    return instant_url.spec();

  // Only extended mode has a local fallback.
  return extended_enabled() ? GetLocalInstantURL() : std::string();
}

bool InstantController::extended_enabled() const {
  return extended_enabled_;
}

bool InstantController::PageIsCurrent(const InstantPage* page) const {

  const std::string& instant_url = GetInstantURL();
  if (instant_url.empty() ||
      !chrome::MatchesOriginAndPath(GURL(page->instant_url()),
                                    GURL(instant_url)))
    return false;

  return page->supports_instant();
}

void InstantController::ResetNTP(const std::string& instant_url) {
  // Never load the Instant NTP if it is disabled.
  if (!chrome::ShouldShowInstantNTP())
    return;

  // Instant NTP is only used in extended mode so we should always have a
  // non-empty URL to use.
  DCHECK(!instant_url.empty());
  ntp_.reset(new InstantNTP(this, instant_url,
                            browser_->profile()->IsOffTheRecord()));
  ntp_->InitContents(profile(), browser_->GetActiveWebContents(),
                     base::Bind(&InstantController::ReloadStaleNTP,
                                base::Unretained(this)));
  LOG_INSTANT_DEBUG_EVENT(this, base::StringPrintf(
      "ResetNTP: instant_url='%s'", instant_url.c_str()));
}

void InstantController::ReloadStaleNTP() {
  if (extended_enabled())
    ResetNTP(GetInstantURL());
}

bool InstantController::ShouldSwitchToLocalNTP() const {
  if (!ntp())
    return true;

  // Assume users with Javascript disabled do not want the online experience.
  if (!IsJavascriptEnabled())
    return true;

  // Already a local page. Not calling IsLocal() because we want to distinguish
  // between the Google-specific and generic local NTP.
  if (extended_enabled() && ntp()->instant_url() == GetLocalInstantURL())
    return false;

  if (PageIsCurrent(ntp()))
    return false;

  // The preloaded NTP does not support instant yet. If we're not in startup,
  // always fall back to the local NTP. If we are in startup, use the local NTP
  // (unless the finch flag to use the remote NTP is set).
  return !(InStartup() && chrome::ShouldPreferRemoteNTPOnStartup());
}

void InstantController::ResetInstantTab() {
  if (!search_mode_.is_origin_default()) {
    content::WebContents* active_tab = browser_->GetActiveWebContents();
    if (!instant_tab_ || active_tab != instant_tab_->contents()) {
      instant_tab_.reset(
          new InstantTab(this, browser_->profile()->IsOffTheRecord()));
      instant_tab_->Init(active_tab);
      UpdateInfoForInstantTab();
    }
  } else {
    instant_tab_.reset();
  }
}

void InstantController::UpdateInfoForInstantTab() {
  if (instant_tab_) {
    instant_tab_->sender()->SetOmniboxBounds(omnibox_bounds_);

    // Update theme details.
    InstantService* instant_service = GetInstantService();
    if (instant_service) {
      instant_service->UpdateThemeInfo();
      instant_service->UpdateMostVisitedItemsInfo();
    }

    instant_tab_->InitializeFonts();
    instant_tab_->InitializePromos();
    instant_tab_->sender()->FocusChanged(omnibox_focus_state_,
                                         omnibox_focus_change_reason_);
    instant_tab_->sender()->SetInputInProgress(IsInputInProgress());
  }
}

bool InstantController::IsInputInProgress() const {
  return !search_mode_.is_ntp() &&
      omnibox_focus_state_ == OMNIBOX_FOCUS_VISIBLE;
}

bool InstantController::UsingLocalPage() const {
  return instant_tab_ && instant_tab_->IsLocal();
}

void InstantController::RedirectToLocalNTP(content::WebContents* contents) {
  contents->GetController().LoadURL(
    chrome::GetLocalInstantURL(browser_->profile()),
    content::Referrer(),
    content::PAGE_TRANSITION_SERVER_REDIRECT,
    std::string());  // No extra headers.
  // TODO(dcblack): Remove extraneous history entry caused by 404s.
  // Note that the base case of a 204 being returned doesn't push a history
  // entry.
}

bool InstantController::IsJavascriptEnabled() const {
  GURL instant_url(GetInstantURL());
  GURL origin(instant_url.GetOrigin());
  ContentSetting js_setting = profile()->GetHostContentSettingsMap()->
      GetContentSetting(origin, origin, CONTENT_SETTINGS_TYPE_JAVASCRIPT,
                        NO_RESOURCE_IDENTIFIER);
  // Javascript can be disabled either in content settings or via a WebKit
  // preference, so check both. Disabling it through the Settings page affects
  // content settings. I'm not sure how to disable the WebKit preference, but
  // it's theoretically possible some users have it off.
  bool js_content_enabled =
      js_setting == CONTENT_SETTING_DEFAULT ||
      js_setting == CONTENT_SETTING_ALLOW;
  bool js_webkit_enabled = profile()->GetPrefs()->GetBoolean(
      prefs::kWebKitJavascriptEnabled);
  return js_content_enabled && js_webkit_enabled;
}

bool InstantController::InStartup() const {
  // TODO(shishir): This is not completely reliable. Find a better way to detect
  // startup time.
  return !browser_->GetActiveWebContents();
}

InstantService* InstantController::GetInstantService() const {
  return InstantServiceFactory::GetForProfile(profile());
}
