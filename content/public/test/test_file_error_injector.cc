// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/test_file_error_injector.h"

#include <vector>

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "content/browser/download/download_file_impl.h"
#include "content/browser/download/download_file_factory.h"
#include "content/browser/download/download_interrupt_reasons_impl.h"
#include "content/browser/download/download_manager_impl.h"
#include "content/browser/power_save_blocker.h"
#include "content/browser/renderer_host/resource_dispatcher_host_impl.h"
#include "content/public/browser/browser_thread.h"
#include "googleurl/src/gurl.h"

namespace content {
class ByteStreamReader;
}

namespace {

// A class that performs file operations and injects errors.
class DownloadFileWithErrors: public DownloadFileImpl {
 public:
  typedef base::Callback<void(const GURL& url)> ConstructionCallback;
  typedef base::Callback<void(const GURL& url)> DestructionCallback;

  DownloadFileWithErrors(
      const content::DownloadSaveInfo& save_info,
      const GURL& url,
      const GURL& referrer_url,
      int64 received_bytes,
      bool calculate_hash,
      scoped_ptr<content::ByteStreamReader> stream,
      const net::BoundNetLog& bound_net_log,
      scoped_ptr<content::PowerSaveBlocker> power_save_blocker,
      base::WeakPtr<content::DownloadDestinationObserver> observer,
      const content::TestFileErrorInjector::FileErrorInfo& error_info,
      const ConstructionCallback& ctor_callback,
      const DestructionCallback& dtor_callback);

  ~DownloadFileWithErrors();

  virtual void Initialize(const InitializeCallback& callback) OVERRIDE;

  // DownloadFile interface.
  virtual content::DownloadInterruptReason AppendDataToFile(
      const char* data, size_t data_len) OVERRIDE;
  virtual void Rename(const FilePath& full_path,
                      bool overwrite_existing_file,
                      const RenameCompletionCallback& callback) OVERRIDE;

 private:
  // Error generating helper.
  content::DownloadInterruptReason ShouldReturnError(
      content::TestFileErrorInjector::FileOperationCode code,
      content::DownloadInterruptReason original_error);

  // Determine whether to overwrite an operation with the given code
  // with a substitute error; if returns true, |*original_error| is
  // written with the error to use for overwriting.
  // NOTE: This routine changes state; specifically, it increases the
  // operations counts for the specified code.  It should only be called
  // once per operation.
  bool OverwriteError(
    content::TestFileErrorInjector::FileOperationCode code,
    content::DownloadInterruptReason* output_error);

  // Source URL for the file being downloaded.
  GURL source_url_;

  // Our injected error.  Only one per file.
  content::TestFileErrorInjector::FileErrorInfo error_info_;

  // Count per operation.  0-based.
  std::map<content::TestFileErrorInjector::FileOperationCode, int>
      operation_counter_;

  // Callback for destruction.
  DestructionCallback destruction_callback_;
};

static void InitializeErrorCallback(
    const content::DownloadFile::InitializeCallback original_callback,
    content::DownloadInterruptReason overwrite_error,
    content::DownloadInterruptReason original_error) {
  original_callback.Run(overwrite_error);
}

static void RenameErrorCallback(
    const content::DownloadFile::RenameCompletionCallback original_callback,
    content::DownloadInterruptReason overwrite_error,
    content::DownloadInterruptReason original_error,
    const FilePath& path_result) {
  original_callback.Run(
      overwrite_error,
      overwrite_error == content::DOWNLOAD_INTERRUPT_REASON_NONE ?
      path_result : FilePath());
}

DownloadFileWithErrors::DownloadFileWithErrors(
    const content::DownloadSaveInfo& save_info,
    const GURL& url,
    const GURL& referrer_url,
    int64 received_bytes,
    bool calculate_hash,
    scoped_ptr<content::ByteStreamReader> stream,
    const net::BoundNetLog& bound_net_log,
    scoped_ptr<content::PowerSaveBlocker> power_save_blocker,
    base::WeakPtr<content::DownloadDestinationObserver> observer,
    const content::TestFileErrorInjector::FileErrorInfo& error_info,
    const ConstructionCallback& ctor_callback,
    const DestructionCallback& dtor_callback)
        : DownloadFileImpl(
            save_info, url, referrer_url, received_bytes, calculate_hash,
            stream.Pass(), bound_net_log, power_save_blocker.Pass(),
            observer),
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
  content::DownloadInterruptReason error_to_return =
      content::DOWNLOAD_INTERRUPT_REASON_NONE;
  InitializeCallback callback_to_use = callback;

  // Replace callback if the error needs to be overwritten.
  if (OverwriteError(
          content::TestFileErrorInjector::FILE_OPERATION_INITIALIZE,
          &error_to_return)) {
    callback_to_use = base::Bind(&InitializeErrorCallback, callback,
                                 error_to_return);
  }

  DownloadFileImpl::Initialize(callback_to_use);
}

content::DownloadInterruptReason DownloadFileWithErrors::AppendDataToFile(
    const char* data, size_t data_len) {
  return ShouldReturnError(
      content::TestFileErrorInjector::FILE_OPERATION_WRITE,
      DownloadFileImpl::AppendDataToFile(data, data_len));
}

void DownloadFileWithErrors::Rename(
    const FilePath& full_path,
    bool overwrite_existing_file,
    const RenameCompletionCallback& callback) {
  content::DownloadInterruptReason error_to_return =
      content::DOWNLOAD_INTERRUPT_REASON_NONE;
  RenameCompletionCallback callback_to_use = callback;

  // Replace callback if the error needs to be overwritten.
  if (OverwriteError(
          content::TestFileErrorInjector::FILE_OPERATION_RENAME,
          &error_to_return)) {
    callback_to_use = base::Bind(&RenameErrorCallback, callback,
                                 error_to_return);
  }

  DownloadFileImpl::Rename(full_path, overwrite_existing_file, callback_to_use);
}

