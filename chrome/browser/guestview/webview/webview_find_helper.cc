// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/guestview/webview/webview_find_helper.h"

#include <utility>

#include "chrome/browser/extensions/api/webview/webview_api.h"
#include "chrome/browser/guestview/webview/webview_constants.h"

WebviewFindHelper::WebviewFindHelper()
    : current_find_request_id_(0) {
}

WebviewFindHelper::~WebviewFindHelper() {
}

void WebviewFindHelper::CancelAllFindSessions() {
  current_find_session_ = linked_ptr<WebviewFindHelper::FindInfo>();
  while (!find_info_map_.empty()) {
    find_info_map_.begin()->second->SendResponse(true /* canceled */);
    find_info_map_.erase(find_info_map_.begin());
  }
}

void WebviewFindHelper::EndFindSession(int session_request_id, bool canceled) {
  FindInfoMap::iterator session_iterator =
      find_info_map_.find(session_request_id);
  DCHECK(session_iterator != find_info_map_.end());
  FindInfo* find_info = session_iterator->second.get();

  // Call the callback function of the first request of the find session.
  find_info->SendResponse(canceled);

  // For every subsequent find request of the find session.
  for (std::vector<base::WeakPtr<WebviewFindHelper::FindInfo> >::iterator i =
           find_info->find_next_requests_.begin();
       i != find_info->find_next_requests_.end(); ++i) {
    DCHECK(i->get());

    // Do not call callbacks for subsequent find requests that have not been
    // replied to yet. These requests will get their own final updates in the
    // same order as they appear in |find_next_requests_|, i.e. the order that
    // the requests were made in. Once one request is found that has not been
    // replied to, none that follow will be replied to either, and do not need
    // to be checked.
    if (!(*i)->replied_)
      break;

    // Update the request's number of matches (if not canceled).
    if (!canceled) {
      (*i)->find_results_.number_of_matches_ =
          find_info->find_results_.number_of_matches_;
    }

    // Call the request's callback function with the find results, and then
    // delete its map entry to free the WebviewFindFunction object.
    (*i)->SendResponse(canceled);
    find_info_map_.erase((*i)->request_id_);
  }

  // Erase the first find request's map entry to free the WebviewFindFunction
  // object.
  find_info_map_.erase(session_request_id);
}

void WebviewFindHelper::Find(
    content::WebContents* guest_web_contents,
    const base::string16& search_text,
    const blink::WebFindOptions& options,
    scoped_refptr<extensions::WebviewFindFunction> find_function) {
  // Need a new request_id for each new find request.
  ++current_find_request_id_;

  // Stores the find request information by request_id so that its callback
  // function can be called when the find results are available.
  std::pair<FindInfoMap::iterator, bool> insert_result =
      find_info_map_.insert(
          std::make_pair(current_find_request_id_,
                         linked_ptr<WebviewFindHelper::FindInfo>(
                             new WebviewFindHelper::FindInfo(
                                 current_find_request_id_,
                                 search_text,
                                 options,
                                 find_function))));
  // No duplicate insertions.
  DCHECK(insert_result.second);

  // Find options including the implicit |findNext| field.
  blink::WebFindOptions* full_options = insert_result.first->second->options();

  // Set |findNext| implicitly.
  if (current_find_session_.get()) {
    const base::string16& current_search_text =
        current_find_session_->search_text();
    bool current_match_case = current_find_session_->options()->matchCase;
    full_options->findNext = !current_search_text.empty() &&
        current_search_text == search_text &&
        current_match_case == options.matchCase;
  } else {
    full_options->findNext = false;
  }

  // Link find requests that are a part of the same find session.
  if (full_options->findNext && current_find_session_.get()) {
    DCHECK(current_find_request_id_ != current_find_session_->request_id());
    current_find_session_->AddFindNextRequest(
        insert_result.first->second->AsWeakPtr());
  }

  // Update the current find session, if necessary.
  if (!full_options->findNext)
    current_find_session_ = insert_result.first->second;

  guest_web_contents->Find(current_find_request_id_,
                           search_text, *full_options);
}

