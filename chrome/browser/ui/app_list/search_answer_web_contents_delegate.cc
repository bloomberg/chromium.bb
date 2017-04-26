// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search_answer_web_contents_delegate.h"

#include "base/command_line.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/common/renderer_preferences.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "ui/app_list/app_list_model.h"
#include "ui/app_list/app_list_switches.h"
#include "ui/app_list/search_box_model.h"
#include "ui/views/controls/webview/webview.h"

namespace app_list {

SearchAnswerWebContentsDelegate::SearchAnswerWebContentsDelegate(
    Profile* profile,
    app_list::AppListModel* model)
    : profile_(profile),
      model_(model),
      web_view_(base::MakeUnique<views::WebView>(profile)),
      web_contents_(
          content::WebContents::Create(content::WebContents::CreateParams(
              profile,
              content::SiteInstance::Create(profile)))),
      answer_server_url_(switches::AnswerServerUrl()) {
  content::RendererPreferences* renderer_prefs =
      web_contents_->GetMutableRendererPrefs();
  renderer_prefs->can_accept_load_drops = false;
  // We need the OpenURLFromTab() to get called.
  renderer_prefs->browser_handles_all_top_level_requests = true;
  web_contents_->GetRenderViewHost()->SyncRendererPrefs();

  Observe(web_contents_.get());
  web_contents_->SetDelegate(this);
  web_view_->set_owned_by_client();
  web_view_->SetWebContents(web_contents_.get());
}

SearchAnswerWebContentsDelegate::~SearchAnswerWebContentsDelegate() {}

views::View* SearchAnswerWebContentsDelegate::web_view() {
  return web_view_.get();
}

void SearchAnswerWebContentsDelegate::Update() {
  if (!answer_server_url_.is_valid())
    return;

  // Reset the state.
  received_answer_ = false;
  model_->SetSearchAnswerAvailable(false);
  current_request_url_ = GURL();

  if (!model_->search_engine_is_google())
    return;

  // Start a request to the answer server.
  base::string16 query;
  base::TrimWhitespace(model_->search_box()->text(), base::TRIM_ALL, &query);
  if (query.empty())
    return;

  // Lifetime of |prefixed_query| should be longer than the one of
  // |replacements|.
  base::string16 prefixed_query(base::UTF8ToUTF16("q=") + query);
  GURL::ReplacementsW replacements;
  replacements.SetQueryStr(prefixed_query);
  current_request_url_ = answer_server_url_.ReplaceComponents(replacements);

  content::NavigationController::LoadURLParams load_params(
      current_request_url_);
  load_params.transition_type = ui::PAGE_TRANSITION_AUTO_TOPLEVEL;
  load_params.should_clear_history_list = true;
  web_contents_->GetController().LoadURLWithParams(load_params);

  // We are going to call WebContents::GetPreferredSize().
  web_contents_->GetRenderViewHost()->EnablePreferredSizeMode();
}

void SearchAnswerWebContentsDelegate::UpdatePreferredSize(
    content::WebContents* web_contents,
    const gfx::Size& pref_size) {
  web_view_->SetPreferredSize(pref_size);
}

content::WebContents* SearchAnswerWebContentsDelegate::OpenURLFromTab(
    content::WebContents* source,
    const content::OpenURLParams& params) {
  if (!params.user_gesture)
    return WebContentsDelegate::OpenURLFromTab(source, params);

  // Open the user-clicked link in a new browser tab. This will automatically
  // close the app list.
  chrome::NavigateParams new_tab_params(profile_, params.url,
                                        params.transition);
  new_tab_params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
  new_tab_params.window_action = chrome::NavigateParams::SHOW_WINDOW;

  chrome::Navigate(&new_tab_params);

  return new_tab_params.target_contents;
}

void SearchAnswerWebContentsDelegate::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (navigation_handle->GetURL() != current_request_url_)
    return;

  if (!navigation_handle->HasCommitted() || navigation_handle->IsErrorPage() ||
      !navigation_handle->IsInMainFrame()) {
    return;
  }

  const net::HttpResponseHeaders* headers =
      navigation_handle->GetResponseHeaders();
  if (!headers || headers->response_code() != net::HTTP_OK ||
      !headers->HasHeaderValue("has_answer", "true")) {
    return;
  }

  received_answer_ = true;
}

void SearchAnswerWebContentsDelegate::DidStopLoading() {
  if (!received_answer_)
    return;

  model_->SetSearchAnswerAvailable(true);
}

}  // namespace app_list
