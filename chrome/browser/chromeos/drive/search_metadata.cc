// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/search_metadata.h"

#include <algorithm>
#include <queue>

#include "base/bind.h"
#include "base/i18n/string_search.h"
#include "base/metrics/histogram.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/chromeos/drive/file_system_util.h"
#include "chrome/browser/drive/drive_api_util.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/escape.h"

using content::BrowserThread;

namespace drive {
namespace internal {

namespace {

struct ResultCandidate {
  ResultCandidate(const std::string& local_id,
                  const ResourceEntry& entry,
                  const std::string& highlighted_base_name)
      : local_id(local_id),
        entry(entry),
        highlighted_base_name(highlighted_base_name) {
  }

  std::string local_id;
  ResourceEntry entry;
  std::string highlighted_base_name;
};

// Used to sort the result candidates per the last accessed/modified time. The
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

struct ResultCandidateComparator {
  bool operator()(const ResultCandidate* a, const ResultCandidate* b) const {
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
    // Keep top alive for the pop() call so that debug checks can access
    // underlying data (e.g. validating heap property of the priority queue
    // will call the comparator).
    T* saved_top = queue_.top();
    queue_.pop();
    delete saved_top;
  }

 private:
  std::priority_queue<T*, std::vector<T*>, Compare> queue_;

  DISALLOW_COPY_AND_ASSIGN(ScopedPriorityQueue);
};

// Classifies the given entry as hidden if it's not under specific directories.
class HiddenEntryClassifier {
 public:
  HiddenEntryClassifier(ResourceMetadata* metadata,
                        const std::string& mydrive_local_id)
      : metadata_(metadata) {
    // Only things under My Drive and drive/other are not hidden.
    is_hiding_child_[mydrive_local_id] = false;
    is_hiding_child_[util::kDriveOtherDirLocalId] = false;

    // Everything else is hidden, including the directories mentioned above
    // themselves.
    is_hiding_child_[""] = true;
  }

  // |result| is set to true if |entry| is hidden.
  FileError IsHidden(const ResourceEntry& entry, bool* result) {
    // Look up for parents recursively.
    std::vector<std::string> undetermined_ids;
    undetermined_ids.push_back(entry.parent_local_id());

    std::map<std::string, bool>::iterator it =
        is_hiding_child_.find(undetermined_ids.back());
    for (; it == is_hiding_child_.end();
         it = is_hiding_child_.find(undetermined_ids.back())) {
      ResourceEntry parent;
      FileError error =
          metadata_->GetResourceEntryById(undetermined_ids.back(), &parent);
      if (error != FILE_ERROR_OK)
        return error;
      undetermined_ids.push_back(parent.parent_local_id());
    }

    // Cache the result.
    undetermined_ids.pop_back();  // The last one is already in the map.
    for (size_t i = 0; i < undetermined_ids.size(); ++i)
      is_hiding_child_[undetermined_ids[i]] = it->second;

    *result = it->second;
    return FILE_ERROR_OK;
  }

 private:
  ResourceMetadata* metadata_;