bool DownloadFileWithErrors::OverwriteError(
    content::TestFileErrorInjector::FileOperationCode code,
    content::DownloadInterruptReason* output_error) {
  int counter = operation_counter_[code]++;

  if (code != error_info_.code)
    return false;

  if (counter != error_info_.operation_instance)
    return false;

  *output_error = error_info_.error;
  return true;
}

content::DownloadInterruptReason DownloadFileWithErrors::ShouldReturnError(
    content::TestFileErrorInjector::FileOperationCode code,
    content::DownloadInterruptReason original_error) {
  content::DownloadInterruptReason output_error = original_error;
  OverwriteError(code, &output_error);
  return output_error;
}

}  // namespace

namespace content {

// A factory for constructing DownloadFiles that inject errors.
class DownloadFileWithErrorsFactory : public DownloadFileFactory {
 public:

  DownloadFileWithErrorsFactory(
      const DownloadFileWithErrors::ConstructionCallback& ctor_callback,
      const DownloadFileWithErrors::DestructionCallback& dtor_callback);
  virtual ~DownloadFileWithErrorsFactory();

  // DownloadFileFactory interface.
  virtual DownloadFile* CreateFile(
    const content::DownloadSaveInfo& save_info,
    GURL url,
    GURL referrer_url,
    int64 received_bytes,
    bool calculate_hash,
    scoped_ptr<content::ByteStreamReader> stream,
    const net::BoundNetLog& bound_net_log,
    base::WeakPtr<content::DownloadDestinationObserver> observer);

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

content::DownloadFile* DownloadFileWithErrorsFactory::CreateFile(
    const content::DownloadSaveInfo& save_info,
    GURL url,
    GURL referrer_url,
    int64 received_bytes,
    bool calculate_hash,
    scoped_ptr<content::ByteStreamReader> stream,
    const net::BoundNetLog& bound_net_log,
    base::WeakPtr<content::DownloadDestinationObserver> observer) {
  if (injected_errors_.find(url.spec()) == injected_errors_.end()) {
    // Have to create entry, because FileErrorInfo is not a POD type.
    TestFileErrorInjector::FileErrorInfo err_info = {
      url.spec(),
      TestFileErrorInjector::FILE_OPERATION_INITIALIZE,
      -1,
      content::DOWNLOAD_INTERRUPT_REASON_NONE
    };
    injected_errors_[url.spec()] = err_info;
  }

  scoped_ptr<content::PowerSaveBlocker> psb(
      new content::PowerSaveBlocker(
          content::PowerSaveBlocker::kPowerSaveBlockPreventAppSuspension,
          "Download in progress"));
  return new DownloadFileWithErrors(
      save_info,
      url,
      referrer_url,
      received_bytes,
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
    scoped_refptr<content::DownloadManager> download_manager)
    : created_factory_(NULL),
      // This code is only used for browser_tests, so a
      // DownloadManager is always a DownloadManagerImpl.
      download_manager_(
          static_cast<DownloadManagerImpl*>(download_manager.release())) {
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
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  DCHECK_LE(0, error_info.operation_instance);
  DCHECK(injected_errors_.find(error_info.url) == injected_errors_.end());

  // Creates an empty entry if necessary.
  injected_errors_[error_info.url] = error_info;

  return true;
}

void TestFileErrorInjector::ClearErrors() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  injected_errors_.clear();
}

bool TestFileErrorInjector::InjectErrors() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  ClearFoundFiles();

  DCHECK_EQ(static_cast<content::DownloadFileFactory*>(created_factory_),
            download_manager_->GetDownloadFileFactoryForTesting());

  created_factory_->ClearErrors();

  for (ErrorMap::const_iterator it = injected_errors_.begin();
       it != injected_errors_.end(); ++it)
    created_factory_->AddError(it->second);

  return true;
}

size_t TestFileErrorInjector::CurrentFileCount() const {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  return files_.size();
}

size_t TestFileErrorInjector::TotalFileCount() const {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  return found_files_.size();
}


bool TestFileErrorInjector::HadFile(const GURL& url) const {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  return (found_files_.find(url) != found_files_.end());
}

void TestFileErrorInjector::ClearFoundFiles() {
  found_files_.clear();
}

void TestFileErrorInjector::DownloadFileCreated(GURL url) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  DCHECK(files_.find(url) == files_.end());

  files_.insert(url);
  found_files_.insert(url);
}

void TestFileErrorInjector::DestroyingDownloadFile(GURL url) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  DCHECK(files_.find(url) != files_.end());

  files_.erase(url);
}

void TestFileErrorInjector::RecordDownloadFileConstruction(const GURL& url) {
  content::BrowserThread::PostTask(
      content::BrowserThread::UI,
      FROM_HERE,
      base::Bind(&TestFileErrorInjector::DownloadFileCreated,
                 this,
                 url));
}

void TestFileErrorInjector::RecordDownloadFileDestruction(const GURL& url) {
  content::BrowserThread::PostTask(
      content::BrowserThread::UI,
      FROM_HERE,
      base::Bind(&TestFileErrorInjector::DestroyingDownloadFile,
                 this,
                 url));
}

// static
scoped_refptr<TestFileErrorInjector> TestFileErrorInjector::Create(
    scoped_refptr<content::DownloadManager> download_manager) {
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
    case FILE_OPERATION_RENAME:
      return "RENAME";
    default:
      break;
  }

  return "Unknown";
}

}  // namespace content
