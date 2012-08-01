// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/search/search_tab_helper.h"

#include "chrome/browser/google/google_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/omnibox/omnibox_edit_model.h"
#include "chrome/browser/ui/tab_contents/tab_contents.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/web_contents.h"
#include "ipc/ipc_message.h"

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
      model_(contents) {
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

content::WebContents* SearchTabHelper::GetNTPWebContents() {
  if (!ntp_web_contents_.get()) {
    ntp_web_contents_.reset(content::WebContents::Create(
        model_.tab_contents()->profile(),
        model_.tab_contents()->web_contents()->GetSiteInstance(),
        MSG_ROUTING_NONE,
        NULL,
        NULL));
    ntp_web_contents_->GetController().LoadURL(
        GURL(chrome::kChromeUINewTabURL),
        content::Referrer(),
        content::PAGE_TRANSITION_START_PAGE,
        std::string());
  }
  return ntp_web_contents_.get();
}

void SearchTabHelper::OmniboxEditModelChanged(OmniboxEditModel* edit_model) {
  if (!is_search_enabled_)
    return;

  if (model_.mode().is_ntp()) {
    if (edit_model->user_input_in_progress())
      model_.SetMode(Mode(Mode::MODE_SEARCH, true));
    return;
  }

  Mode::Type mode = Mode::MODE_DEFAULT;
  GURL url(web_contents()->GetURL());
  // TODO(kuan): revisit this condition when zero suggest becomes available.
  if (google_util::IsInstantExtendedAPIGoogleSearchUrl(url.spec()) ||
      (edit_model->has_focus() && edit_model->user_input_in_progress())) {
    mode = Mode::MODE_SEARCH;
  }
  model_.SetMode(Mode(mode, true));
}

void SearchTabHelper::NavigateToPendingEntry(
    const GURL& url,
    content::NavigationController::ReloadType reload_type) {
  if (!is_search_enabled_)
    return;

  UpdateModel(url);
  FlushNTP(url);
}

void SearchTabHelper::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK_EQ(content::NOTIFICATION_NAV_ENTRY_COMMITTED, type);
  content::LoadCommittedDetails* committed_details =
      content::Details<content::LoadCommittedDetails>(details).ptr();
  UpdateModel(committed_details->entry->GetURL());
  FlushNTP(committed_details->entry->GetURL());
}

void SearchTabHelper::UpdateModel(const GURL& url) {
  Mode::Type type = Mode::MODE_DEFAULT;
  if (IsNTP(url))
    type = Mode::MODE_NTP;
  else if (google_util::IsInstantExtendedAPIGoogleSearchUrl(url.spec()))
    type = Mode::MODE_SEARCH;
  model_.SetMode(Mode(type, true));
}

void SearchTabHelper::FlushNTP(const GURL& url) {
  if (!IsNTP(url) &&
      !google_util::IsInstantExtendedAPIGoogleSearchUrl(url.spec())) {
    ntp_web_contents_.reset();
  }
}

}  // namespace search
}  // namespace chrome
