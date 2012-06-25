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
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_widget_host.h"
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
      model_(new SearchModel(contents)),
      ntp_load_state_(DEFAULT),
      main_frame_id_(0) {
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

  registrar_.RemoveAll();
  Mode::Type type = Mode::MODE_DEFAULT;
  ntp_load_state_ = DEFAULT;
  if (IsNTP(url)) {
    type = Mode::MODE_NTP_LOADING;
    ntp_load_state_ = WAITING_FOR_FRAME_ID;
  } else if (google_util::IsInstantExtendedAPIGoogleSearchUrl(url.spec())) {
    type = Mode::MODE_SEARCH;
  }
  model_->SetMode(Mode(type, true));
}

void SearchTabHelper::DidStartProvisionalLoadForFrame(
    int64 frame_id,
    bool is_main_frame,
    const GURL& validated_url,
    bool is_error_page,
    content::RenderViewHost* render_view_host) {
  if (ntp_load_state_ == WAITING_FOR_FRAME_ID && is_main_frame &&
      IsNTP(validated_url)) {
    content::NavigationEntry* pending_entry =
        web_contents()->GetController().GetPendingEntry();
    if (pending_entry && IsNTP(pending_entry->GetURL())) {
      ntp_load_state_ = WAITING_FOR_FRAME_LOAD;
      main_frame_id_ = frame_id;
    }
  }
}

void SearchTabHelper::DocumentLoadedInFrame(int64 frame_id) {
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
  DCHECK_EQ(content::NOTIFICATION_RENDER_WIDGET_HOST_DID_UPDATE_BACKING_STORE,
            type);
  if (ntp_load_state_ == WAITING_FOR_PAINT) {
    DCHECK_EQ(GetRenderWidgetHost(),
              content::Source<content::RenderWidgetHost>(source).ptr());
    ntp_load_state_ = DEFAULT;
    model_->MaybeChangeMode(Mode::MODE_NTP_LOADING, Mode::MODE_NTP);
    registrar_.RemoveAll();
  }
}

content::RenderWidgetHost* SearchTabHelper::GetRenderWidgetHost() {
  content::RenderWidgetHostView* rwhv =
      web_contents()->GetRenderWidgetHostView();
  return rwhv ? rwhv->GetRenderWidgetHost() : NULL;
}

}  // namespace search
}  // namespace chrome
