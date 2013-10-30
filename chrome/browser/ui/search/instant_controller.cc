// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/search/instant_controller.h"

#include "base/prefs/pref_service.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/chrome_notification_types.h"
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
#include "chrome/browser/ui/search/instant_tab.h"
#include "chrome/browser/ui/search/search_tab_helper.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/content_settings_types.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/search_urls.h"
#include "chrome/common/url_constants.h"
#include "components/sessions/serialized_navigation_entry.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_view.h"
#include "net/base/escape.h"
#include "net/base/network_change_notifier.h"
#include "url/gurl.h"

#if defined(TOOLKIT_VIEWS)
#include "ui/views/widget/widget.h"
#endif

namespace {

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

}  // namespace

InstantController::InstantController(BrowserInstantController* browser)
    : browser_(browser),
      omnibox_focus_state_(OMNIBOX_FOCUS_NONE),
      omnibox_focus_change_reason_(OMNIBOX_FOCUS_CHANGE_EXPLICIT),
      omnibox_bounds_(-1, -1, 0, 0) {
}

InstantController::~InstantController() {
}

void InstantController::SetOmniboxBounds(const gfx::Rect& bounds) {
  if (omnibox_bounds_ == bounds)
    return;

  omnibox_bounds_ = bounds;
  if (instant_tab_)
    instant_tab_->sender()->SetOmniboxBounds(omnibox_bounds_);
}

void InstantController::SetSuggestionToPrefetch(
    const InstantSuggestion& suggestion) {
  if (instant_tab_ && search_mode_.is_search()) {
    SearchTabHelper::FromWebContents(instant_tab_->contents())->
        SetSuggestionToPrefetch(suggestion);
  }
}

void InstantController::ToggleVoiceSearch() {
  if (instant_tab_)
    instant_tab_->sender()->ToggleVoiceSearch();
}

void InstantController::InstantPageLoadFailed(content::WebContents* contents) {
  if (!chrome::ShouldPreferRemoteNTPOnStartup()) {
    // We only need to fall back on errors if we're showing the online page
    // at startup, as otherwise we fall back correctly when trying to show
    // a page that hasn't yet indicated that it supports the InstantExtended
    // API.
    return;
  }

  DCHECK(IsContentsFrom(instant_tab(), contents));

  // Verify we're not already on a local page and that the URL precisely
  // equals the instant_url (minus the query params, as those will be filled
  // in by template values).  This check is necessary to make sure we don't
  // inadvertently redirect to the local NTP if someone, say, reloads a SRP
  // while offline, as a committed results page still counts as an instant
  // url.  We also check to make sure there's no forward history, as if
  // someone hits the back button a lot when offline and returns to a NTP
  // we don't want to redirect and nuke their forward history stack.
  const GURL& current_url = contents->GetURL();
  GURL instant_url = chrome::GetInstantURL(profile(),
                                           chrome::kDisableStartMargin, false);
  if (instant_tab_->IsLocal() ||
      !search::MatchesOriginAndPath(instant_url, current_url) ||
      !current_url.ref().empty() ||
      contents->GetController().CanGoForward())
    return;
  LOG_INSTANT_DEBUG_EVENT(this, "InstantPageLoadFailed: instant_tab");
  RedirectToLocalNTP(contents);
}

bool InstantController::SubmitQuery(const string16& search_terms) {
  if (instant_tab_ && instant_tab_->supports_instant() &&
      search_mode_.is_origin_search()) {
    // Use |instant_tab_| to run the query if we're already on a search results
    // page. (NOTE: in particular, we do not send the query to NTPs.)
    SearchTabHelper::FromWebContents(instant_tab_->contents())->Submit(
        search_terms);
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
  if (!instant_tab_)
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
  LOG_INSTANT_DEBUG_EVENT(this, base::StringPrintf(
      "SearchModeChanged: [origin:mode] %d:%d to %d:%d", old_mode.origin,
      old_mode.mode, new_mode.origin, new_mode.mode));

  search_mode_ = new_mode;
  ResetInstantTab();

  if (instant_tab_ && old_mode.is_ntp() != new_mode.is_ntp())
    instant_tab_->sender()->SetInputInProgress(IsInputInProgress());
}

void InstantController::ActiveTabChanged() {
  LOG_INSTANT_DEBUG_EVENT(this, "ActiveTabChanged");
  ResetInstantTab();
}

void InstantController::TabDeactivated(content::WebContents* contents) {
  // If user is deactivating an NTP tab, log the number of mouseovers for this
  // NTP session.
  if (chrome::IsInstantNTP(contents))
    InstantTab::EmitMouseoverCount(contents);
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

Profile* InstantController::profile() const {
  return browser_->profile();
}

InstantTab* InstantController::instant_tab() const {
  return instant_tab_.get();
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
  DCHECK(IsContentsFrom(instant_tab(), contents));

  if (!supports_instant)
    base::MessageLoop::current()->DeleteSoon(FROM_HERE, instant_tab_.release());

  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_INSTANT_TAB_SUPPORT_DETERMINED,
      content::Source<InstantController>(this),
      content::NotificationService::NoDetails());
}

void InstantController::InstantPageAboutToNavigateMainFrame(
    const content::WebContents* contents,
    const GURL& url) {
  DCHECK(IsContentsFrom(instant_tab(), contents));

  // The Instant tab navigated.  Send it the data it needs to display
  // properly.
  UpdateInfoForInstantTab();
}

void InstantController::PasteIntoOmnibox(const content::WebContents* contents,
      const string16& text) {
  if (search_mode_.is_origin_default())
    return;

  DCHECK(IsContentsFrom(instant_tab(), contents));

  browser_->PasteIntoOmnibox(text);
}

void InstantController::ResetInstantTab() {
  if (!search_mode_.is_origin_default()) {
    content::WebContents* active_tab = browser_->GetActiveWebContents();
    if (!instant_tab_ || active_tab != instant_tab_->contents()) {
      instant_tab_.reset(new InstantTab(this, browser_->profile()));
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

    instant_tab_->sender()->FocusChanged(omnibox_focus_state_,
                                         omnibox_focus_change_reason_);
    instant_tab_->sender()->SetInputInProgress(IsInputInProgress());
  }
}

bool InstantController::IsInputInProgress() const {
  return !search_mode_.is_ntp() &&
      omnibox_focus_state_ == OMNIBOX_FOCUS_VISIBLE;
}

void InstantController::RedirectToLocalNTP(content::WebContents* contents) {
  contents->GetController().LoadURL(
      GURL(chrome::kChromeSearchLocalNtpUrl),
      content::Referrer(),
      content::PAGE_TRANSITION_SERVER_REDIRECT,
      std::string());  // No extra headers.
  // TODO(dcblack): Remove extraneous history entry caused by 404s.
  // Note that the base case of a 204 being returned doesn't push a history
  // entry.
}

InstantService* InstantController::GetInstantService() const {
  return InstantServiceFactory::GetForProfile(profile());
}
