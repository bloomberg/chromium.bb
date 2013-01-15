// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/test_file_error_injector.h"

#include <vector>

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "content/browser/download/download_file_factory.h"
#include "content/browser/download/download_file_impl.h"
#include "content/browser/download/download_interrupt_reasons_impl.h"
#include "content/browser/download/download_manager_impl.h"
#include "content/browser/loader/resource_dispatcher_host_impl.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/power_save_blocker.h"
#include "googleurl/src/gurl.h"

namespace content {
class ByteStreamReader;

namespace {

// A class that performs file operations and injects errors.
class DownloadFileWithErrors: public DownloadFileImpl {
 public:
  typedef base::Callback<void(const GURL& url)> ConstructionCallback;
  typedef base::Callback<void(const GURL& url)> DestructionCallback;

  DownloadFileWithErrors(
      scoped_ptr<DownloadSaveInfo> save_info,
      const FilePath& default_download_directory,
      const GURL& url,
      const GURL& referrer_url,
      bool calculate_hash,
      scoped_ptr<ByteStreamReader> stream,
      const net::BoundNetLog& bound_net_log,
      scoped_ptr<PowerSaveBlocker> power_save_blocker,
      base::WeakPtr<DownloadDestinationObserver> observer,
      const TestFileErrorInjector::FileErrorInfo& error_info,
      const ConstructionCallback& ctor_callback,
      const DestructionCallback& dtor_callback);

  ~DownloadFileWithErrors();

  virtual void Initialize(const InitializeCallback& callback) OVERRIDE;

  // DownloadFile interface.
  virtual DownloadInterruptReason AppendDataToFile(
      const char* data, size_t data_len) OVERRIDE;
  virtual void RenameAndUniquify(
      const FilePath& full_path,
      const RenameCompletionCallback& callback) OVERRIDE;
  virtual void RenameAndAnnotate(
      const FilePath& full_path,
      const RenameCompletionCallback& callback) OVERRIDE;

 private:
  // Error generating helper.
  DownloadInterruptReason ShouldReturnError(
      TestFileErrorInjector::FileOperationCode code,
      DownloadInterruptReason original_error);

  // Determine whether to overwrite an operation with the given code
  // with a substitute error; if returns true, |*original_error| is
  // written with the error to use for overwriting.
  // NOTE: This routine changes state; specifically, it increases the
  // operations counts for the specified code.  It should only be called
  // once per operation.
  bool OverwriteError(
    TestFileErrorInjector::FileOperationCode code,
    DownloadInterruptReason* output_error);

  // Source URL for the file being downloaded.
  GURL source_url_;

  // Our injected error.  Only one per file.
  TestFileErrorInjector::FileErrorInfo error_info_;

  // Count per operation.  0-based.
  std::map<TestFileErrorInjector::FileOperationCode, int> operation_counter_;

