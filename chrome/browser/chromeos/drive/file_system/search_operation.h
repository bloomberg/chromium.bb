// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DRIVE_FILE_SYSTEM_SEARCH_OPERATION_H_
#define CHROME_BROWSER_CHROMEOS_DRIVE_FILE_SYSTEM_SEARCH_OPERATION_H_

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/drive/file_errors.h"
#include "chrome/browser/chromeos/drive/file_system_interface.h"
#include "google_apis/drive/gdata_errorcode.h"

namespace base {
class SequencedTaskRunner;
}  // namespace base

namespace google_apis {
class FileList;
}  // namespace google_apis

namespace drive {

class JobScheduler;

namespace internal {
class LoaderController;
class ResourceMetadata;
}  // namespace internal

namespace file_system {

// This class encapsulates the drive Search function. It is responsible for
// sending the request to the drive API.
class SearchOperation {
 public:
  SearchOperation(base::SequencedTaskRunner* blocking_task_runner,
                  JobScheduler* scheduler,
                  internal::ResourceMetadata* metadata,
                  internal::LoaderController* loader_controller);
  ~SearchOperation();

  // Performs server side content search operation for |search_query|.  If
  // |next_link| is set, this is the search result url that will be fetched.
  // Upon completion, |callback| will be called with the result.  This is
  // implementation of FileSystemInterface::Search() method.
  //
  // |callback| must not be null.
  void Search(const std::string& search_query,
              const GURL& next_link,
              const SearchCallback& callback);

 private:
  // Part of Search(), called after the FileList is fetched from the server.
  void SearchAfterGetFileList(
      const SearchCallback& callback,
      google_apis::GDataErrorCode gdata_error,
      scoped_ptr<google_apis::FileList> file_list);

  // Part of Search(), called after |result| is filled on the blocking pool.
  void SearchAfterResolveSearchResult(
      const SearchCallback& callback,
      const GURL& next_link,
      scoped_ptr<std::vector<SearchResultInfo> > result,
      FileError error);

  scoped_refptr<base::SequencedTaskRunner> blocking_task_runner_;
  JobScheduler* scheduler_;
  internal::ResourceMetadata* metadata_;
  internal::LoaderController* loader_controller_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate the weak pointers before any other members are destroyed.
  base::WeakPtrFactory<SearchOperation> weak_ptr_factory_;
  DISALLOW_COPY_AND_ASSIGN(SearchOperation);
};

}  // namespace file_system
}  // namespace drive

#endif  // CHROME_BROWSER_CHROMEOS_DRIVE_FILE_SYSTEM_SEARCH_OPERATION_H_
