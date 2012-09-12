// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/search/search_tab_helper.h"

#include "chrome/browser/google/google_util.h"
#include "chrome/browser/ui/tab_contents/tab_contents.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_widget_host_view.h"
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
      is_initial_navigation_commit_(true),
      model_(contents),
      ntp_load_state_(DEFAULT),
      main_frame_id_(0) {
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

void SearchTabHelper::OmniboxEditModelChanged(bool user_input_in_progress,
                                              bool cancelling) {
  if (!is_search_enabled_)
    return;

  if (user_input_in_progress)
    model_.SetMode(Mode(Mode::MODE_SEARCH_SUGGESTIONS, true));
  else if (cancelling)
    UpdateModelBasedOnURL(web_contents()->GetURL(), ntp_load_state_, true);
}

void SearchTabHelper::NavigateToPendingEntry(
    const GURL& url,
    content::NavigationController::ReloadType reload_type) {
  if (!is_search_enabled_)
    return;

  ntp_load_state_ = DEFAULT;
  main_frame_id_ = 0;

  // Do not animate if this url is the very first navigation for the tab.
  // NTP mode changes are initiated at "pending", all others are initiated
  // when "committed".  This is because NTP is rendered natively so is faster
  // to render than the web contents and we need to coordinate the animations.
  if (IsNTP(url)) {
    ntp_load_state_ = WAITING_FOR_FRAME_ID;
    UpdateModelBasedOnURL(url, ntp_load_state_, !is_initial_navigation_commit_);
  }
}

void SearchTabHelper::DidStartProvisionalLoadForFrame(
    int64 frame_id,
    bool is_main_frame,
    const GURL& validated_url,
    bool is_error_page,
    content::RenderViewHost* render_view_host) {
  if (ntp_load_state_ == WAITING_FOR_FRAME_ID &&
      is_main_frame && IsNTP(validated_url)) {
    content::NavigationEntry* pending_entry =
        web_contents()->GetController().GetPendingEntry();
    if (pending_entry && IsNTP(pending_entry->GetURL())) {
      ntp_load_state_ = WAITING_FOR_FRAME_LOAD;
      main_frame_id_ = frame_id;
    }
  }
}

void SearchTabHelper::DocumentLoadedInFrame(
    int64 frame_id,
    content::RenderViewHost* render_view_host) {
  if (ntp_load_state_ == WAITING_FOR_FRAME_LOAD && main_frame_id_ == frame_id) {
    ntp_load_state_ = WAITING_FOR_PAINT;
    registrar_.Add(
        this,
        content::NOTIFICATION_RENDER_WIDGET_HOST_DID_UPDATE_BACKING_STORE,
        content::Source<content::RenderWidgetHost>(GetRenderWidgetHost()));
  }
}

void SearchTabHelper::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  switch (type) {
    case content::NOTIFICATION_NAV_ENTRY_COMMITTED: {
      content::LoadCommittedDetails* committed_details =
          content::Details<content::LoadCommittedDetails>(details).ptr();
      // See comment in |NavigateToPendingEntry()| about why |!IsNTP()| is used.
      if (!IsNTP(committed_details->entry->GetURL())) {
        UpdateModelBasedOnURL(committed_details->entry->GetURL(),
                              ntp_load_state_,
                              !is_initial_navigation_commit_);
      }
      is_initial_navigation_commit_ = false;
      break;
    }

    case content::NOTIFICATION_RENDER_WIDGET_HOST_DID_UPDATE_BACKING_STORE: {
      if (ntp_load_state_ == WAITING_FOR_PAINT) {
        DCHECK_EQ(GetRenderWidgetHost(),
                  content::Source<content::RenderWidgetHost>(source).ptr());
        DCHECK(IsNTP(web_contents()->GetURL()));
        ntp_load_state_ = PAINTED;
        UpdateModelBasedOnURL(web_contents()->GetURL(), ntp_load_state_, false);
        registrar_.Remove(
            this,
            content::NOTIFICATION_RENDER_WIDGET_HOST_DID_UPDATE_BACKING_STORE,
            content::Source<content::RenderWidgetHost>(GetRenderWidgetHost()));
      }
      break;
    }

    default:
      NOTREACHED() << "Unexpected notification type.";
  }
}

void SearchTabHelper::UpdateModelBasedOnURL(const GURL& url,
                                            NTPLoadState load_state,
                                            bool animate) {
  Mode::Type type = Mode::MODE_DEFAULT;
  if (IsNTP(url))
    type = load_state == PAINTED ? Mode::MODE_NTP : Mode::MODE_NTP_LOADING;
  else if (google_util::IsInstantExtendedAPIGoogleSearchUrl(url.spec()))
    type = Mode::MODE_SEARCH_RESULTS;
  model_.SetMode(Mode(type, animate));
}

content::WebContents* SearchTabHelper::web_contents() {
  return model_.tab_contents()->web_contents();
}

content::RenderWidgetHost* SearchTabHelper::GetRenderWidgetHost() {
  content::RenderWidgetHostView* rwhv =
      web_contents()->GetRenderWidgetHostView();
  return rwhv ? rwhv->GetRenderWidgetHost() : NULL;
}

}  // namespace search
}  // namespace chrome