  // Callback for destruction.
  DestructionCallback destruction_callback_;
};

static void InitializeErrorCallback(
    const DownloadFile::InitializeCallback original_callback,
    DownloadInterruptReason overwrite_error,
    DownloadInterruptReason original_error) {
  original_callback.Run(overwrite_error);
}

static void RenameErrorCallback(
    const DownloadFile::RenameCompletionCallback original_callback,
    DownloadInterruptReason overwrite_error,
    DownloadInterruptReason original_error,
    const FilePath& path_result) {
  original_callback.Run(
      overwrite_error,
      overwrite_error == DOWNLOAD_INTERRUPT_REASON_NONE ?
      path_result : FilePath());
}

DownloadFileWithErrors::DownloadFileWithErrors(
    scoped_ptr<DownloadSaveInfo> save_info,
    const FilePath& default_download_directory,
    const GURL& url,
    const GURL& referrer_url,
    bool calculate_hash,
    scoped_ptr<ByteStreamReader> stream,
    const net::BoundNetLog& bound_net_log,
    scoped_ptr<PowerSaveBlocker> power_save_blocker,
    base::WeakPtr<DownloadDestinationObserver> observer,
    const TestFileErrorInjector::FileErrorInfo& error_info,
    const ConstructionCallback& ctor_callback,
    const DestructionCallback& dtor_callback)
        : DownloadFileImpl(
            save_info.Pass(), default_download_directory, url, referrer_url,
            calculate_hash, stream.Pass(), bound_net_log,
            power_save_blocker.Pass(), observer),
          source_url_(url),
          error_info_(error_info),
          destruction_callback_(dtor_callback) {
  ctor_callback.Run(source_url_);
}

DownloadFileWithErrors::~DownloadFileWithErrors() {
  destruction_callback_.Run(source_url_);
}

void DownloadFileWithErrors::Initialize(
    const InitializeCallback& callback) {
  DownloadInterruptReason error_to_return = DOWNLOAD_INTERRUPT_REASON_NONE;
  InitializeCallback callback_to_use = callback;

  // Replace callback if the error needs to be overwritten.
  if (OverwriteError(
          TestFileErrorInjector::FILE_OPERATION_INITIALIZE,
          &error_to_return)) {
    callback_to_use = base::Bind(&InitializeErrorCallback, callback,
                                 error_to_return);
  }

  DownloadFileImpl::Initialize(callback_to_use);
}

DownloadInterruptReason DownloadFileWithErrors::AppendDataToFile(
    const char* data, size_t data_len) {
  return ShouldReturnError(
      TestFileErrorInjector::FILE_OPERATION_WRITE,
      DownloadFileImpl::AppendDataToFile(data, data_len));
}

void DownloadFileWithErrors::RenameAndUniquify(
    const FilePath& full_path,
    const RenameCompletionCallback& callback) {
  DownloadInterruptReason error_to_return = DOWNLOAD_INTERRUPT_REASON_NONE;
  RenameCompletionCallback callback_to_use = callback;

  // Replace callback if the error needs to be overwritten.
  if (OverwriteError(
          TestFileErrorInjector::FILE_OPERATION_RENAME_UNIQUIFY,
          &error_to_return)) {
    callback_to_use = base::Bind(&RenameErrorCallback, callback,
                                 error_to_return);
  }

  DownloadFileImpl::RenameAndUniquify(full_path, callback_to_use);
}

void DownloadFileWithErrors::RenameAndAnnotate(
    const FilePath& full_path,
    const RenameCompletionCallback& callback) {
  DownloadInterruptReason error_to_return = DOWNLOAD_INTERRUPT_REASON_NONE;
  RenameCompletionCallback callback_to_use = callback;

  // Replace callback if the error needs to be overwritten.
  if (OverwriteError(
          TestFileErrorInjector::FILE_OPERATION_RENAME_ANNOTATE,
          &error_to_return)) {
    callback_to_use = base::Bind(&RenameErrorCallback, callback,
                                 error_to_return);
  }

  DownloadFileImpl::RenameAndAnnotate(full_path, callback_to_use);
}

bool DownloadFileWithErrors::OverwriteError(
    TestFileErrorInjector::FileOperationCode code,
    DownloadInterruptReason* output_error) {
  int counter = operation_counter_[code]++;

  if (code != error_info_.code)
    return false;

  if (counter != error_info_.operation_instance)
    return false;

  *output_error = error_info_.error;
  return true;
}

DownloadInterruptReason DownloadFileWithErrors::ShouldReturnError(
    TestFileErrorInjector::FileOperationCode code,
    DownloadInterruptReason original_error) {
  DownloadInterruptReason output_error = original_error;
  OverwriteError(code, &output_error);
  return output_error;
}

}  // namespace

// A factory for constructing DownloadFiles that inject errors.
class DownloadFileWithErrorsFactory : public DownloadFileFactory {
 public:
  DownloadFileWithErrorsFactory(
      const DownloadFileWithErrors::ConstructionCallback& ctor_callback,
      const DownloadFileWithErrors::DestructionCallback& dtor_callback);
  virtual ~DownloadFileWithErrorsFactory();

  // DownloadFileFactory interface.
  virtual DownloadFile* CreateFile(
      scoped_ptr<DownloadSaveInfo> save_info,
      const FilePath& default_download_directory,
      const GURL& url,
      const GURL& referrer_url,
      bool calculate_hash,
      scoped_ptr<ByteStreamReader> stream,
      const net::BoundNetLog& bound_net_log,
      base::WeakPtr<DownloadDestinationObserver> observer) OVERRIDE;

  bool AddError(
      const TestFileErrorInjector::FileErrorInfo& error_info);

  void ClearErrors();

 private:
  // Our injected error list, mapped by URL.  One per file.
   TestFileErrorInjector::ErrorMap injected_errors_;

