// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/search_metadata.h"

#include <algorithm>
#include <queue>

#include "base/bind.h"
#include "base/string_util.h"
#include "chrome/browser/chromeos/drive/file_system_util.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/escape.h"

using content::BrowserThread;

namespace drive {

namespace {

// Used to sort the result canididates per the last accessed/modified time. The
// recently accessed/modified files come first.
bool CompareByTimestamp(const ResourceEntry& a, const ResourceEntry& b) {
  const PlatformFileInfoProto& a_file_info = a.file_info();
  const PlatformFileInfoProto& b_file_info = b.file_info();

  if (a_file_info.last_accessed() != b_file_info.last_accessed())
    return a_file_info.last_accessed() > b_file_info.last_accessed();

  // When the entries have the same last access time (which happens quite often
  // because Drive server doesn't set the field until an entry is viewed via
  // drive.google.com), we use last modified time as the tie breaker.
  return a_file_info.last_modified() > b_file_info.last_modified();
}

struct MetadataSearchResultComparator {
  bool operator()(const MetadataSearchResult* a,
                  const MetadataSearchResult* b) const {
    return CompareByTimestamp(a->entry, b->entry);
  }
};

// A wrapper of std::priority_queue which deals with pointers of values.
template<typename T, typename Compare>
class ScopedPriorityQueue {
 public:
  ScopedPriorityQueue() {}

  ~ScopedPriorityQueue() {
    while (!empty())
      pop();
  }

  bool empty() const { return queue_.empty(); }

  size_t size() const { return queue_.size(); }

  const T* top() const { return queue_.top(); }

  void push(T* x) { queue_.push(x); }

  void pop() {
    delete queue_.top();
    queue_.pop();
  }

 private:
  std::priority_queue<T*, std::vector<T*>, Compare> queue_;

  DISALLOW_COPY_AND_ASSIGN(ScopedPriorityQueue);
};

// Returns true if |entry| is eligible for the search |options| and should be
// tested for the match with the query.
// If SEARCH_METADATA_EXCLUDE_HOSTED_DOCUMENTS is requested, the hosted
// documents are skipped. If SEARCH_METADATA_EXCLUDE_DIRECTORIES is requested,
// the directories are skipped. If SEARCH_METADATA_SHARED_WITH_ME is requested,
// only the entries with shared-with-me label will be tested.
bool IsEligibleEntry(const ResourceEntry& entry, int options) {
  if ((options & SEARCH_METADATA_EXCLUDE_HOSTED_DOCUMENTS) &&
      entry.file_specific_info().is_hosted_document())
    return false;

  if ((options & SEARCH_METADATA_EXCLUDE_DIRECTORIES) &&
      entry.file_info().is_directory())
    return false;

  if (options & SEARCH_METADATA_SHARED_WITH_ME)
    return entry.shared_with_me();

  // Exclude "drive", "drive/root", and "drive/other".
  if (entry.resource_id() == util::kDriveGrandRootSpecialResourceId ||
      entry.parent_resource_id() == util::kDriveGrandRootSpecialResourceId) {
    return false;
  }

  return true;
}

}  // namespace

// Helper class for searching the local resource metadata.
class SearchMetadataHelper {
 public:
  SearchMetadataHelper(internal::ResourceMetadata* resource_metadata,
                       const std::string& query,
                       int options,
                       int at_most_num_matches,
                       const SearchMetadataCallback& callback)
    : resource_metadata_(resource_metadata),
      query_(query),
      options_(options),
      at_most_num_matches_(at_most_num_matches),
      callback_(callback),
      results_(new MetadataSearchResultVector),
      weak_ptr_factory_(this) {
    DCHECK_LE(0, at_most_num_matches);
  }

  // Starts searching the local resource metadata.
  void Start() {
    resource_metadata_->IterateEntries(
        base::Bind(&SearchMetadataHelper::MaybeAddEntryToResult,
                   base::Unretained(this)),
        base::Bind(&SearchMetadataHelper::PrepareResults,
                   weak_ptr_factory_.GetWeakPtr()));
  }


