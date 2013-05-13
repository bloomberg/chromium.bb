// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DRIVE_FILE_SYSTEM_SEARCH_OPERATION_H_
#define CHROME_BROWSER_CHROMEOS_DRIVE_FILE_SYSTEM_SEARCH_OPERATION_H_

#include "base/basictypes.h"
#include "base/callback_forward.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/drive/file_errors.h"
#include "chrome/browser/chromeos/drive/file_system/drive_operations.h"
#include "chrome/browser/chromeos/drive/file_system_interface.h"
#include "chrome/browser/google_apis/gdata_errorcode.h"

class GURL;

namespace base {
class SequencedTaskRunner;
}  // namespace base

namespace google_apis {
class ResourceEntry;
}  // namespace google_apis

namespace drive {

class JobScheduler;

namespace internal {
class ResourceMetadata;
}  // namespace internal

namespace file_system {

// This class encapsulates the drive Search function. It is responsible for
// sending the request to the drive API.
class SearchOperation {
 public:
  SearchOperation(base::SequencedTaskRunner* blocking_task_runner_,
                  JobScheduler* job_scheduler,
                  internal::ResourceMetadata* resource_metadata);
  ~SearchOperation();

  // Performs server side content search operation for |search_query|.
  // If |next_feed| is set, this is the feed url that will be fetched.
  // Upon completion, |callback| will be called with the result.
  // This is implementation of FileSystemInterface::Search() method.
  //
  // |callback| must not be null.
  void Search(const std::string& search_query,
              const GURL& next_feed,
              const SearchOperationCallback& callback);

 private:
  // Part of Search(). This is called after the ResourceList is fetched from
  // the server.
  void SearchAfterGetResourceList(
      const SearchOperationCallback& callback,
      google_apis::GDataErrorCode gdata_error,
      scoped_ptr<google_apis::ResourceList> resource_list);

  // Part of Search(). This is called after the RefreshEntryOnBlockingPool
  // is completed.
  void SearchAfterRefreshEntry(
      const SearchOperationCallback& callback,
      const GURL& next_feed,
      bool* is_update_needed,
      scoped_ptr<std::vector<SearchResultInfo> > result,
      FileError error);

  scoped_refptr<base::SequencedTaskRunner> blocking_task_runner_;
  JobScheduler* scheduler_;  // Not owned.
  internal::ResourceMetadata* resource_metadata_;  // Not owned.

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate the weak pointers before any other members are destroyed.
  base::WeakPtrFactory<SearchOperation> weak_ptr_factory_;
  DISALLOW_COPY_AND_ASSIGN(SearchOperation);
};

}  // namespace file_system
}  // namespace drive

#endif  // CHROME_BROWSER_CHROMEOS_DRIVE_FILE_SYSTEM_SEARCH_OPERATION_H_
