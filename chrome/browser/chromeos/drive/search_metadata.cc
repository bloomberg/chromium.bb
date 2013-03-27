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

}  // namespace

// Helper class for searching the local resource metadata.
class SearchMetadataHelper {
 public:
  SearchMetadataHelper(DriveResourceMetadata* resource_metadata,
                       const std::string& query,
                       int at_most_num_matches,
                       SearchMetadataTarget target,
                       const SearchMetadataCallback& callback)
    : resource_metadata_(resource_metadata),
      query_(query),
      at_most_num_matches_(at_most_num_matches),
      target_(target),
      callback_(callback),
      results_(new MetadataSearchResultVector),
      num_pending_reads_(0),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_ptr_factory_(this)) {
  }

  // Starts searching the local resource metadata by reading the root
  // directory.
  void Start() {
    ++num_pending_reads_;
    resource_metadata_->ReadDirectoryByPath(
        util::GetDriveMyDriveRootPath(),
        base::Bind(&SearchMetadataHelper::DidReadDirectoryByPath,
                   weak_ptr_factory_.GetWeakPtr(),
                   util::GetDriveMyDriveRootPath()));
  }


 private:
  // Called when a directory is read. Continues searching the local resource
  // metadata by recursively reading sub directories.
  void DidReadDirectoryByPath(const base::FilePath& parent_path,
                              DriveFileError error,
                              scoped_ptr<DriveEntryProtoVector> entries) {
    if (error != DRIVE_FILE_OK) {
      callback_.Run(error, scoped_ptr<MetadataSearchResultVector>());
      // There could be some in-flight ReadDirectoryByPath() requests, but
      // deleting |this| is safe thanks to the weak pointer.
      delete this;
      return;
    }
    DCHECK(entries);

    --num_pending_reads_;
    for (size_t i = 0; i < entries->size(); ++i) {
      const DriveEntryProto& entry = entries->at(i);
      const base::FilePath current_path = parent_path.Append(
          base::FilePath::FromUTF8Unsafe(entry.base_name()));
      // Skip the hosted document if "hide hosted documents" setting is
      // enabled.
      if (target_ == SEARCH_METADATA_EXCLUDE_HOSTED_DOCUMENTS &&
          entry.file_specific_info().is_hosted_document())
        continue;

      // Add it to the search result if the base name of the file contains
      // the query.
      std::string highlighted;
      if (FindAndHighlight(entry.base_name(), query_, &highlighted)) {
        results_->push_back(
            MetadataSearchResult(current_path, entry, highlighted));
      }

      // Recursively reading the sub directory.
      if (entry.file_info().is_directory()) {
        ++num_pending_reads_;
        resource_metadata_->ReadDirectoryByPath(
            current_path,
            base::Bind(&SearchMetadataHelper::DidReadDirectoryByPath,
                       weak_ptr_factory_.GetWeakPtr(),
                       current_path));
      }
    }

    if (num_pending_reads_ == 0) {
      // Search is complete. Send the result to the callback.
      std::sort(results_->begin(), results_->end(), &CompareByTimestamp);
      if (results_->size() > static_cast<size_t>(at_most_num_matches_)) {
        // Don't use resize() as it requires a default constructor.
        results_->erase(results_->begin() + at_most_num_matches_,
                        results_->end());
      }
      callback_.Run(DRIVE_FILE_OK, results_.Pass());
      delete this;
    }
  }

  DriveResourceMetadata* resource_metadata_;
  const std::string query_;
  const int at_most_num_matches_;
  const SearchMetadataTarget target_;
  const SearchMetadataCallback callback_;
  scoped_ptr<MetadataSearchResultVector> results_;
  int num_pending_reads_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<SearchMetadataHelper> weak_ptr_factory_;
};

void SearchMetadata(DriveResourceMetadata* resource_metadata,
                    const std::string& query,
                    int at_most_num_matches,
                    SearchMetadataTarget target,
                    const SearchMetadataCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  // |helper| will delete itself when the search is done.
  SearchMetadataHelper* helper =
      new SearchMetadataHelper(resource_metadata,
                               query,
                               at_most_num_matches,
                               target,
                               callback);
  helper->Start();
}

bool FindAndHighlight(const std::string& text,
                      const std::string& query,
                      std::string* highlighted_text) {
  DCHECK(highlighted_text);
  highlighted_text->clear();

  if (query.empty())
    return false;

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
