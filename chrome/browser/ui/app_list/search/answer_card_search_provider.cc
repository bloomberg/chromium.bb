// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/answer_card_search_provider.h"

#include "base/command_line.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/search/answer_card_result.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/common/renderer_preferences.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "ui/app_list/app_list_features.h"
#include "ui/app_list/app_list_model.h"
#include "ui/app_list/search_box_model.h"
#include "ui/views/controls/webview/web_contents_set_background_color.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/widget/widget.h"

namespace app_list {

namespace {

enum class SearchAnswerRequestResult {
  REQUEST_RESULT_ANOTHER_REQUEST_STARTED = 0,
  REQUEST_RESULT_REQUEST_FAILED = 1,
  REQUEST_RESULT_NO_ANSWER = 2,
  REQUEST_RESULT_RECEIVED_ANSWER = 3,
  REQUEST_RESULT_RECEIVED_ANSWER_TOO_LARGE = 4,
  REQUEST_RESULT_MAX = 5
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

    if (GetWidget() && GetWidget()->IsVisible() && IsDrawn()) {
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

AnswerCardSearchProvider::AnswerCardSearchProvider(
    Profile* profile,
    app_list::AppListModel* model,
    AppListControllerDelegate* list_controller)
    : profile_(profile),
      model_(model),
      list_controller_(list_controller),
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

  // Make the webview transparent since it's going to be shown on top of a
  // highlightable button.
  views::WebContentsSetBackgroundColor::CreateForWebContentsWithColor(
      web_contents_.get(), SK_ColorTRANSPARENT);
}

AnswerCardSearchProvider::~AnswerCardSearchProvider() {
  RecordReceivedAnswerFinalResult();
}

views::View* AnswerCardSearchProvider::web_view() {
  return web_view_.get();
}

void AnswerCardSearchProvider::Start(bool is_voice_query,
                                     const base::string16& query) {
  RecordReceivedAnswerFinalResult();
  // Reset the state.
  received_answer_ = false;
  OnResultAvailable(false);
  current_request_url_ = GURL();
  result_url_.empty();
  result_title_.empty();
  server_request_start_time_ = answer_loaded_time_ = base::TimeTicks();

  if (is_voice_query) {
    // No need to send a server request and show a card because launcher
    // automatically closes upon voice queries.
    return;
  }

  if (!model_->search_engine_is_google())
    return;

  if (query.empty())
    return;

  // Start a request to the answer server.

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

void AnswerCardSearchProvider::UpdatePreferredSize(
    content::WebContents* web_contents,
    const gfx::Size& pref_size) {
  OnResultAvailable(received_answer_ && IsCardSizeOk() &&
                    !web_contents_->IsLoading());
  web_view_->SetPreferredSize(pref_size);
  if (!answer_loaded_time_.is_null()) {
    UMA_HISTOGRAM_TIMES("SearchAnswer.ResizeAfterLoadTime",
                        base::TimeTicks::Now() - answer_loaded_time_);
  }
}

content::WebContents* AnswerCardSearchProvider::OpenURLFromTab(
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

bool AnswerCardSearchProvider::HandleContextMenu(
    const content::ContextMenuParams& params) {
  // Disable showing the menu.
  return true;
}

void AnswerCardSearchProvider::DidFinishNavigation(
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
    if (!ParseResponseHeaders(navigation_handle->GetResponseHeaders())) {
      RecordRequestResult(SearchAnswerRequestResult::REQUEST_RESULT_NO_ANSWER);
      return;
    }
  } else {
    // In the dark run mode, every other "server response" contains a card.
    dark_run_received_answer_ = !dark_run_received_answer_;
    if (!dark_run_received_answer_)
      return;
    // SearchResult requires a non-empty id. This "url" will never be opened.
    result_url_ = "some string";
  }

  received_answer_ = true;
  UMA_HISTOGRAM_TIMES("SearchAnswer.NavigationTime",
                      base::TimeTicks::Now() - server_request_start_time_);
}

void AnswerCardSearchProvider::DidStopLoading() {
  if (!received_answer_)
    return;

  if (IsCardSizeOk())
    OnResultAvailable(true);
  answer_loaded_time_ = base::TimeTicks::Now();
  UMA_HISTOGRAM_TIMES("SearchAnswer.LoadingTime",
                      answer_loaded_time_ - server_request_start_time_);
  base::RecordAction(base::UserMetricsAction("SearchAnswer_StoppedLoading"));
}

void AnswerCardSearchProvider::DidGetUserInteraction(
    const blink::WebInputEvent::Type type) {
  base::RecordAction(base::UserMetricsAction("SearchAnswer_UserInteraction"));
}

bool AnswerCardSearchProvider::IsCardSizeOk() const {
  if (features::IsAnswerCardDarkRunEnabled())
    return true;

  const gfx::Size size = web_contents_->GetPreferredSize();
  return size.width() <= features::AnswerCardMaxWidth() &&
         size.height() <= features::AnswerCardMaxHeight();
}

void AnswerCardSearchProvider::RecordReceivedAnswerFinalResult() {
  // Recording whether a server response with an answer contains a card of a
  // fitting size, or a too large one. Cannot do this in DidStopLoading() or
  // UpdatePreferredSize() because this may be followed by a resizing with
  // different dimensions, so this method gets called when card's life ends.
  if (!received_answer_)
    return;

  RecordRequestResult(
      IsCardSizeOk() ? SearchAnswerRequestResult::REQUEST_RESULT_RECEIVED_ANSWER
                     : SearchAnswerRequestResult::
                           REQUEST_RESULT_RECEIVED_ANSWER_TOO_LARGE);
}

void AnswerCardSearchProvider::OnResultAvailable(bool is_available) {
  SearchProvider::Results results;
  if (is_available) {
    results.reserve(1);
    results.emplace_back(base::MakeUnique<AnswerCardResult>(
        profile_, list_controller_, result_url_,
        base::UTF8ToUTF16(result_title_), web_view_.get(),
        web_contents_.get()));
  }
  SwapResults(&results);
}

bool AnswerCardSearchProvider::ParseResponseHeaders(
    const net::HttpResponseHeaders* headers) {
  if (!headers || headers->response_code() != net::HTTP_OK)
    return false;
  if (!headers->HasHeaderValue("SearchAnswer-HasResult", "true"))
    return false;
  if (!headers->GetNormalizedHeader("SearchAnswer-OpenResultUrl", &result_url_))
    return false;
  if (!headers->GetNormalizedHeader("SearchAnswer-Title", &result_title_))
    return false;
  return true;
}

}  // namespace app_list