  // local ID to is_hidden map.
  std::map<std::string, bool> is_hiding_child_;
};

// Returns true if |entry| is eligible for the search |options| and should be
// tested for the match with the query.  If
// SEARCH_METADATA_EXCLUDE_HOSTED_DOCUMENTS is requested, the hosted documents
// are skipped. If SEARCH_METADATA_EXCLUDE_DIRECTORIES is requested, the
// directories are skipped. If SEARCH_METADATA_SHARED_WITH_ME is requested, only
// the entries with shared-with-me label will be tested. If
// SEARCH_METADATA_OFFLINE is requested, only hosted documents and cached files
// match with the query. This option can not be used with other options.
bool IsEligibleEntry(const ResourceEntry& entry, int options) {
  if ((options & SEARCH_METADATA_EXCLUDE_HOSTED_DOCUMENTS) &&
      entry.file_specific_info().is_hosted_document())
    return false;

  if ((options & SEARCH_METADATA_EXCLUDE_DIRECTORIES) &&
      entry.file_info().is_directory())
    return false;

  if (options & SEARCH_METADATA_SHARED_WITH_ME)
    return entry.shared_with_me();

  if (options & SEARCH_METADATA_OFFLINE) {
    if (entry.file_specific_info().is_hosted_document()) {
      // Not all hosted documents are cached by Drive offline app.
      // http://support.google.com/drive/bin/answer.py?hl=en&answer=1628467
      std::string mime_type = drive::util::GetHostedDocumentMimeType(
          entry.file_specific_info().document_extension());
      return mime_type == drive::util::kGoogleDocumentMimeType ||
             mime_type == drive::util::kGoogleSpreadsheetMimeType ||
             mime_type == drive::util::kGooglePresentationMimeType ||
             mime_type == drive::util::kGoogleDrawingMimeType;
    } else {
      return entry.file_specific_info().cache_state().is_present();
    }
  }

  return true;
}

// Used to implement SearchMetadata.
// Adds entry to the result when appropriate.
// In particular, if |query| is non-null, only adds files with the name matching
// the query.
FileError MaybeAddEntryToResult(
    ResourceMetadata* resource_metadata,
    ResourceMetadata::Iterator* it,
    base::i18n::FixedPatternStringSearchIgnoringCaseAndAccents* query,
    int options,
    size_t at_most_num_matches,
    HiddenEntryClassifier* hidden_entry_classifier,
    ScopedPriorityQueue<ResultCandidate,
                        ResultCandidateComparator>* result_candidates) {
  DCHECK_GE(at_most_num_matches, result_candidates->size());

  const ResourceEntry& entry = it->GetValue();

  // If the candidate set is already full, and this |entry| is old, do nothing.
  // We perform this check first in order to avoid the costly find-and-highlight
  // or FilePath lookup as much as possible.
  if (result_candidates->size() == at_most_num_matches &&
      !CompareByTimestamp(entry, result_candidates->top()->entry))
    return FILE_ERROR_OK;

  // Add |entry| to the result if the entry is eligible for the given
  // |options| and matches the query. The base name of the entry must
  // contain |query| to match the query.
  std::string highlighted;
  if (!IsEligibleEntry(entry, options) ||
      (query && !FindAndHighlight(entry.base_name(), query, &highlighted)))
    return FILE_ERROR_OK;

  // Hidden entry should not be returned.
  bool hidden = false;
  FileError error = hidden_entry_classifier->IsHidden(entry, &hidden);
  if (error != FILE_ERROR_OK || hidden)
    return error;

  // Make space for |entry| when appropriate.
  if (result_candidates->size() == at_most_num_matches)
    result_candidates->pop();
  result_candidates->push(new ResultCandidate(it->GetID(), entry, highlighted));
  return FILE_ERROR_OK;
}

// Implements SearchMetadata().
FileError SearchMetadataOnBlockingPool(ResourceMetadata* resource_metadata,
                                       const std::string& query_text,
                                       int options,
                                       int at_most_num_matches,
                                       MetadataSearchResultVector* results) {
  ScopedPriorityQueue<ResultCandidate,
                      ResultCandidateComparator> result_candidates;

  // Prepare data structure for searching.
  base::i18n::FixedPatternStringSearchIgnoringCaseAndAccents query(
      base::UTF8ToUTF16(query_text));

  // Prepare an object to filter out hidden entries.
  ResourceEntry mydrive;
  FileError error = resource_metadata->GetResourceEntryByPath(
      util::GetDriveMyDriveRootPath(), &mydrive);
  if (error != FILE_ERROR_OK)
    return error;
  HiddenEntryClassifier hidden_entry_classifier(resource_metadata,
                                                mydrive.local_id());

  // Iterate over entries.
  scoped_ptr<ResourceMetadata::Iterator> it = resource_metadata->GetIterator();
  for (; !it->IsAtEnd(); it->Advance()) {
    FileError error = MaybeAddEntryToResult(resource_metadata, it.get(),
                                            query_text.empty() ? NULL : &query,
                                            options,
                                            at_most_num_matches,
                                            &hidden_entry_classifier,
                                            &result_candidates);
    if (error != FILE_ERROR_OK)
      return error;
  }

  // Prepare the result.
  for (; !result_candidates.empty(); result_candidates.pop()) {
    const ResultCandidate& candidate = *result_candidates.top();
    // The path field of entries in result_candidates are empty at this point,
    // because we don't want to run the expensive metadata DB look up except for
    // the final results. Hence, here we fill the part.
    base::FilePath path;
    error = resource_metadata->GetFilePath(candidate.local_id, &path);
    if (error != FILE_ERROR_OK)
      return error;
    bool is_directory = candidate.entry.file_info().is_directory();
    results->push_back(MetadataSearchResult(
        path, is_directory, candidate.highlighted_base_name));
  }

  // Reverse the order here because |result_candidates| puts the most
  // uninteresting candidate at the top.
  std::reverse(results->begin(), results->end());

  return FILE_ERROR_OK;
}

// Runs the SearchMetadataCallback and updates the histogram.
void RunSearchMetadataCallback(const SearchMetadataCallback& callback,
                               const base::TimeTicks& start_time,
                               scoped_ptr<MetadataSearchResultVector> results,
                               FileError error) {
  if (error != FILE_ERROR_OK)
    results.reset();
  callback.Run(error, results.Pass());

  UMA_HISTOGRAM_TIMES("Drive.SearchMetadataTime",
                      base::TimeTicks::Now() - start_time);
}

}  // namespace

void SearchMetadata(
    scoped_refptr<base::SequencedTaskRunner> blocking_task_runner,
    ResourceMetadata* resource_metadata,
    const std::string& query,
    int options,
    int at_most_num_matches,
    const SearchMetadataCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK_LE(0, at_most_num_matches);
  DCHECK(!callback.is_null());

  const base::TimeTicks start_time = base::TimeTicks::Now();

  scoped_ptr<MetadataSearchResultVector> results(
      new MetadataSearchResultVector);
  MetadataSearchResultVector* results_ptr = results.get();
  base::PostTaskAndReplyWithResult(blocking_task_runner.get(),
                                   FROM_HERE,
                                   base::Bind(&SearchMetadataOnBlockingPool,
                                              resource_metadata,
                                              query,
                                              options,
                                              at_most_num_matches,
                                              results_ptr),
                                   base::Bind(&RunSearchMetadataCallback,
                                              callback,
                                              start_time,
                                              base::Passed(&results)));
}

bool FindAndHighlight(
    const std::string& text,
    base::i18n::FixedPatternStringSearchIgnoringCaseAndAccents* query,
    std::string* highlighted_text) {
  DCHECK(query);
  DCHECK(highlighted_text);
  highlighted_text->clear();

  base::string16 text16 = base::UTF8ToUTF16(text);
  size_t match_start = 0;
  size_t match_length = 0;
  if (!query->Search(text16, &match_start, &match_length))
    return false;

  base::string16 pre = text16.substr(0, match_start);
  base::string16 match = text16.substr(match_start, match_length);
  base::string16 post = text16.substr(match_start + match_length);
  highlighted_text->append(net::EscapeForHTML(base::UTF16ToUTF8(pre)));
  highlighted_text->append("<b>");
  highlighted_text->append(net::EscapeForHTML(base::UTF16ToUTF8(match)));
  highlighted_text->append("</b>");
  highlighted_text->append(net::EscapeForHTML(base::UTF16ToUTF8(post)));
  return true;
}

}  // namespace internal
}  // namespace drive