 private:
  // Adds entry to the result when appropriate.
  void MaybeAddEntryToResult(const ResourceEntry& entry) {
    DCHECK_GE(at_most_num_matches_,
              static_cast<int>(result_candidates_.size()));

    // Add |entry| to the result if the entry is eligible for the given
    // |options| and matches the query. The base name of the entry must
    // contains |query| to match the query.
    std::string highlighted;
    if (!IsEligibleEntry(entry, options_) ||
        !FindAndHighlight(entry.base_name(), query_, &highlighted))
      return;

    // Make space for |entry| when appropriate.
    if (static_cast<int>(result_candidates_.size()) == at_most_num_matches_ &&
        CompareByTimestamp(entry, result_candidates_.top()->entry))
      result_candidates_.pop();

    // Add |entry| to the result when appropriate.
    if (static_cast<int>(result_candidates_.size()) < at_most_num_matches_) {
      result_candidates_.push(new MetadataSearchResult(
          base::FilePath(), entry, highlighted));
    }
  }

  // Prepares results by popping candidates one by one.
  void PrepareResults() {
    if (result_candidates_.empty()) {
      Finish(FILE_ERROR_OK);
      return;
    }
    resource_metadata_->GetEntryInfoByResourceId(
        result_candidates_.top()->entry.resource_id(),
        base::Bind(&SearchMetadataHelper::ContinuePrepareResults,
                   weak_ptr_factory_.GetWeakPtr()));
  }

  // Implements PrepareResults().
  void ContinuePrepareResults(FileError error,
                              const base::FilePath& path,
                              scoped_ptr<ResourceEntry> unused_entry) {
    if (error != FILE_ERROR_OK) {
      Finish(error);
      return;
    }
    results_->push_back(*result_candidates_.top());
    results_->back().path = path;
    result_candidates_.pop();
    PrepareResults();
  }

  // Sends the result to the callback and deletes this instance.
  void Finish(FileError error) {
    // Reverse the order here because |result_candidates_| puts the most
    // uninterested candidate at the top.
    std::reverse(results_->begin(), results_->end());

    if (error != FILE_ERROR_OK)
      results_.reset();
    callback_.Run(error, results_.Pass());
    delete this;
  }

  internal::ResourceMetadata* resource_metadata_;
  const std::string query_;
  const int options_;
  const int at_most_num_matches_;
  const SearchMetadataCallback callback_;
  ScopedPriorityQueue<MetadataSearchResult,
                      MetadataSearchResultComparator> result_candidates_;
  scoped_ptr<MetadataSearchResultVector> results_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<SearchMetadataHelper> weak_ptr_factory_;
};

void SearchMetadata(internal::ResourceMetadata* resource_metadata,
                    const std::string& query,
                    int options,
                    int at_most_num_matches,
                    const SearchMetadataCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  // |helper| will delete itself when the search is done.
  SearchMetadataHelper* helper =
      new SearchMetadataHelper(resource_metadata,
                               query,
                               options,
                               at_most_num_matches,
                               callback);
  helper->Start();
}

bool FindAndHighlight(const std::string& text,
                      const std::string& query,
                      std::string* highlighted_text) {
  DCHECK(highlighted_text);
  highlighted_text->clear();

  // For empty query, any filename matches with no highlighted text.
  if (query.empty())
    return true;

  // TODO(satorux): Should support non-ASCII characters.
  std::string lower_text = StringToLowerASCII(text);
  std::string lower_query = StringToLowerASCII(query);

  int num_matches = 0;
  std::string::size_type cursor = 0;

  while (cursor < text.size()) {
    std::string::size_type matched_position =
        lower_text.find(lower_query, cursor);
    if (matched_position == std::string::npos)
      break;
    ++num_matches;

    std::string skipped_piece =
        net::EscapeForHTML(text.substr(cursor, matched_position - cursor));
    std::string matched_piece =
        net::EscapeForHTML(text.substr(matched_position, query.size()));

    highlighted_text->append(skipped_piece);
    highlighted_text->append("<b>" + matched_piece + "</b>");

    cursor = matched_position + query.size();
  }
  if (num_matches == 0)
    return false;

  std::string remaining_piece = text.substr(cursor);
  highlighted_text->append(net::EscapeForHTML(remaining_piece));

  return true;
}

}  // namespace drive
