// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/file_system/search_operation.h"

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/sequenced_task_runner.h"
#include "base/sequenced_task_runner.h"
#include "base/task_runner_util.h"
#include "chrome/browser/chromeos/drive/file_system_util.h"
#include "chrome/browser/chromeos/drive/job_scheduler.h"
#include "chrome/browser/chromeos/drive/resource_entry_conversion.h"
#include "chrome/browser/chromeos/drive/resource_metadata.h"
#include "chrome/browser/google_apis/gdata_wapi_parser.h"
#include "content/public/browser/browser_thread.h"
#include "googleurl/src/gurl.h"

using content::BrowserThread;

namespace drive {
namespace file_system {
namespace {

// Refreshes entries of |resource_metadata| based on |resource_list|, and
// returns the result. |is_update_needed| will be set to true, if an
// inconsitency is found between |resource_list| and |resource_metadata|.
// Refreshed entries will be stored into |result|.
FileError RefreshEntriesOnBlockingPool(
    internal::ResourceMetadata* resource_metadata,
    scoped_ptr<google_apis::ResourceList> resource_list,
    bool* is_update_needed,
    std::vector<SearchResultInfo>* result) {
  DCHECK(resource_metadata);
  DCHECK(is_update_needed);
  DCHECK(result);

  const ScopedVector<google_apis::ResourceEntry>& entries =
      resource_list->entries();
  result->reserve(entries.size());
  for (size_t i = 0; i < entries.size(); ++i) {
    ResourceEntry entry = ConvertToResourceEntry(*entries[i]);
    base::FilePath drive_file_path;
    FileError error = resource_metadata->RefreshEntry(
        entry, &drive_file_path, &entry);
    if (error == FILE_ERROR_OK) {
      result->push_back(SearchResultInfo(drive_file_path, entry));
    } else if (error == FILE_ERROR_NOT_FOUND) {
      // If a result is not present in our local resource metadata,
      // it is necessary to update the resource metadata.
      // This may happen if the entry has recently been added to the drive (and
      // we haven't received the delta update feed yet).
      // This is not a fatal error, so skip to add the result, but notify
      // the caller that the update is needed.
      *is_update_needed = true;
    } else {
      // Otherwise, it is a fatal error. Give up to return the search result.
      return error;
    }
  }

  return FILE_ERROR_OK;
}

}  // namespace

SearchOperation::SearchOperation(
    base::SequencedTaskRunner* blocking_task_runner,
    JobScheduler* scheduler,
    internal::ResourceMetadata* resource_metadata)
    : blocking_task_runner_(blocking_task_runner),
      scheduler_(scheduler),
      resource_metadata_(resource_metadata),
      weak_ptr_factory_(this) {
}

SearchOperation::~SearchOperation() {
}

void SearchOperation::Search(const std::string& search_query,
                             const GURL& next_feed,
                             const SearchOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  if (next_feed.is_empty()) {
    // This is first request for the |search_query|.
    scheduler_->Search(
        search_query,
        base::Bind(&SearchOperation::SearchAfterGetResourceList,
                   weak_ptr_factory_.GetWeakPtr(), callback));
  } else {
    // There is the remaining result so fetch it.
    scheduler_->ContinueGetResourceList(
        next_feed,
        base::Bind(&SearchOperation::SearchAfterGetResourceList,
                   weak_ptr_factory_.GetWeakPtr(), callback));
  }
}

void SearchOperation::SearchAfterGetResourceList(
    const SearchOperationCallback& callback,
    google_apis::GDataErrorCode gdata_error,
    scoped_ptr<google_apis::ResourceList> resource_list) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  FileError error = util::GDataToFileError(gdata_error);
  if (error != FILE_ERROR_OK) {
    callback.Run(
        error, false, GURL(), scoped_ptr<std::vector<SearchResultInfo> >());
    return;
  }

  DCHECK(resource_list);

  GURL next_feed;
  resource_list->GetNextFeedURL(&next_feed);

  LOG(ERROR) << "Search Result: " << resource_list->entries().size();

  // The search results will be returned using virtual directory.
  // The directory is not really part of the file system, so it has no parent or
  // root.
  scoped_ptr<std::vector<SearchResultInfo> > result(
      new std::vector<SearchResultInfo>);
  if (resource_list->entries().empty()) {
    // Short cut. If the resource entry is empty, we don't need to refresh
    // the resource metadata.
    callback.Run(FILE_ERROR_OK, false, next_feed, result.Pass());
    return;
  }

  bool* is_update_needed = new bool(false);
  std::vector<SearchResultInfo>* result_ptr = result.get();
  base::PostTaskAndReplyWithResult(
      blocking_task_runner_,
      FROM_HERE,
      base::Bind(&RefreshEntriesOnBlockingPool,
                 resource_metadata_,
                 base::Passed(&resource_list),
                 is_update_needed,
                 result_ptr),
      base::Bind(&SearchOperation::SearchAfterRefreshEntry,
                 weak_ptr_factory_.GetWeakPtr(),
                 callback,
                 next_feed,
                 base::Owned(is_update_needed),
                 base::Passed(&result)));
}

void SearchOperation::SearchAfterRefreshEntry(
    const SearchOperationCallback& callback,
    const GURL& next_feed,
    bool* is_update_needed,
    scoped_ptr<std::vector<SearchResultInfo> > result,
    FileError error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());
  DCHECK(is_update_needed);
  DCHECK(result);

  if (error != FILE_ERROR_OK) {
    callback.Run(
        error, false, GURL(), scoped_ptr<std::vector<SearchResultInfo> >());
    return;
  }

  LOG(ERROR) << "Search Result2: " << result->size();
  LOG(ERROR) << "Is update needed: " << (*is_update_needed ? "true" : "false");

  callback.Run(error, *is_update_needed, next_feed, result.Pass());
}

}  // namespace file_system
}  // namespace drive
