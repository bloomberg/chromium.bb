// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/history/history_types.h"

#include <limits>

#include "base/logging.h"
#include "base/stl_util-inl.h"

using base::Time;
using base::TimeDelta;

namespace history {

// URLRow ----------------------------------------------------------------------

URLRow::URLRow() {
  Initialize();
}

URLRow::URLRow(const GURL& url) : url_(url) {
  // Initialize will not set the URL, so our initialization above will stay.
  Initialize();
}

URLRow::URLRow(const GURL& url, URLID id) : url_(url) {
  // Initialize will not set the URL, so our initialization above will stay.
  Initialize();
  // Initialize will zero the id_, so set it here.
  id_ = id;
}

URLRow::~URLRow() {
}

URLRow& URLRow::operator=(const URLRow& other) {
  id_ = other.id_;
  url_ = other.url_;
  title_ = other.title_;
  visit_count_ = other.visit_count_;
  typed_count_ = other.typed_count_;
  last_visit_ = other.last_visit_;
  hidden_ = other.hidden_;
  favicon_id_ = other.favicon_id_;
  return *this;
}

void URLRow::Swap(URLRow* other) {
  std::swap(id_, other->id_);
  url_.Swap(&other->url_);
  title_.swap(other->title_);
  std::swap(visit_count_, other->visit_count_);
  std::swap(typed_count_, other->typed_count_);
  std::swap(last_visit_, other->last_visit_);
  std::swap(hidden_, other->hidden_);
  std::swap(favicon_id_, other->favicon_id_);
}

void URLRow::Initialize() {
  id_ = 0;
  visit_count_ = 0;
  typed_count_ = 0;
  last_visit_ = Time();
  hidden_ = false;
  favicon_id_ = 0;
}

// VisitRow --------------------------------------------------------------------

VisitRow::VisitRow()
    : visit_id(0),
      url_id(0),
      referring_visit(0),
      transition(PageTransition::LINK),
      segment_id(0),
      is_indexed(false) {
}

VisitRow::VisitRow(URLID arg_url_id,
                   Time arg_visit_time,
                   VisitID arg_referring_visit,
                   PageTransition::Type arg_transition,
                   SegmentID arg_segment_id)
    : visit_id(0),
      url_id(arg_url_id),
      visit_time(arg_visit_time),
      referring_visit(arg_referring_visit),
      transition(arg_transition),
      segment_id(arg_segment_id),
      is_indexed(false) {
}

VisitRow::~VisitRow() {
}

// Favicons -------------------------------------------------------------------

ImportedFavIconUsage::ImportedFavIconUsage() {
}

ImportedFavIconUsage::~ImportedFavIconUsage() {
}

// StarredEntry ----------------------------------------------------------------

StarredEntry::StarredEntry()
    : id(0),
      parent_group_id(0),
      group_id(0),
      visual_order(0),
      type(URL),
      url_id(0) {
}

StarredEntry::~StarredEntry() {
}

void StarredEntry::Swap(StarredEntry* other) {
  std::swap(id, other->id);
  title.swap(other->title);
  std::swap(date_added, other->date_added);
  std::swap(parent_group_id, other->parent_group_id);
  std::swap(group_id, other->group_id);
  std::swap(visual_order, other->visual_order);
  std::swap(type, other->type);
  url.Swap(&other->url);
  std::swap(url_id, other->url_id);
  std::swap(date_group_modified, other->date_group_modified);
}

// URLResult -------------------------------------------------------------------

URLResult::URLResult() {
}

URLResult::URLResult(const GURL& url, base::Time visit_time)
    : URLRow(url),
      visit_time_(visit_time) {
}

URLResult::URLResult(const GURL& url,
                     const Snippet::MatchPositions& title_matches)
    : URLRow(url) {
  title_match_positions_ = title_matches;
}

URLResult::~URLResult() {
}

void URLResult::SwapResult(URLResult* other) {
  URLRow::Swap(other);
  std::swap(visit_time_, other->visit_time_);
  snippet_.Swap(&other->snippet_);
  title_match_positions_.swap(other->title_match_positions_);
}

// QueryResults ----------------------------------------------------------------

QueryResults::QueryResults() : reached_beginning_(false) {
}

QueryResults::~QueryResults() {
  // Free all the URL objects.
  STLDeleteContainerPointers(results_.begin(), results_.end());
}

const size_t* QueryResults::MatchesForURL(const GURL& url,
                                          size_t* num_matches) const {
  URLToResultIndices::const_iterator found = url_to_results_.find(url);
  if (found == url_to_results_.end()) {
    if (num_matches)
      *num_matches = 0;
    return NULL;
  }

  // All entries in the map should have at least one index, otherwise it
  // shouldn't be in the map.
  DCHECK(found->second->size() > 0);
  if (num_matches)
    *num_matches = found->second->size();
  return &found->second->front();
}

void QueryResults::Swap(QueryResults* other) {
  std::swap(first_time_searched_, other->first_time_searched_);
  std::swap(reached_beginning_, other->reached_beginning_);
  results_.swap(other->results_);
  url_to_results_.swap(other->url_to_results_);
}

void QueryResults::AppendURLBySwapping(URLResult* result) {
  URLResult* new_result = new URLResult;
  new_result->SwapResult(result);

  results_.push_back(new_result);
  AddURLUsageAtIndex(new_result->url(), results_.size() - 1);
}

void QueryResults::AppendResultsBySwapping(QueryResults* other,
                                           bool remove_dupes) {
  if (remove_dupes) {
    // Delete all entries in the other array that are already in this one.
    for (size_t i = 0; i < results_.size(); i++)
      other->DeleteURL(results_[i]->url());
  }

  if (first_time_searched_ > other->first_time_searched_)
    std::swap(first_time_searched_, other->first_time_searched_);

  if (reached_beginning_ != other->reached_beginning_)
    std::swap(reached_beginning_, other->reached_beginning_);

  for (size_t i = 0; i < other->results_.size(); i++) {
    // Just transfer pointer ownership.
    results_.push_back(other->results_[i]);
    AddURLUsageAtIndex(results_.back()->url(), results_.size() - 1);
  }

  // We just took ownership of all the results in the input vector.
  other->results_.clear();
  other->url_to_results_.clear();
}

void QueryResults::DeleteURL(const GURL& url) {
  // Delete all instances of this URL. We re-query each time since each
  // mutation will cause the indices to change.
  while (const size_t* match_indices = MatchesForURL(url, NULL))
    DeleteRange(*match_indices, *match_indices);
}

void QueryResults::DeleteRange(size_t begin, size_t end) {
  DCHECK(begin <= end && begin < size() && end < size());

  // First delete the pointers in the given range and store all the URLs that
  // were modified. We will delete references to these later.
  std::set<GURL> urls_modified;
  for (size_t i = begin; i <= end; i++) {
    urls_modified.insert(results_[i]->url());
    delete results_[i];
    results_[i] = NULL;
  }

  // Now just delete that range in the vector en masse (the STL ending is
  // exclusive, while ours is inclusive, hence the +1).
  results_.erase(results_.begin() + begin, results_.begin() + end + 1);

  // Delete the indicies referencing the deleted entries.
  for (std::set<GURL>::const_iterator url = urls_modified.begin();
       url != urls_modified.end(); ++url) {
    URLToResultIndices::iterator found = url_to_results_.find(*url);
    if (found == url_to_results_.end()) {
      NOTREACHED();
      continue;
    }

    // Need a signed loop type since we do -- which may take us to -1.
    for (int match = 0; match < static_cast<int>(found->second->size());
         match++) {
      if (found->second[match] >= begin && found->second[match] <= end) {
        // Remove this referece from the list.
        found->second->erase(found->second->begin() + match);
        match--;
      }
    }

    // Clear out an empty lists if we just made one.
    if (found->second->empty())
      url_to_results_.erase(found);
  }

  // Shift all other indices over to account for the removed ones.
  AdjustResultMap(end + 1, std::numeric_limits<size_t>::max(),
                  -static_cast<ptrdiff_t>(end - begin + 1));
}

void QueryResults::AddURLUsageAtIndex(const GURL& url, size_t index) {
  URLToResultIndices::iterator found = url_to_results_.find(url);
  if (found != url_to_results_.end()) {
    // The URL is already in the list, so we can just append the new index.
    found->second->push_back(index);
    return;
  }

  // Need to add a new entry for this URL.
  StackVector<size_t, 4> new_list;
  new_list->push_back(index);
  url_to_results_[url] = new_list;
}

void QueryResults::AdjustResultMap(size_t begin, size_t end, ptrdiff_t delta) {
  for (URLToResultIndices::iterator i = url_to_results_.begin();
       i != url_to_results_.end(); ++i) {
    for (size_t match = 0; match < i->second->size(); match++) {
      size_t match_index = i->second[match];
      if (match_index >= begin && match_index <= end)
        i->second[match] += delta;
    }
  }
}

// QueryOptions ----------------------------------------------------------------

QueryOptions::QueryOptions() : max_count(0) {}

void QueryOptions::SetRecentDayRange(int days_ago) {
  end_time = base::Time::Now();
  begin_time = end_time - base::TimeDelta::FromDays(days_ago);
}

// KeywordSearchTermVisit -----------------------------------------------------

KeywordSearchTermVisit::KeywordSearchTermVisit() {}

KeywordSearchTermVisit::~KeywordSearchTermVisit() {}

// KeywordSearchTermRow --------------------------------------------------------

KeywordSearchTermRow::KeywordSearchTermRow() : keyword_id(0), url_id(0) {}

KeywordSearchTermRow::~KeywordSearchTermRow() {}

// MostVisitedURL --------------------------------------------------------------

MostVisitedURL::MostVisitedURL() {}

MostVisitedURL::MostVisitedURL(const GURL& in_url,
                               const GURL& in_favicon_url,
                               const string16& in_title)
    : url(in_url),
      favicon_url(in_favicon_url),
      title(in_title) {
}

MostVisitedURL::~MostVisitedURL() {}

// Images ---------------------------------------------------------------------

Images::Images() {}

Images::~Images() {}

// TopSitesDelta --------------------------------------------------------------

TopSitesDelta::TopSitesDelta() {}

TopSitesDelta::~TopSitesDelta() {}

// HistoryAddPageArgs ---------------------------------------------------------

HistoryAddPageArgs::HistoryAddPageArgs(
    const GURL& arg_url,
    base::Time arg_time,
    const void* arg_id_scope,
    int32 arg_page_id,
    const GURL& arg_referrer,
    const history::RedirectList& arg_redirects,
    PageTransition::Type arg_transition,
    VisitSource arg_source,
    bool arg_did_replace_entry)
      : url(arg_url),
        time(arg_time),
        id_scope(arg_id_scope),
        page_id(arg_page_id),
        referrer(arg_referrer),
        redirects(arg_redirects),
        transition(arg_transition),
        visit_source(arg_source),
        did_replace_entry(arg_did_replace_entry) {
}

HistoryAddPageArgs::~HistoryAddPageArgs() {}

HistoryAddPageArgs* HistoryAddPageArgs::Clone() const {
  return new HistoryAddPageArgs(
      url, time, id_scope, page_id, referrer, redirects, transition,
      visit_source, did_replace_entry);
}

ThumbnailMigration::ThumbnailMigration() {}

ThumbnailMigration::~ThumbnailMigration() {}

MostVisitedThumbnails::MostVisitedThumbnails() {}

MostVisitedThumbnails::~MostVisitedThumbnails() {}

}  // namespace history
