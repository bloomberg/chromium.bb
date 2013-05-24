// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/file_system/search_operation.h"

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
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
// returns the result. Refreshed entries will be stored into |result|.
FileError RefreshEntriesOnBlockingPool(
    internal::ResourceMetadata* resource_metadata,
    scoped_ptr<google_apis::ResourceList> resource_list,
    std::vector<SearchResultInfo>* result) {
  DCHECK(resource_metadata);
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
      // The result is absent in local resource metadata. There are two cases:
      //
      // 1) Resource metadata is not up-to-date, and the entry has recently
      //    been added to the drive. This is not a fatal error, so just skip to
      //    add the result. We should soon receive XMPP update notification
      //    and refresh both the metadata and search result UI in Files.app.
      //
      // 2) Resource metadata is not fully loaded.
      // TODO(kinaba) crbug.com/181075:
      //    In this case, we are doing "fast fetch" fetching directory lists on
      //    the fly to quickly deliver results to the user. However, we have no
      //    such equivalent for Search.
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
    internal::ResourceMetadata* metadata)
    : blocking_task_runner_(blocking_task_runner),
      scheduler_(scheduler),
      metadata_(metadata),
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
    callback.Run(error, GURL(), scoped_ptr<std::vector<SearchResultInfo> >());
    return;
  }

  DCHECK(resource_list);

  GURL next_feed;
  resource_list->GetNextFeedURL(&next_feed);

  // The search results will be returned using virtual directory.
  // The directory is not really part of the file system, so it has no parent or
  // root.
  scoped_ptr<std::vector<SearchResultInfo> > result(
      new std::vector<SearchResultInfo>);
  if (resource_list->entries().empty()) {
    // Short cut. If the resource entry is empty, we don't need to refresh
    // the resource metadata.
    callback.Run(FILE_ERROR_OK, next_feed, result.Pass());
    return;
  }

  std::vector<SearchResultInfo>* result_ptr = result.get();
  base::PostTaskAndReplyWithResult(
      blocking_task_runner_,
      FROM_HERE,
      base::Bind(&RefreshEntriesOnBlockingPool,
                 metadata_,
                 base::Passed(&resource_list),
                 result_ptr),
      base::Bind(&SearchOperation::SearchAfterRefreshEntry,
                 weak_ptr_factory_.GetWeakPtr(),
                 callback,
                 next_feed,
                 base::Passed(&result)));
}

void SearchOperation::SearchAfterRefreshEntry(
    const SearchOperationCallback& callback,
    const GURL& next_feed,
    scoped_ptr<std::vector<SearchResultInfo> > result,
    FileError error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());
  DCHECK(result);

  if (error != FILE_ERROR_OK) {
    callback.Run(error, GURL(), scoped_ptr<std::vector<SearchResultInfo> >());
    return;
  }

  callback.Run(error, next_feed, result.Pass());
}

}  // namespace file_system
}  // namespace drive
