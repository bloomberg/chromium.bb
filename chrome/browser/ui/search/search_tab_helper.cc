// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/search/search_tab_helper.h"

#include "chrome/browser/google/google_util.h"
#include "chrome/browser/ui/omnibox/omnibox_edit_model.h"
#include "chrome/browser/ui/search/search_model.h"
#include "chrome/browser/ui/tab_contents/tab_contents.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/web_contents.h"

namespace {

bool IsNTP(const GURL& url) {
  return url.SchemeIs(chrome::kChromeUIScheme) &&
         url.host() == chrome::kChromeUINewTabHost;
}

}  // namespace

namespace chrome {
namespace search {

SearchTabHelper::SearchTabHelper(
    TabContents* contents,
    bool is_search_enabled)
    : WebContentsObserver(contents->web_contents()),
      is_search_enabled_(is_search_enabled),
      model_(new SearchModel(contents)) {
  if (!is_search_enabled)
    return;

  registrar_.Add(
      this,
      content::NOTIFICATION_NAV_ENTRY_COMMITTED,
      content::Source<content::NavigationController>(
          &contents->web_contents()->GetController()));
}

SearchTabHelper::~SearchTabHelper() {
}

void SearchTabHelper::OmniboxEditModelChanged(OmniboxEditModel* edit_model) {
  if (!is_search_enabled_)
    return;

  if (model_->mode().is_ntp()) {
    if (edit_model->user_input_in_progress())
      model_->SetMode(Mode(Mode::MODE_SEARCH, true));
    return;
  }

  Mode::Type mode = Mode::MODE_DEFAULT;
  GURL url(web_contents()->GetURL());
  if (google_util::IsInstantExtendedAPIGoogleSearchUrl(url.spec()) ||
      edit_model->has_focus()) {
    mode = Mode::MODE_SEARCH;
  }
  model_->SetMode(Mode(mode, true));
}

void SearchTabHelper::NavigateToPendingEntry(
    const GURL& url,
    content::NavigationController::ReloadType reload_type) {
  if (!is_search_enabled_)
    return;

  UpdateModel(url);
}

void SearchTabHelper::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK_EQ(content::NOTIFICATION_NAV_ENTRY_COMMITTED, type);
  content::LoadCommittedDetails* committed_details =
      content::Details<content::LoadCommittedDetails>(details).ptr();
  UpdateModel(committed_details->entry->GetURL());
}

void SearchTabHelper::UpdateModel(const GURL& url) {
  Mode::Type type = Mode::MODE_DEFAULT;
  if (IsNTP(url))
    type = Mode::MODE_NTP;
  else if (google_util::IsInstantExtendedAPIGoogleSearchUrl(url.spec()))
    type = Mode::MODE_SEARCH;
  model_->SetMode(Mode(type, true));
}

}  // namespace search
}  // namespace chrome
