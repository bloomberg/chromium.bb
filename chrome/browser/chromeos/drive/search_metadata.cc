// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/search_metadata.h"

#include <algorithm>
#include "base/bind.h"
#include "base/string_util.h"
#include "chrome/browser/chromeos/drive/drive_file_system_util.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/escape.h"

using content::BrowserThread;

namespace drive {

namespace {

// Used to sort the search result per the last accessed/modified time. The
// recently accessed/modified files come first.
bool CompareByTimestamp(const MetadataSearchResult& a,
                        const MetadataSearchResult& b) {
  const PlatformFileInfoProto& a_file_info = a.entry_proto.file_info();
  const PlatformFileInfoProto& b_file_info = b.entry_proto.file_info();

  if (a_file_info.last_accessed() != b_file_info.last_accessed())
    return a_file_info.last_accessed() > b_file_info.last_accessed();

  // When the entries have the same last access time (which happens quite often
  // because Drive server doesn't set the field until an entry is viewed via
  // drive.google.com), we use last modified time as the tie breaker.
  return a_file_info.last_modified() > b_file_info.last_modified();
}

// Returns true if |entry| is eligible for the search |options| and should be
// tested for the match with the query.
// If SEARCH_METADATA_EXCLUDE_HOSTED_DOCUMENTS is requested, the hosted
// documents are skipped. If SEARCH_METADATA_EXCLUDE_DIRECTORIES is requested,
// the directories are skipped. If SEARCH_METADATA_SHARED_WITH_ME is requested,
// only the entries with shared-with-me label will be tested.
bool IsEligibleEntry(const DriveEntryProto& entry, int options) {
  if ((options & SEARCH_METADATA_EXCLUDE_HOSTED_DOCUMENTS) &&
      entry.file_specific_info().is_hosted_document())
    return false;

  if ((options & SEARCH_METADATA_EXCLUDE_DIRECTORIES) &&
      entry.file_info().is_directory())
    return false;

  if (options & SEARCH_METADATA_SHARED_WITH_ME)
    return entry.shared_with_me();

  return true;
}

}  // namespace

// Helper class for searching the local resource metadata.
class SearchMetadataHelper {
 public:
  SearchMetadataHelper(DriveResourceMetadata* resource_metadata,
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
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_ptr_factory_(this)) {
  }

  // Starts searching the local resource metadata by reading the root
  // directory.
  void Start() {
    resource_metadata_->IterateEntries(
        base::Bind(&SearchMetadataHelper::MaybeAddEntryToResult,
                   base::Unretained(this)),
        base::Bind(&SearchMetadataHelper::SortResult,
                   weak_ptr_factory_.GetWeakPtr()));
  }


 private:
  // Adds entry to the result when appropriate.
  void MaybeAddEntryToResult(const DriveEntryProto& entry) {
    // Add it to the search result if the entry is eligible for the given
    // |options| and matches the query. The base name of the entry must
    // contains |query| to match the query.
    std::string highlighted;
    if (IsEligibleEntry(entry, options_) &&
        FindAndHighlight(entry.base_name(), query_, &highlighted)) {
      results_->push_back(
          MetadataSearchResult(base::FilePath(), entry, highlighted));
    }
  }

  // Sorts the result by time stamps.
  void SortResult() {
    std::sort(results_->begin(), results_->end(), &CompareByTimestamp);
    if (results_->size() > static_cast<size_t>(at_most_num_matches_)) {
      // Don't use resize() as it requires a default constructor.
      results_->erase(results_->begin() + at_most_num_matches_,
                      results_->end());
    }

    FillFilePaths(0);
  }

  // Fills file paths of results.
  void FillFilePaths(size_t index) {
    if (index == results_->size()) {
      Finish(DRIVE_FILE_OK);
      return;
    }
    resource_metadata_->GetEntryInfoByResourceId(
        results_->at(index).entry_proto.resource_id(),
        base::Bind(&SearchMetadataHelper::ContinueFillFilePaths,
                   weak_ptr_factory_.GetWeakPtr(),
                   index));
  }

  // Implements FillFilePaths().
  void ContinueFillFilePaths(size_t index,
                             DriveFileError error,
                             const base::FilePath& path,
                             scoped_ptr<DriveEntryProto> unused_entry) {
    if (error != DRIVE_FILE_OK) {
      Finish(error);
      return;
    }
    MetadataSearchResult* result = &(*results_)[index];
    result->path = path;
    FillFilePaths(index + 1);
  }

  // Sends the result to the callback and deletes this instance.
  void Finish(DriveFileError error) {
    if (error != DRIVE_FILE_OK)
      results_.reset();
    callback_.Run(error, results_.Pass());
    delete this;
  }

  DriveResourceMetadata* resource_metadata_;
  const std::string query_;
  const int options_;
  const int at_most_num_matches_;
  const SearchMetadataCallback callback_;
  scoped_ptr<MetadataSearchResultVector> results_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<SearchMetadataHelper> weak_ptr_factory_;
};

void SearchMetadata(DriveResourceMetadata* resource_metadata,
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
