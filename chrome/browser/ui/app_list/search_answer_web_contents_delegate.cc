// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search_answer_web_contents_delegate.h"

#include "base/command_line.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
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
#include "ui/app_list/app_list_features.h"
#include "ui/app_list/app_list_model.h"
#include "ui/app_list/search_box_model.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/widget/widget.h"

namespace app_list {

namespace {

enum class SearchAnswerRequestResult {
  REQUEST_RESULT_ANOTHER_REQUEST_STARTED = 0,
  REQUEST_RESULT_REQUEST_FAILED = 1,
  REQUEST_RESULT_NO_ANSWER = 2,
  REQUEST_RESULT_RECEIVED_ANSWER = 3,
  REQUEST_RESULT_MAX = 4
};

void RecordRequestResult(SearchAnswerRequestResult request_result) {
  UMA_HISTOGRAM_ENUMERATION("SearchAnswer.RequestResult", request_result,
                            SearchAnswerRequestResult::REQUEST_RESULT_MAX);
}

class SearchAnswerWebView : public views::WebView {
 public:
  explicit SearchAnswerWebView(content::BrowserContext* browser_context)
      : WebView(browser_context) {}

  // views::WebView overrides:
  void VisibilityChanged(View* starting_from, bool is_visible) override {
    WebView::VisibilityChanged(starting_from, is_visible);

    if (GetWidget()->IsVisible() && IsDrawn()) {
      if (shown_time_.is_null())
        shown_time_ = base::TimeTicks::Now();
    } else {
      if (!shown_time_.is_null()) {
        UMA_HISTOGRAM_MEDIUM_TIMES("SearchAnswer.AnswerVisibleTime",
                                   base::TimeTicks::Now() - shown_time_);
        shown_time_ = base::TimeTicks();
      }
    }
  }

  const char* GetClassName() const override { return "SearchAnswerWebView"; }

 private:
  // Time when the answer became visible to the user.
  base::TimeTicks shown_time_;

  DISALLOW_COPY_AND_ASSIGN(SearchAnswerWebView);
};

}  // namespace

SearchAnswerWebContentsDelegate::SearchAnswerWebContentsDelegate(
    Profile* profile,
    app_list::AppListModel* model)
    : profile_(profile),
      model_(model),
      web_view_(base::MakeUnique<SearchAnswerWebView>(profile)),
      web_contents_(
          content::WebContents::Create(content::WebContents::CreateParams(
              profile,
              content::SiteInstance::Create(profile)))),
      answer_server_url_(features::AnswerServerUrl()) {
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
  if (features::IsAnswerCardDarkRunEnabled())
    web_view_->SetFocusBehavior(views::View::FocusBehavior::NEVER);

  model->AddObserver(this);
}

SearchAnswerWebContentsDelegate::~SearchAnswerWebContentsDelegate() {
  model_->RemoveObserver(this);
}

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
  server_request_start_time_ = answer_loaded_time_ = base::TimeTicks();

  if (model_->search_box()->is_voice_query()) {
    // No need to send a server request and show a card because launcher
    // automatically closes upon voice queries.
    return;
  }

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
  server_request_start_time_ = base::TimeTicks::Now();

  // We are going to call WebContents::GetPreferredSize().
  web_contents_->GetRenderViewHost()->EnablePreferredSizeMode();
}

void SearchAnswerWebContentsDelegate::UpdatePreferredSize(
    content::WebContents* web_contents,
    const gfx::Size& pref_size) {
  if (!features::IsAnswerCardDarkRunEnabled())
    web_view_->SetPreferredSize(pref_size);
  if (!answer_loaded_time_.is_null()) {
    UMA_HISTOGRAM_TIMES("SearchAnswer.ResizeAfterLoadTime",
                        base::TimeTicks::Now() - answer_loaded_time_);
  }
}

content::WebContents* SearchAnswerWebContentsDelegate::OpenURLFromTab(
    content::WebContents* source,
    const content::OpenURLParams& params) {
  if (!params.user_gesture)
    return WebContentsDelegate::OpenURLFromTab(source, params);

  // Open the user-clicked link in the browser taking into account the requested
  // disposition.
  chrome::NavigateParams new_tab_params(profile_, params.url,
                                        params.transition);

  new_tab_params.disposition = params.disposition;

  if (params.disposition == WindowOpenDisposition::NEW_BACKGROUND_TAB) {
    // When the user asks to open a link as a background tab, we show an
    // activated window with the new activated tab after the user closes the
    // launcher. So it's "background" relative to the launcher itself.
    new_tab_params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
    new_tab_params.window_action = chrome::NavigateParams::SHOW_WINDOW_INACTIVE;
  }

  chrome::Navigate(&new_tab_params);

  base::RecordAction(base::UserMetricsAction("SearchAnswer_OpenedUrl"));

  return new_tab_params.target_contents;
}

bool SearchAnswerWebContentsDelegate::HandleContextMenu(
    const content::ContextMenuParams& params) {
  // Disable showing the menu.
  return true;
}

void SearchAnswerWebContentsDelegate::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (navigation_handle->GetURL() != current_request_url_) {
    RecordRequestResult(
        SearchAnswerRequestResult::REQUEST_RESULT_ANOTHER_REQUEST_STARTED);
    return;
  }

  if (!navigation_handle->HasCommitted() || navigation_handle->IsErrorPage() ||
      !navigation_handle->IsInMainFrame()) {
    RecordRequestResult(
        SearchAnswerRequestResult::REQUEST_RESULT_REQUEST_FAILED);
    return;
  }

  if (!features::IsAnswerCardDarkRunEnabled()) {
    const net::HttpResponseHeaders* headers =
        navigation_handle->GetResponseHeaders();
    if (!headers || headers->response_code() != net::HTTP_OK ||
        !headers->HasHeaderValue("has_answer", "true")) {
      RecordRequestResult(SearchAnswerRequestResult::REQUEST_RESULT_NO_ANSWER);
      return;
    }
  } else {
    // In the dark run mode, every other "server response" contains a card.
    dark_run_received_answer_ = !dark_run_received_answer_;
    if (!dark_run_received_answer_)
      return;
  }

  received_answer_ = true;
  UMA_HISTOGRAM_TIMES("SearchAnswer.NavigationTime",
                      base::TimeTicks::Now() - server_request_start_time_);
  RecordRequestResult(
      SearchAnswerRequestResult::REQUEST_RESULT_RECEIVED_ANSWER);
}

void SearchAnswerWebContentsDelegate::DidStopLoading() {
  if (!received_answer_)
    return;

  model_->SetSearchAnswerAvailable(true);
  answer_loaded_time_ = base::TimeTicks::Now();
  UMA_HISTOGRAM_TIMES("SearchAnswer.LoadingTime",
                      answer_loaded_time_ - server_request_start_time_);
  base::RecordAction(base::UserMetricsAction("SearchAnswer_StoppedLoading"));
}

void SearchAnswerWebContentsDelegate::DidGetUserInteraction(
    const blink::WebInputEvent::Type type) {
  base::RecordAction(base::UserMetricsAction("SearchAnswer_UserInteraction"));
}

void SearchAnswerWebContentsDelegate::OnSearchEngineIsGoogleChanged(
    bool is_google) {
  Update();
}

}  // namespace app_list
