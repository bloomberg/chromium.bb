// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/search/search_tab_helper.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/search/search.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"

DEFINE_WEB_CONTENTS_USER_DATA_KEY(chrome::search::SearchTabHelper);

namespace {

bool IsSearchEnabled(const content::WebContents* contents) {
  Profile* profile = Profile::FromBrowserContext(contents->GetBrowserContext());
  return chrome::search::IsInstantExtendedAPIEnabled(profile);
}

bool IsNTP(const content::WebContents* contents) {
  // We can't use WebContents::GetURL() because that uses the active entry,
  // whereas we want the visible entry.
  const content::NavigationEntry* entry =
      contents->GetController().GetVisibleEntry();
  if (entry && entry->GetVirtualURL() == GURL(chrome::kChromeUINewTabURL))
    return true;

  return chrome::search::IsInstantNTP(contents);
}

bool IsSearchResults(const content::WebContents* contents) {
  return !chrome::search::GetSearchTerms(contents).empty();
}

}  // namespace

namespace chrome {
namespace search {

SearchTabHelper::SearchTabHelper(content::WebContents* web_contents)
    : is_search_enabled_(IsSearchEnabled(web_contents)),
      user_input_in_progress_(false),
      model_(web_contents) {
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

  UpdateModel();
}

void SearchTabHelper::NavigationEntryUpdated() {
  if (!is_search_enabled_)
    return;

  UpdateModel();
}

void SearchTabHelper::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK_EQ(content::NOTIFICATION_NAV_ENTRY_COMMITTED, type);
  UpdateModel();
}

void SearchTabHelper::UpdateModel() {
  Mode::Type type = Mode::MODE_DEFAULT;
  Mode::Origin origin = Mode::ORIGIN_DEFAULT;
  if (IsNTP(web_contents())) {
    type = Mode::MODE_NTP;
    origin = Mode::ORIGIN_NTP;
  } else if (IsSearchResults(web_contents())) {
    type = Mode::MODE_SEARCH_RESULTS;
    origin = Mode::ORIGIN_SEARCH;
  }
  if (user_input_in_progress_)
    type = Mode::MODE_SEARCH_SUGGESTIONS;
  model_.SetMode(Mode(type, origin));
}

const content::WebContents* SearchTabHelper::web_contents() const {
  return model_.web_contents();
}

}  // namespace search
}  // namespace chrome
