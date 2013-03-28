// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/search/search_tab_helper.h"

#include "chrome/browser/search/search.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"

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

}  // namespace

SearchTabHelper::SearchTabHelper(content::WebContents* web_contents)
    : WebContentsObserver(web_contents),
      is_search_enabled_(chrome::IsInstantExtendedAPIEnabled()),
      user_input_in_progress_(false),
      web_contents_(web_contents) {
  if (!is_search_enabled_)
    return;

  registrar_.Add(
      this,
      content::NOTIFICATION_NAV_ENTRY_COMMITTED,
      content::Source<content::NavigationController>(
          &web_contents->GetController()));
}

SearchTabHelper::~SearchTabHelper() {
}

void SearchTabHelper::OmniboxEditModelChanged(bool user_input_in_progress,
                                              bool cancelling) {
  if (!is_search_enabled_)
    return;

  user_input_in_progress_ = user_input_in_progress;
  if (!user_input_in_progress && !cancelling)
    return;

  UpdateMode();
}

void SearchTabHelper::NavigationEntryUpdated() {
  if (!is_search_enabled_)
    return;

  UpdateMode();
}

void SearchTabHelper::OnStopObservingTab() {
  if (!is_search_enabled_)
    return;

  // When switching away from a |DEFAULT| tab with an Instant overlay, the
  // overlay will be closed, so bookmark and info bars should be visible for
  // this tab if user switches back to it.
  const SearchMode& mode = model_.state().mode;
  if (mode.is_origin_default() && mode.is_search_suggestions())
    model_.SetTopBarsVisible(true);
}

void SearchTabHelper::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK_EQ(content::NOTIFICATION_NAV_ENTRY_COMMITTED, type);
  UpdateMode();
}

bool SearchTabHelper::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(SearchTabHelper, message)
    IPC_MESSAGE_HANDLER(ChromeViewHostMsg_SearchBoxShowBars,
                        OnSearchBoxShowBars)
    IPC_MESSAGE_HANDLER(ChromeViewHostMsg_SearchBoxHideBars,
                        OnSearchBoxHideBars)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
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
  model_.SetMode(SearchMode(type, origin));
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
