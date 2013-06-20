// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/search/search_tab_helper.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/search.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_type.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/web_contents.h"

DEFINE_WEB_CONTENTS_USER_DATA_KEY(SearchTabHelper);

namespace {

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

// TODO(kmadhusu): Move this helper from anonymous namespace to chrome
// namespace and remove InstantPage::IsLocal().
bool IsLocal(const content::WebContents* contents) {
  return contents &&
      (contents->GetURL() == GURL(chrome::kChromeSearchLocalNtpUrl) ||
       contents->GetURL() == GURL(chrome::kChromeSearchLocalGoogleNtpUrl));
}

}  // namespace

SearchTabHelper::SearchTabHelper(content::WebContents* web_contents)
    : WebContentsObserver(web_contents),
      is_search_enabled_(chrome::IsInstantExtendedAPIEnabled()),
      user_input_in_progress_(false),
      popup_is_open_(false),
      user_text_is_empty_(true),
      web_contents_(web_contents) {
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
}

SearchTabHelper::~SearchTabHelper() {
}

void SearchTabHelper::OmniboxEditModelChanged(bool user_input_in_progress,
                                              bool cancelling,
                                              bool popup_is_open,
                                              bool user_text_is_empty) {
  if (!is_search_enabled_)
    return;

  user_input_in_progress_ = user_input_in_progress;
  popup_is_open_ = popup_is_open;
  user_text_is_empty_ = user_text_is_empty;
  if (!user_input_in_progress && !cancelling)
    return;

  UpdateMode();
}

void SearchTabHelper::NavigationEntryUpdated() {
  if (!is_search_enabled_)
    return;

  UpdateMode();
}

bool SearchTabHelper::UpdateLastKnownMostVisitedItems(
    const std::vector<InstantMostVisitedItem>& items) {
  if (chrome::AreMostVisitedItemsEqual(items, last_known_most_visited_items_))
    return false;

  last_known_most_visited_items_ = items;
  return true;
}

void SearchTabHelper::InstantSupportChanged(bool instant_support) {
  if (!is_search_enabled_)
    return;

  model_.SetInstantSupportState(instant_support ? INSTANT_SUPPORT_YES :
                                                  INSTANT_SUPPORT_NO);
}

bool SearchTabHelper::SupportsInstant() const {
  return model_.instant_support() == INSTANT_SUPPORT_YES;
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

  UpdateMode();
  last_known_most_visited_items_.clear();

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
    return;
  }

  model_.SetInstantSupportState(INSTANT_SUPPORT_UNKNOWN);
}

bool SearchTabHelper::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(SearchTabHelper, message)
    IPC_MESSAGE_HANDLER(ChromeViewHostMsg_SearchBoxShowBars,
                        OnSearchBoxShowBars)
    IPC_MESSAGE_HANDLER(ChromeViewHostMsg_SearchBoxHideBars,
                        OnSearchBoxHideBars)
    IPC_MESSAGE_HANDLER(ChromeViewHostMsg_InstantSupportDetermined,
                        OnInstantSupportDetermined)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void SearchTabHelper::DidFinishLoad(
    int64 /* frame_id */,
    const GURL&  /* validated_url */,
    bool is_main_frame,
    content::RenderViewHost* /* render_view_host */) {
  if (is_main_frame)
    DetermineIfPageSupportsInstant();
}

void SearchTabHelper::UpdateMode() {
  SearchMode::Type type = SearchMode::MODE_DEFAULT;
  SearchMode::Origin origin = SearchMode::ORIGIN_DEFAULT;
  if (IsNTP(web_contents_)) {
    type = SearchMode::MODE_NTP;
    origin = SearchMode::ORIGIN_NTP;
  } else if (IsSearchResults(web_contents_)) {
    type = SearchMode::MODE_SEARCH_RESULTS;
    origin = SearchMode::ORIGIN_SEARCH;
  }
  if (user_input_in_progress_)
    type = SearchMode::MODE_SEARCH_SUGGESTIONS;

  if (type == SearchMode::MODE_NTP && origin == SearchMode::ORIGIN_NTP &&
      !popup_is_open_ && !user_text_is_empty_) {
    // We're switching back (|popup_is_open_| is false) to an NTP (type and
    // mode are |NTP|) with suggestions (|user_text_is_empty_| is false), don't
    // modify visibility of top bars.  This specific omnibox state is set when
    // OmniboxEditModelChanged() is called from
    // OmniboxEditModel::SetInputInProgress() which is called from
    // OmniboxEditModel::Revert().
    model_.SetState(SearchModel::State(SearchMode(type, origin),
                                       model_.state().top_bars_visible,
                                       model_.instant_support()));
  } else {
    model_.SetMode(SearchMode(type, origin));
  }
}

void SearchTabHelper::DetermineIfPageSupportsInstant() {
  Profile* profile =
      Profile::FromBrowserContext(web_contents_->GetBrowserContext());
  if (!chrome::ShouldAssignURLToInstantRenderer(web_contents_->GetURL(),
                                                profile)) {
    // The page is not in the Instant process. This page does not support
    // instant. If we send an IPC message to a page that is not in the Instant
    // process, it will never receive it and will never respond. Therefore,
    // return immediately.
    InstantSupportChanged(false);
  } else if (IsLocal(web_contents_)) {
    // Local pages always support Instant.
    InstantSupportChanged(true);
  } else {
    Send(new ChromeViewMsg_DetermineIfPageSupportsInstant(routing_id()));
  }
}

void SearchTabHelper::OnInstantSupportDetermined(int page_id,
                                                 bool instant_support) {
  if (!web_contents()->IsActiveEntry(page_id))
    return;

  InstantSupportChanged(instant_support);
}

void SearchTabHelper::OnSearchBoxShowBars(int page_id) {
  if (web_contents()->IsActiveEntry(page_id))
    model_.SetTopBarsVisible(true);
}

void SearchTabHelper::OnSearchBoxHideBars(int page_id) {
  if (web_contents()->IsActiveEntry(page_id)) {
    model_.SetTopBarsVisible(false);
    Send(new ChromeViewMsg_SearchBoxBarsHidden(routing_id()));
  }
}