  // Callback for creation and destruction.
  DownloadFileWithErrors::ConstructionCallback construction_callback_;
  DownloadFileWithErrors::DestructionCallback destruction_callback_;
};

DownloadFileWithErrorsFactory::DownloadFileWithErrorsFactory(
    const DownloadFileWithErrors::ConstructionCallback& ctor_callback,
    const DownloadFileWithErrors::DestructionCallback& dtor_callback)
        : construction_callback_(ctor_callback),
          destruction_callback_(dtor_callback) {
}

DownloadFileWithErrorsFactory::~DownloadFileWithErrorsFactory() {
}

DownloadFile* DownloadFileWithErrorsFactory::CreateFile(
    scoped_ptr<DownloadSaveInfo> save_info,
    const FilePath& default_download_directory,
    const GURL& url,
    const GURL& referrer_url,
    bool calculate_hash,
    scoped_ptr<ByteStreamReader> stream,
    const net::BoundNetLog& bound_net_log,
    base::WeakPtr<DownloadDestinationObserver> observer) {
  if (injected_errors_.find(url.spec()) == injected_errors_.end()) {
    // Have to create entry, because FileErrorInfo is not a POD type.
    TestFileErrorInjector::FileErrorInfo err_info = {
      url.spec(),
      TestFileErrorInjector::FILE_OPERATION_INITIALIZE,
      -1,
      DOWNLOAD_INTERRUPT_REASON_NONE
    };
    injected_errors_[url.spec()] = err_info;
  }

  scoped_ptr<PowerSaveBlocker> psb(
      PowerSaveBlocker::Create(
          PowerSaveBlocker::kPowerSaveBlockPreventAppSuspension,
          "Download in progress"));

  return new DownloadFileWithErrors(
      save_info.Pass(),
      default_download_directory,
      url,
      referrer_url,
      calculate_hash,
      stream.Pass(),
      bound_net_log,
      psb.Pass(),
      observer,
      injected_errors_[url.spec()],
      construction_callback_,
      destruction_callback_);
}

bool DownloadFileWithErrorsFactory::AddError(
    const TestFileErrorInjector::FileErrorInfo& error_info) {
  // Creates an empty entry if necessary.  Duplicate entries overwrite.
  injected_errors_[error_info.url] = error_info;

  return true;
}

void DownloadFileWithErrorsFactory::ClearErrors() {
  injected_errors_.clear();
}

TestFileErrorInjector::TestFileErrorInjector(
    scoped_refptr<DownloadManager> download_manager)
    : created_factory_(NULL),
      // This code is only used for browser_tests, so a
      // DownloadManager is always a DownloadManagerImpl.
      download_manager_(
          static_cast<DownloadManagerImpl*>(download_manager.get())) {
  // Record the value of the pointer, for later validation.
  created_factory_ =
      new DownloadFileWithErrorsFactory(
          base::Bind(&TestFileErrorInjector::RecordDownloadFileConstruction,
                     this),
          base::Bind(&TestFileErrorInjector::RecordDownloadFileDestruction,
                     this));

  // We will transfer ownership of the factory to the download manager.
  scoped_ptr<DownloadFileFactory> download_file_factory(
      created_factory_);

  download_manager_->SetDownloadFileFactoryForTesting(
      download_file_factory.Pass());
}

TestFileErrorInjector::~TestFileErrorInjector() {
}

bool TestFileErrorInjector::AddError(const FileErrorInfo& error_info) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK_LE(0, error_info.operation_instance);
  DCHECK(injected_errors_.find(error_info.url) == injected_errors_.end());

  // Creates an empty entry if necessary.
  injected_errors_[error_info.url] = error_info;

  return true;
}

void TestFileErrorInjector::ClearErrors() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  injected_errors_.clear();
}

bool TestFileErrorInjector::InjectErrors() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  ClearFoundFiles();

  DCHECK_EQ(static_cast<DownloadFileFactory*>(created_factory_),
            download_manager_->GetDownloadFileFactoryForTesting());

  created_factory_->ClearErrors();

  for (ErrorMap::const_iterator it = injected_errors_.begin();
       it != injected_errors_.end(); ++it)
    created_factory_->AddError(it->second);

  return true;
}

size_t TestFileErrorInjector::CurrentFileCount() const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return files_.size();
}

size_t TestFileErrorInjector::TotalFileCount() const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return found_files_.size();
}


bool TestFileErrorInjector::HadFile(const GURL& url) const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  return (found_files_.find(url) != found_files_.end());
}

void TestFileErrorInjector::ClearFoundFiles() {
  found_files_.clear();
}

void TestFileErrorInjector::DownloadFileCreated(GURL url) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(files_.find(url) == files_.end());

  files_.insert(url);
  found_files_.insert(url);
}

void TestFileErrorInjector::DestroyingDownloadFile(GURL url) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(files_.find(url) != files_.end());

  files_.erase(url);
}

void TestFileErrorInjector::RecordDownloadFileConstruction(const GURL& url) {
  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(&TestFileErrorInjector::DownloadFileCreated, this, url));
}

void TestFileErrorInjector::RecordDownloadFileDestruction(const GURL& url) {
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
      base::Bind(&TestFileErrorInjector::DestroyingDownloadFile, this, url));
}

// static
scoped_refptr<TestFileErrorInjector> TestFileErrorInjector::Create(
    scoped_refptr<DownloadManager> download_manager) {
  static bool visited = false;
  DCHECK(!visited);  // Only allowed to be called once.
  visited = true;

  scoped_refptr<TestFileErrorInjector> single_injector(
      new TestFileErrorInjector(download_manager));

  return single_injector;
}

// static
std::string TestFileErrorInjector::DebugString(FileOperationCode code) {
  switch (code) {
    case FILE_OPERATION_INITIALIZE:
      return "INITIALIZE";
    case FILE_OPERATION_WRITE:
      return "WRITE";
    case FILE_OPERATION_RENAME_UNIQUIFY:
      return "RENAME_UNIQUIFY";
    case FILE_OPERATION_RENAME_ANNOTATE:
      return "RENAME_ANNOTATE";
    default:
      break;
  }

  return "Unknown";
}

}  // namespace content
