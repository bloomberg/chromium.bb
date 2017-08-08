// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/answer_card/answer_card_search_provider.h"

#include <utility>

#include "base/command_line.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/app_list/search/answer_card/answer_card_result.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/search_engines/template_url_service.h"
#include "ui/app_list/app_list_features.h"
#include "ui/app_list/app_list_model.h"
#include "ui/app_list/search_box_model.h"

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

}  // namespace

AnswerCardSearchProvider::AnswerCardSearchProvider(
    Profile* profile,
    app_list::AppListModel* model,
    AppListControllerDelegate* list_controller,
    std::unique_ptr<AnswerCardContents> contents)
    : profile_(profile),
      model_(model),
      list_controller_(list_controller),
      contents_(std::move(contents)),
      answer_server_url_(features::AnswerServerUrl()),
      template_url_service_(TemplateURLServiceFactory::GetForProfile(profile)) {
  contents_->SetDelegate(this);
}

AnswerCardSearchProvider::~AnswerCardSearchProvider() {
  RecordReceivedAnswerFinalResult();
}

void AnswerCardSearchProvider::Start(bool is_voice_query,
                                     const base::string16& query) {
  RecordReceivedAnswerFinalResult();
  // Reset the state.
  received_answer_ = false;
  OnResultAvailable(false);
  current_request_url_ = GURL();
  result_url_.clear();
  result_title_.clear();
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
  const std::string prefixed_query(
      "q=" + net::EscapeQueryParamValue(base::UTF16ToUTF8(query), true) +
      features::AnswerServerQuerySuffix());
  GURL::Replacements replacements;
  replacements.SetQueryStr(prefixed_query);
  current_request_url_ = answer_server_url_.ReplaceComponents(replacements);
  contents_->LoadURL(current_request_url_);

  server_request_start_time_ = base::TimeTicks::Now();
}

void AnswerCardSearchProvider::UpdatePreferredSize(const gfx::Size& pref_size) {
  preferred_size_ = pref_size;
  OnResultAvailable(received_answer_ && IsCardSizeOk() &&
                    !contents_->IsLoading());
  if (!answer_loaded_time_.is_null()) {
    UMA_HISTOGRAM_TIMES("SearchAnswer.ResizeAfterLoadTime",
                        base::TimeTicks::Now() - answer_loaded_time_);
  }
}

void AnswerCardSearchProvider::DidFinishNavigation(
    const GURL& url,
    bool has_error,
    bool has_answer_card,
    const std::string& result_title,
    const std::string& issued_query) {
  if (url != current_request_url_) {
    // TODO(vadimt): Remove this and similar logging once testing is complete if
    // we think this is not useful after release or happens too frequently.
    VLOG(1) << "DidFinishNavigation: Another request started";
    RecordRequestResult(
        SearchAnswerRequestResult::REQUEST_RESULT_ANOTHER_REQUEST_STARTED);
    return;
  }

  VLOG(1) << "DidFinishNavigation: Latest request completed";
  if (has_error) {
    RecordRequestResult(
        SearchAnswerRequestResult::REQUEST_RESULT_REQUEST_FAILED);
    return;
  }

  if (!features::IsAnswerCardDarkRunEnabled()) {
    if (!has_answer_card) {
      RecordRequestResult(SearchAnswerRequestResult::REQUEST_RESULT_NO_ANSWER);
      return;
    }
    DCHECK(!result_title.empty());
    DCHECK(!issued_query.empty());
    result_title_ = result_title;
    result_url_ = GetResultUrl(base::UTF8ToUTF16(issued_query));
  } else {
    // In the dark run mode, every other "server response" contains a card.
    dark_run_received_answer_ = !dark_run_received_answer_;
    if (!dark_run_received_answer_)
      return;
    // SearchResult requires a non-empty id. This "url" will never be opened.
    result_url_ = "https://www.google.com/?q=something";
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

bool AnswerCardSearchProvider::IsCardSizeOk() const {
  if (features::IsAnswerCardDarkRunEnabled())
    return true;

  if (preferred_size_.width() <= features::AnswerCardMaxWidth() &&
      preferred_size_.height() <= features::AnswerCardMaxHeight()) {
    return true;
  }

  LOG(ERROR) << "Card is too large: width=" << preferred_size_.width()
             << ", height=" << preferred_size_.height();
  return false;
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

    const GURL stripped_result_url = AutocompleteMatch::GURLToStrippedGURL(
        GURL(result_url_), AutocompleteInput(), template_url_service_,
        base::string16() /* keyword */);

    results.emplace_back(base::MakeUnique<AnswerCardResult>(
        profile_, list_controller_, result_url_, stripped_result_url.spec(),
        base::UTF8ToUTF16(result_title_), contents_.get()));
  }
  SwapResults(&results);
}

std::string AnswerCardSearchProvider::GetResultUrl(
    const base::string16& query) const {
  return template_url_service_->GetDefaultSearchProvider()
      ->url_ref()
      .ReplaceSearchTerms(TemplateURLRef::SearchTermsArgs(query),
                          template_url_service_->search_terms_data());
}

}  // namespace app_list
