// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/test_file_error_injector.h"

#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "content/browser/download/download_file_factory.h"
#include "content/browser/download/download_file_impl.h"
#include "content/browser/download/download_interrupt_reasons_impl.h"
#include "content/browser/download/download_manager_impl.h"
#include "content/browser/loader/resource_dispatcher_host_impl.h"
#include "content/public/browser/browser_thread.h"
#include "url/gurl.h"

namespace net {
class NetLogWithSource;
}

namespace content {
class ByteStreamReader;

namespace {

// A class that performs file operations and injects errors.
class DownloadFileWithError: public DownloadFileImpl {
 public:
  DownloadFileWithError(std::unique_ptr<DownloadSaveInfo> save_info,
                        const base::FilePath& default_download_directory,
                        std::unique_ptr<ByteStreamReader> byte_stream,
                        const net::NetLogWithSource& net_log,
                        base::WeakPtr<DownloadDestinationObserver> observer,
                        const TestFileErrorInjector::FileErrorInfo& error_info,
                        const base::Closure& ctor_callback,
                        const base::Closure& dtor_callback);

  ~DownloadFileWithError() override;

  void Initialize(const InitializeCallback& callback) override;

  // DownloadFile interface.
  DownloadInterruptReason AppendDataToFile(const char* data,
                                           size_t data_len) override;
  void RenameAndUniquify(const base::FilePath& full_path,
                         const RenameCompletionCallback& callback) override;
  void RenameAndAnnotate(const base::FilePath& full_path,
                         const std::string& client_guid,
                         const GURL& source_url,
                         const GURL& referrer_url,
                         const RenameCompletionCallback& callback) override;

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

  // Our injected error.  Only one per file.
  TestFileErrorInjector::FileErrorInfo error_info_;

  // Count per operation.  0-based.
  std::map<TestFileErrorInjector::FileOperationCode, int> operation_counter_;

  // Callback for destruction.
  base::Closure destruction_callback_;
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
    const base::FilePath& path_result) {
  original_callback.Run(
      overwrite_error,
      overwrite_error == DOWNLOAD_INTERRUPT_REASON_NONE ?
      path_result : base::FilePath());
}

DownloadFileWithError::DownloadFileWithError(
    std::unique_ptr<DownloadSaveInfo> save_info,
    const base::FilePath& default_download_directory,
    std::unique_ptr<ByteStreamReader> byte_stream,
    const net::NetLogWithSource& net_log,
    base::WeakPtr<DownloadDestinationObserver> observer,
    const TestFileErrorInjector::FileErrorInfo& error_info,
    const base::Closure& ctor_callback,
    const base::Closure& dtor_callback)
    : DownloadFileImpl(std::move(save_info),
                       default_download_directory,
                       std::move(byte_stream),
                       net_log,
                       observer),
      error_info_(error_info),
      destruction_callback_(dtor_callback) {
  // DownloadFiles are created on the UI thread and are destroyed on the FILE
  // thread. Schedule the ConstructionCallback on the FILE thread so that if a
  // DownloadItem schedules a DownloadFile to be destroyed and creates another
  // one (as happens during download resumption), then the DestructionCallback
  // for the old DownloadFile is run before the ConstructionCallback for the
  // next DownloadFile.
  BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE, ctor_callback);
}

DownloadFileWithError::~DownloadFileWithError() {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);
  destruction_callback_.Run();
}

void DownloadFileWithError::Initialize(
    const InitializeCallback& callback) {
  DownloadInterruptReason error_to_return = DOWNLOAD_INTERRUPT_REASON_NONE;
  InitializeCallback callback_to_use = callback;

  // Replace callback if the error needs to be overwritten.
  if (OverwriteError(
          TestFileErrorInjector::FILE_OPERATION_INITIALIZE,
          &error_to_return)) {
    if (DOWNLOAD_INTERRUPT_REASON_NONE != error_to_return) {
      // Don't execute a, probably successful, Initialize; just
      // return the error.
      BrowserThread::PostTask(
          BrowserThread::UI, FROM_HERE, base::Bind(
              callback, error_to_return));
      return;
    }

    // Otherwise, just wrap the return.
    callback_to_use = base::Bind(&InitializeErrorCallback, callback,
                                 error_to_return);
  }

  DownloadFileImpl::Initialize(callback_to_use);
}

