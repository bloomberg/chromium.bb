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
#include "url/gurl.h"

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
    ResourceEntry entry;
    if (!ConvertToResourceEntry(*entries[i], &entry))
      continue;  // Skip non-file entries.

    const std::string id = entry.resource_id();
    FileError error = resource_metadata->RefreshEntry(id, entry);
    if (error == FILE_ERROR_NOT_FOUND) {
      // The result is absent in local resource metadata. This can happen if
      // the metadata is not synced to the latest server state yet. In that
      // case, we temporarily add the file to the special "drive/other"
      // directory in order to assign a path, which is needed to access the
      // file through FileSystem API.
      //
      // It will be moved to the right place when the metadata gets synced
      // in normal loading process in ChangeListProcessor.
      entry.set_parent_local_id(util::kDriveOtherDirSpecialResourceId);
      error = resource_metadata->AddEntry(entry);

      // FILE_ERROR_EXISTS may happen if we have already added the entry to
      // "drive/other" once before. That's not an error.
      if (error == FILE_ERROR_EXISTS)
        error = FILE_ERROR_OK;
    }
    if (error == FILE_ERROR_OK)
      error = resource_metadata->GetResourceEntryById(id, &entry);
    if (error != FILE_ERROR_OK)
      return error;
    result->push_back(SearchResultInfo(resource_metadata->GetFilePath(id),
                                       entry));
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
                             const GURL& next_url,
                             const SearchCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  if (next_url.is_empty()) {
    // This is first request for the |search_query|.
    scheduler_->Search(
        search_query,
        base::Bind(&SearchOperation::SearchAfterGetResourceList,
                   weak_ptr_factory_.GetWeakPtr(), callback));
  } else {
    // There is the remaining result so fetch it.
    scheduler_->ContinueGetResourceList(
        next_url,
        base::Bind(&SearchOperation::SearchAfterGetResourceList,
                   weak_ptr_factory_.GetWeakPtr(), callback));
  }
}

void SearchOperation::SearchAfterGetResourceList(
    const SearchCallback& callback,
    google_apis::GDataErrorCode gdata_error,
    scoped_ptr<google_apis::ResourceList> resource_list) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  FileError error = GDataToFileError(gdata_error);
  if (error != FILE_ERROR_OK) {
    callback.Run(error, GURL(), scoped_ptr<std::vector<SearchResultInfo> >());
    return;
  }

  DCHECK(resource_list);

  GURL next_url;
  resource_list->GetNextFeedURL(&next_url);

  // The search results will be returned using virtual directory.
  // The directory is not really part of the file system, so it has no parent or
  // root.
  scoped_ptr<std::vector<SearchResultInfo> > result(
      new std::vector<SearchResultInfo>);
  if (resource_list->entries().empty()) {
    // Short cut. If the resource entry is empty, we don't need to refresh
    // the resource metadata.
    callback.Run(FILE_ERROR_OK, next_url, result.Pass());
    return;
  }

  std::vector<SearchResultInfo>* result_ptr = result.get();
  base::PostTaskAndReplyWithResult(
      blocking_task_runner_.get(),
      FROM_HERE,
      base::Bind(&RefreshEntriesOnBlockingPool,
                 metadata_,
                 base::Passed(&resource_list),
                 result_ptr),
      base::Bind(&SearchOperation::SearchAfterRefreshEntry,
                 weak_ptr_factory_.GetWeakPtr(),
                 callback,
                 next_url,
                 base::Passed(&result)));
}

void SearchOperation::SearchAfterRefreshEntry(
    const SearchCallback& callback,
    const GURL& next_url,
    scoped_ptr<std::vector<SearchResultInfo> > result,
    FileError error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());
  DCHECK(result);

  if (error != FILE_ERROR_OK) {
    callback.Run(error, GURL(), scoped_ptr<std::vector<SearchResultInfo> >());
    return;
  }

  callback.Run(error, next_url, result.Pass());
}

}  // namespace file_system
}  // namespace drive
