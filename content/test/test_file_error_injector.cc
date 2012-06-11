// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/test_file_error_injector.h"

#include <vector>

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "content/browser/download/download_create_info.h"
#include "content/browser/download/download_file_impl.h"
#include "content/browser/download/download_file_manager.h"
#include "content/browser/power_save_blocker.h"
#include "content/browser/renderer_host/resource_dispatcher_host_impl.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/download_id.h"
#include "googleurl/src/gurl.h"

namespace content {
class ByteStreamReader;
}

namespace {

DownloadFileManager* GetDownloadFileManager() {
  content::ResourceDispatcherHostImpl* rdh =
      content::ResourceDispatcherHostImpl::Get();
  DCHECK(rdh != NULL);
  return rdh->download_file_manager();
}

// A class that performs file operations and injects errors.
class DownloadFileWithErrors: public DownloadFileImpl {
 public:
  typedef base::Callback<void(const GURL& url, content::DownloadId id)>
      ConstructionCallback;
  typedef base::Callback<void(const GURL& url)> DestructionCallback;

  DownloadFileWithErrors(
      const DownloadCreateInfo* info,
      scoped_ptr<content::ByteStreamReader> stream,
      DownloadRequestHandleInterface* request_handle,
      content::DownloadManager* download_manager,
      bool calculate_hash,
      const net::BoundNetLog& bound_net_log,
      const content::TestFileErrorInjector::FileErrorInfo& error_info,
      const ConstructionCallback& ctor_callback,
      const DestructionCallback& dtor_callback);

  ~DownloadFileWithErrors();

  // DownloadFile interface.
  virtual net::Error Initialize() OVERRIDE;
  virtual net::Error AppendDataToFile(const char* data,
                                      size_t data_len) OVERRIDE;
  virtual net::Error Rename(const FilePath& full_path) OVERRIDE;

 private:
  // Error generating helper.
  net::Error ShouldReturnError(
      content::TestFileErrorInjector::FileOperationCode code,
      net::Error original_net_error);

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

DownloadFileWithErrors::DownloadFileWithErrors(
    const DownloadCreateInfo* info,
    scoped_ptr<content::ByteStreamReader> stream,
    DownloadRequestHandleInterface* request_handle,
    content::DownloadManager* download_manager,
    bool calculate_hash,
    const net::BoundNetLog& bound_net_log,
    const content::TestFileErrorInjector::FileErrorInfo& error_info,
    const ConstructionCallback& ctor_callback,
    const DestructionCallback& dtor_callback)
        : DownloadFileImpl(info,
                           stream.Pass(),
                           request_handle,
                           download_manager,
                           calculate_hash,
                           scoped_ptr<content::PowerSaveBlocker>(NULL).Pass(),
                           bound_net_log),
          source_url_(info->url()),
          error_info_(error_info),
          destruction_callback_(dtor_callback) {
  ctor_callback.Run(source_url_, info->download_id);
}

DownloadFileWithErrors::~DownloadFileWithErrors() {
  destruction_callback_.Run(source_url_);
}

net::Error DownloadFileWithErrors::Initialize() {
  return ShouldReturnError(
      content::TestFileErrorInjector::FILE_OPERATION_INITIALIZE,
      DownloadFileImpl::Initialize());
}

net::Error DownloadFileWithErrors::AppendDataToFile(const char* data,
                                                    size_t data_len) {
  return ShouldReturnError(
      content::TestFileErrorInjector::FILE_OPERATION_WRITE,
      DownloadFileImpl::AppendDataToFile(data, data_len));
}

net::Error DownloadFileWithErrors::Rename(const FilePath& full_path) {
  return ShouldReturnError(
      content::TestFileErrorInjector::FILE_OPERATION_RENAME,
      DownloadFileImpl::Rename(full_path));
}

net::Error DownloadFileWithErrors::ShouldReturnError(
    content::TestFileErrorInjector::FileOperationCode code,
    net::Error original_net_error) {
  int counter = operation_counter_[code];
  ++operation_counter_[code];

  if (code != error_info_.code)
    return original_net_error;

  if (counter != error_info_.operation_instance)
    return original_net_error;

  VLOG(1) << " " << __FUNCTION__ << "()"
          << " url = '" << source_url_.spec() << "'"
          << " code = " << content::TestFileErrorInjector::DebugString(code)
          << " (" << code << ")"
          << " counter = " << counter
          << " original_error = " << net::ErrorToString(original_net_error)
          << " (" << original_net_error << ")"
          << " new error = " << net::ErrorToString(error_info_.net_error)
          << " (" << error_info_.net_error << ")";

  return error_info_.net_error;
}

}  // namespace

namespace content {

// A factory for constructing DownloadFiles that inject errors.
class DownloadFileWithErrorsFactory
    : public DownloadFileManager::DownloadFileFactory {
 public:

  DownloadFileWithErrorsFactory(
      const DownloadFileWithErrors::ConstructionCallback& ctor_callback,
      const DownloadFileWithErrors::DestructionCallback& dtor_callback);
  virtual ~DownloadFileWithErrorsFactory();

  // DownloadFileFactory interface.
  virtual content::DownloadFile* CreateFile(
      DownloadCreateInfo* info,
      scoped_ptr<content::ByteStreamReader> stream,
      const DownloadRequestHandle& request_handle,
      content::DownloadManager* download_manager,
      bool calculate_hash,
      const net::BoundNetLog& bound_net_log);

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
    DownloadCreateInfo* info,
    scoped_ptr<content::ByteStreamReader> stream,
    const DownloadRequestHandle& request_handle,
    content::DownloadManager* download_manager,
    bool calculate_hash,
    const net::BoundNetLog& bound_net_log) {
  std::string url = info->url().spec();

  if (injected_errors_.find(url) == injected_errors_.end()) {
    // Have to create entry, because FileErrorInfo is not a POD type.
    TestFileErrorInjector::FileErrorInfo err_info = {
      url,
      TestFileErrorInjector::FILE_OPERATION_INITIALIZE,
      -1,
      net::OK
    };
    injected_errors_[url] = err_info;
  }

  return new DownloadFileWithErrors(info,
                                    stream.Pass(),
                                    new DownloadRequestHandle(request_handle),
                                    download_manager,
                                    calculate_hash,
                                    bound_net_log,
                                    injected_errors_[url],
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

TestFileErrorInjector::TestFileErrorInjector()
    : created_factory_(NULL) {
  // Record the value of the pointer, for later validation.
  created_factory_ =
      new DownloadFileWithErrorsFactory(
          base::Bind(&TestFileErrorInjector::
                         RecordDownloadFileConstruction,
                     this),
          base::Bind(&TestFileErrorInjector::
                         RecordDownloadFileDestruction,
                     this));

  // We will transfer ownership of the factory to the download file manager.
  scoped_ptr<DownloadFileWithErrorsFactory> download_file_factory(
      created_factory_);

  content::BrowserThread::PostTask(
      content::BrowserThread::FILE,
      FROM_HERE,
      base::Bind(&TestFileErrorInjector::AddFactory,
                 this,
                 base::Passed(&download_file_factory)));
}

TestFileErrorInjector::~TestFileErrorInjector() {
}

void TestFileErrorInjector::AddFactory(
    scoped_ptr<DownloadFileWithErrorsFactory> factory) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::FILE));