DownloadInterruptReason DownloadFileWithError::AppendDataToFile(
    const char* data, size_t data_len) {
  return ShouldReturnError(
      TestFileErrorInjector::FILE_OPERATION_WRITE,
      DownloadFileImpl::AppendDataToFile(data, data_len));
}

void DownloadFileWithError::RenameAndUniquify(
    const base::FilePath& full_path,
    const RenameCompletionCallback& callback) {
  DownloadInterruptReason error_to_return = DOWNLOAD_INTERRUPT_REASON_NONE;
  RenameCompletionCallback callback_to_use = callback;

  // Replace callback if the error needs to be overwritten.
  if (OverwriteError(
          TestFileErrorInjector::FILE_OPERATION_RENAME_UNIQUIFY,
          &error_to_return)) {
    if (DOWNLOAD_INTERRUPT_REASON_NONE != error_to_return) {
      // Don't execute a, probably successful, RenameAndUniquify; just
      // return the error.
      BrowserThread::PostTask(
          BrowserThread::UI, FROM_HERE, base::Bind(
              callback, error_to_return, base::FilePath()));
      return;
    }

    // Otherwise, just wrap the return.
    callback_to_use = base::Bind(&RenameErrorCallback, callback,
                                 error_to_return);
  }

  DownloadFileImpl::RenameAndUniquify(full_path, callback_to_use);
}

void DownloadFileWithError::RenameAndAnnotate(
    const base::FilePath& full_path,
    const std::string& client_guid,
    const GURL& source_url,
    const GURL& referrer_url,
    const RenameCompletionCallback& callback) {
  DownloadInterruptReason error_to_return = DOWNLOAD_INTERRUPT_REASON_NONE;
  RenameCompletionCallback callback_to_use = callback;

  // Replace callback if the error needs to be overwritten.
  if (OverwriteError(
          TestFileErrorInjector::FILE_OPERATION_RENAME_ANNOTATE,
          &error_to_return)) {
    if (DOWNLOAD_INTERRUPT_REASON_NONE != error_to_return) {
      // Don't execute a, probably successful, RenameAndAnnotate; just
      // return the error.
      BrowserThread::PostTask(
          BrowserThread::UI, FROM_HERE, base::Bind(
              callback, error_to_return, base::FilePath()));
      return;
    }

    // Otherwise, just wrap the return.
    callback_to_use = base::Bind(&RenameErrorCallback, callback,
                                 error_to_return);
  }

  DownloadFileImpl::RenameAndAnnotate(
      full_path, client_guid, source_url, referrer_url, callback_to_use);
}

bool DownloadFileWithError::OverwriteError(
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

DownloadInterruptReason DownloadFileWithError::ShouldReturnError(
    TestFileErrorInjector::FileOperationCode code,
    DownloadInterruptReason original_error) {
  DownloadInterruptReason output_error = original_error;
  OverwriteError(code, &output_error);
  return output_error;
}

}  // namespace

// A factory for constructing DownloadFiles that inject errors.
class DownloadFileWithErrorFactory : public DownloadFileFactory {
 public:
  DownloadFileWithErrorFactory(const base::Closure& ctor_callback,
                                const base::Closure& dtor_callback);
  ~DownloadFileWithErrorFactory() override;

  // DownloadFileFactory interface.
  DownloadFile* CreateFile(
      std::unique_ptr<DownloadSaveInfo> save_info,
      const base::FilePath& default_download_directory,
      std::unique_ptr<ByteStreamReader> byte_stream,
      const net::NetLogWithSource& net_log,
      base::WeakPtr<DownloadDestinationObserver> observer) override;

  bool SetError(TestFileErrorInjector::FileErrorInfo error);

