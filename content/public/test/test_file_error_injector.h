// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_TEST_FILE_ERROR_INJECTOR_H_
#define CONTENT_PUBLIC_TEST_TEST_FILE_ERROR_INJECTOR_H_

#include <map>
#include <set>
#include <string>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/ref_counted.h"
#include "content/public/browser/download_interrupt_reasons.h"
#include "url/gurl.h"

namespace content {

class DownloadId;
class DownloadFileWithErrorsFactory;
class DownloadManager;
class DownloadManagerImpl;

// Test helper for injecting errors into download file operations.
// All errors for a download must be injected before it starts.
// This class needs to be |RefCountedThreadSafe| because the implementation
// is referenced by other classes that live past the time when the user is
// nominally done with it.  These classes are internal to content/.
//
// NOTE: No more than one download with the same URL can be in progress at
// the same time.  You can have multiple simultaneous downloads as long as the
// URLs are different, as the URLs are used as keys to get information about
// the download.
//
// Example:
//
// FileErrorInfo a = { url1, ... };
// FileErrorInfo b = { url2, ... };
//
// scoped_refptr<TestFileErrorInjector> injector =
//     TestFileErrorInjector::Create(download_manager);
//
// injector->AddError(a);
// injector->AddError(b);
// injector->InjectErrors();
//
// download_manager->DownloadUrl(url1, ...);
// download_manager->DownloadUrl(url2, ...);
// ... wait for downloads to finish or get an injected error ...
class TestFileErrorInjector
    : public base::RefCountedThreadSafe<TestFileErrorInjector> {
 public:
  enum FileOperationCode {
    FILE_OPERATION_INITIALIZE,
    FILE_OPERATION_WRITE,
    FILE_OPERATION_RENAME_UNIQUIFY,
    FILE_OPERATION_RENAME_ANNOTATE,
  };

  // Structure that encapsulates the information needed to inject a file error.
  struct FileErrorInfo {
    std::string url;  // Full URL of the download.  Identifies the download.
    FileOperationCode code;  // Operation to affect.
    int operation_instance;  // 0-based count of operation calls, for each code.
    DownloadInterruptReason error;  // Error to inject.
  };

  typedef std::map<std::string, FileErrorInfo> ErrorMap;

  // Creates an instance.  May only be called once.
  // Lives until all callbacks (in the implementation) are complete and the
  // creator goes out of scope.
  // TODO(rdsmith): Allow multiple calls for different download managers.
  static scoped_refptr<TestFileErrorInjector> Create(
      DownloadManager* download_manager);

  // Adds an error.
  // Must be called before |InjectErrors()| for a particular download file.
  // It is an error to call |AddError()| more than once for the same file
  // (URL), unless you call |ClearErrors()| in between them.
  bool AddError(const FileErrorInfo& error_info);

  // Clears all errors.
  // Only affects files created after the next call to InjectErrors().
  void ClearErrors();

  // Injects the errors such that new download files will be affected.
  // The download system must already be initialized before calling this.
  // Multiple calls are allowed, but only useful if the errors have changed.
  // Replaces the injected error list.
  bool InjectErrors();

  // Tells how many files are currently open.
  size_t CurrentFileCount() const;

  // Tells how many files have ever been open (since construction or the
  // last call to |ClearFoundFiles()|).
  size_t TotalFileCount() const;

  // Returns whether or not a file matching |url| has been created.
  bool HadFile(const GURL& url) const;

  // Resets the found file list.
  void ClearFoundFiles();

  static std::string DebugString(FileOperationCode code);

 private:
  friend class base::RefCountedThreadSafe<TestFileErrorInjector>;

  typedef std::set<GURL> FileSet;

  explicit TestFileErrorInjector(DownloadManager* download_manager);

  virtual ~TestFileErrorInjector();

  // Callbacks from the download file, to record lifetimes.
  // These may be called on any thread.
  void RecordDownloadFileConstruction(const GURL& url);
  void RecordDownloadFileDestruction(const GURL& url);

  // These run on the UI thread.
  void DownloadFileCreated(GURL url);
  void DestroyingDownloadFile(GURL url);

  // All the data is used on the UI thread.
  // Our injected error list, mapped by URL.  One per file.
  ErrorMap injected_errors_;

  // Keep track of active DownloadFiles.
  FileSet files_;

  // Keep track of found DownloadFiles.
  FileSet found_files_;

  // The factory we created.  May outlive this class.
  DownloadFileWithErrorsFactory* created_factory_;

  // The download manager we set the factory on.
  DownloadManagerImpl* download_manager_;

  DISALLOW_COPY_AND_ASSIGN(TestFileErrorInjector);
};

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_TEST_FILE_ERROR_INJECTOR_H_