  DownloadFileManager* download_file_manager = GetDownloadFileManager();
  DCHECK(download_file_manager);

  // Convert to base class pointer, for GCC.
  scoped_ptr<DownloadFileManager::DownloadFileFactory> plain_factory(
      factory.release());

  download_file_manager->SetFileFactoryForTesting(plain_factory.Pass());
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

  content::BrowserThread::PostTask(
      content::BrowserThread::FILE,
      FROM_HERE,
      base::Bind(&TestFileErrorInjector::InjectErrorsOnFileThread,
                 this,
                 injected_errors_,
                 created_factory_));

  return true;
}

void TestFileErrorInjector::InjectErrorsOnFileThread(
    ErrorMap map, DownloadFileWithErrorsFactory* factory) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::FILE));

  // Validate that our factory is in use.
  DownloadFileManager* download_file_manager = GetDownloadFileManager();
  DCHECK(download_file_manager);

  DownloadFileManager::DownloadFileFactory* file_factory =
      download_file_manager->GetFileFactoryForTesting();

  // Validate that we still have the same factory.
  DCHECK_EQ(static_cast<DownloadFileManager::DownloadFileFactory*>(factory),
            file_factory);

  // We want to replace all existing injection errors.
  factory->ClearErrors();

  for (ErrorMap::const_iterator it = map.begin(); it != map.end(); ++it)
    factory->AddError(it->second);
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

const content::DownloadId TestFileErrorInjector::GetId(
    const GURL& url) const {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  FileMap::const_iterator it = found_files_.find(url);
  if (it == found_files_.end())
    return content::DownloadId::Invalid();

  return it->second;
}

void TestFileErrorInjector::ClearFoundFiles() {
  found_files_.clear();
}

void TestFileErrorInjector::DownloadFileCreated(GURL url,
                                                    content::DownloadId id) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  DCHECK(files_.find(url) == files_.end());

  files_[url] = id;
  found_files_[url] = id;
}

void TestFileErrorInjector::DestroyingDownloadFile(GURL url) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  DCHECK(files_.find(url) != files_.end());

  files_.erase(url);
}

void TestFileErrorInjector::RecordDownloadFileConstruction(
    const GURL& url, content::DownloadId id) {
  content::BrowserThread::PostTask(
      content::BrowserThread::UI,
      FROM_HERE,
      base::Bind(&TestFileErrorInjector::DownloadFileCreated,
                 this,
                 url,
                 id));
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
scoped_refptr<TestFileErrorInjector> TestFileErrorInjector::Create() {
  static bool visited = false;
  DCHECK(!visited);  // Only allowed to be called once.
  visited = true;

  scoped_refptr<TestFileErrorInjector> single_injector(
      new TestFileErrorInjector);

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