 private:
  // Our injected error.
  TestFileErrorInjector::FileErrorInfo injected_error_;

  // Callback for creation and destruction.
  base::Closure construction_callback_;
  base::Closure destruction_callback_;
};

DownloadFileWithErrorFactory::DownloadFileWithErrorFactory(
    const base::Closure& ctor_callback,
    const base::Closure& dtor_callback)
    : construction_callback_(ctor_callback),
      destruction_callback_(dtor_callback) {
  injected_error_.code = TestFileErrorInjector::FILE_OPERATION_INITIALIZE;
  injected_error_.error = DOWNLOAD_INTERRUPT_REASON_NONE;
  injected_error_.operation_instance = -1;
}

DownloadFileWithErrorFactory::~DownloadFileWithErrorFactory() {}

DownloadFile* DownloadFileWithErrorFactory::CreateFile(
    std::unique_ptr<DownloadSaveInfo> save_info,
    const base::FilePath& default_download_directory,
    std::unique_ptr<ByteStreamReader> byte_stream,
    const net::NetLogWithSource& net_log,
    base::WeakPtr<DownloadDestinationObserver> observer) {
  return new DownloadFileWithError(std::move(save_info),
                                   default_download_directory,
                                   std::move(byte_stream),
                                   net_log,
                                   observer,
                                   injected_error_,
                                   construction_callback_,
                                   destruction_callback_);
}

bool DownloadFileWithErrorFactory::SetError(
    TestFileErrorInjector::FileErrorInfo error) {
  injected_error_ = std::move(error);
  return true;
}

TestFileErrorInjector::TestFileErrorInjector(DownloadManager* download_manager)
    :  // This code is only used for browser_tests, so a
      // DownloadManager is always a DownloadManagerImpl.
      download_manager_(static_cast<DownloadManagerImpl*>(download_manager)) {
  // Record the value of the pointer, for later validation.
  created_factory_ = new DownloadFileWithErrorFactory(
      base::Bind(&TestFileErrorInjector::RecordDownloadFileConstruction, this),
      base::Bind(&TestFileErrorInjector::RecordDownloadFileDestruction, this));

  // We will transfer ownership of the factory to the download manager.
  std::unique_ptr<DownloadFileFactory> download_file_factory(created_factory_);

  download_manager_->SetDownloadFileFactoryForTesting(
      std::move(download_file_factory));
}

TestFileErrorInjector::~TestFileErrorInjector() {
}

void TestFileErrorInjector::ClearError() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // An error with an index of -1, which will never be reached.
  static const TestFileErrorInjector::FileErrorInfo kNoOpErrorInfo = {
      TestFileErrorInjector::FILE_OPERATION_INITIALIZE, -1,
      DOWNLOAD_INTERRUPT_REASON_NONE};
  InjectError(kNoOpErrorInfo);
}

bool TestFileErrorInjector::InjectError(const FileErrorInfo& error_info) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  ClearTotalFileCount();
  DCHECK_EQ(static_cast<DownloadFileFactory*>(created_factory_),
            download_manager_->GetDownloadFileFactoryForTesting());
  created_factory_->SetError(error_info);
  return true;
}

size_t TestFileErrorInjector::CurrentFileCount() const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return active_file_count_;
}

size_t TestFileErrorInjector::TotalFileCount() const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return total_file_count_;
}

void TestFileErrorInjector::ClearTotalFileCount() {
  total_file_count_ = 0;
}

void TestFileErrorInjector::DownloadFileCreated() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  ++active_file_count_;
  ++total_file_count_;
}

void TestFileErrorInjector::DestroyingDownloadFile() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  --active_file_count_;
}

void TestFileErrorInjector::RecordDownloadFileConstruction() {
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&TestFileErrorInjector::DownloadFileCreated, this));
}

void TestFileErrorInjector::RecordDownloadFileDestruction() {
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&TestFileErrorInjector::DestroyingDownloadFile, this));
}

// static
scoped_refptr<TestFileErrorInjector> TestFileErrorInjector::Create(
    DownloadManager* download_manager) {
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
