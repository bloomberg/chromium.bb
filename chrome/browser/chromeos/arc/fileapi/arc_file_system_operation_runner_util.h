// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ARC_FILEAPI_ARC_FILE_SYSTEM_OPERATION_RUNNER_UTIL_H_
#define CHROME_BROWSER_CHROMEOS_ARC_FILEAPI_ARC_FILE_SYSTEM_OPERATION_RUNNER_UTIL_H_

#include <string>

#include "base/memory/ref_counted.h"
#include "chrome/browser/chromeos/arc/fileapi/arc_file_system_operation_runner.h"

class GURL;

namespace arc {

namespace file_system_operation_runner_util {

using GetFileSizeCallback = ArcFileSystemOperationRunner::GetFileSizeCallback;
using OpenFileToReadCallback =
    ArcFileSystemOperationRunner::OpenFileToReadCallback;
using GetDocumentCallback = ArcFileSystemOperationRunner::GetDocumentCallback;
using GetChildDocumentsCallback =
    ArcFileSystemOperationRunner::GetChildDocumentsCallback;
using AddWatcherCallback = ArcFileSystemOperationRunner::AddWatcherCallback;
using RemoveWatcherCallback =
    ArcFileSystemOperationRunner::RemoveWatcherCallback;
using WatcherCallback = ArcFileSystemOperationRunner::WatcherCallback;

// Wraps an ArcFileSystemOperationRunner::Observer so it is called on the IO
// thread.
class ObserverIOThreadWrapper
    : public base::RefCountedThreadSafe<ObserverIOThreadWrapper>,
      public ArcFileSystemOperationRunner::Observer {
 public:
  explicit ObserverIOThreadWrapper(
      ArcFileSystemOperationRunner::Observer* underlying_observer);

  // Disables the observer. After calling this function, the underlying observer
  // will never be called.
  // This function should be called on the IO thread.
  void Disable();

  // ArcFileSystemOperationRunner::Observer overrides:
  void OnWatchersCleared() override;

 private:
  friend class base::RefCountedThreadSafe<ObserverIOThreadWrapper>;

  ~ObserverIOThreadWrapper() override;

  void OnWatchersClearedOnIOThread();

  ArcFileSystemOperationRunner::Observer* const underlying_observer_;

  // If set to true, |observer_| will not be called. This variable should be
  // accessed only on the IO thread.
  bool disabled_ = false;

  DISALLOW_COPY_AND_ASSIGN(ObserverIOThreadWrapper);
};

// Utility functions to post a task to run ArcFileSystemOperationRunner methods.
// These functions must be called on the IO thread. Callbacks and observers will
// be called on the IO thread.
void GetFileSizeOnIOThread(const GURL& url,
                           const GetFileSizeCallback& callback);
void OpenFileToReadOnIOThread(const GURL& url,
                              const OpenFileToReadCallback& callback);
void GetDocumentOnIOThread(const std::string& authority,
                           const std::string& document_id,
                           const GetDocumentCallback& callback);
void GetChildDocumentsOnIOThread(const std::string& authority,
                                 const std::string& parent_document_id,
                                 const GetChildDocumentsCallback& callback);
void AddWatcherOnIOThread(const std::string& authority,
                          const std::string& document_id,
                          const WatcherCallback& watcher_callback,
                          const AddWatcherCallback& callback);
void RemoveWatcherOnIOThread(int64_t watcher_id,
                             const RemoveWatcherCallback& callback);
void AddObserverOnIOThread(scoped_refptr<ObserverIOThreadWrapper> observer);
void RemoveObserverOnIOThread(scoped_refptr<ObserverIOThreadWrapper> observer);

}  // namespace file_system_operation_runner_util
}  // namespace arc

#endif  // CHROME_BROWSER_CHROMEOS_ARC_FILEAPI_ARC_FILE_SYSTEM_OPERATION_RUNNER_UTIL_H_