void WebviewFindHelper::FindReply(int request_id,
                                  int number_of_matches,
                                  const gfx::Rect& selection_rect,
                                  int active_match_ordinal,
                                  bool final_update) {
  FindInfoMap::iterator find_iterator = find_info_map_.find(request_id);

  // Ignore slow replies to canceled find requests.
  if (find_iterator == find_info_map_.end())
    return;

  // This find request must be a part of an existing find session.
  DCHECK(current_find_session_.get());

  WebviewFindHelper::FindInfo* find_info = find_iterator->second.get();

  // Handle canceled find requests.
  if (!find_info->options()->findNext &&
      find_info_map_.begin()->first < request_id) {
    DCHECK_NE(current_find_session_->request_id(),
              find_info_map_.begin()->first);
    EndFindSession(find_info_map_.begin()->first, true /* canceled */);
  }

  // Aggregate the find results.
  find_info->AggregateResults(number_of_matches, selection_rect,
                              active_match_ordinal, final_update);

  // Call the callback functions of completed find requests.
  if (final_update)
    EndFindSession(request_id, false /* canceled */);
}

WebviewFindHelper::FindResults::FindResults()
    : number_of_matches_(0),
      active_match_ordinal_(0) {}

WebviewFindHelper::FindResults::~FindResults() {
}

void WebviewFindHelper::FindResults::AggregateResults(
    int number_of_matches,
    const gfx::Rect& selection_rect,
    int active_match_ordinal,
    bool final_update) {
  if (number_of_matches != -1)
    number_of_matches_ = number_of_matches;

  if (active_match_ordinal != -1)
    active_match_ordinal_ = active_match_ordinal;

  if (final_update && active_match_ordinal_ == 0) {
    // No match found, so the selection rectangle is empty.
    selection_rect_ = gfx::Rect();
  } else if (!selection_rect.IsEmpty()) {
    selection_rect_ = selection_rect;
  }
}

void WebviewFindHelper::FindResults::PrepareResults(
    base::DictionaryValue* results) {
  results->SetInteger(webview::kFindNumberOfMatches, number_of_matches_);
  results->SetInteger(webview::kFindActiveMatchOrdinal, active_match_ordinal_);
  base::DictionaryValue rect;
  rect.SetInteger(webview::kFindRectLeft, selection_rect_.x());
  rect.SetInteger(webview::kFindRectTop, selection_rect_.y());
  rect.SetInteger(webview::kFindRectWidth, selection_rect_.width());
  rect.SetInteger(webview::kFindRectHeight, selection_rect_.height());
  results->Set(webview::kFindSelectionRect, rect.DeepCopy());
}

WebviewFindHelper::FindInfo::FindInfo(
    int request_id,
    const base::string16& search_text,
    const blink::WebFindOptions& options,
    scoped_refptr<extensions::WebviewFindFunction> find_function)
    : request_id_(request_id),
      search_text_(search_text),
      options_(options),
      find_function_(find_function),
      replied_(false),
      weak_ptr_factory_(this) {
}

WebviewFindHelper::FindInfo::~FindInfo() {
}

void WebviewFindHelper::FindInfo::AggregateResults(
    int number_of_matches,
    const gfx::Rect& selection_rect,
    int active_match_ordinal,
    bool final_update) {
  replied_ = true;
  find_results_.AggregateResults(number_of_matches, selection_rect,
                                 active_match_ordinal, final_update);
}

base::WeakPtr<WebviewFindHelper::FindInfo>
    WebviewFindHelper::FindInfo::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void WebviewFindHelper::FindInfo::SendResponse(bool canceled) {
  // Prepare the find results to pass to the callback function.
  base::DictionaryValue results;
  find_results_.PrepareResults(&results);
  results.SetBoolean(webview::kFindCanceled, canceled);

  // Call the callback.
  find_function_->SetResult(results.DeepCopy());
  find_function_->SendResponse(true);
}
