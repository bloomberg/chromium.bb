// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/local_discovery/storage/privet_filesystem_operations.h"

#include "chrome/browser/local_discovery/storage/privet_filesystem_constants.h"

namespace local_discovery {

PrivetFileSystemOperationFactory::PrivetFileSystemOperationFactory(
    content::BrowserContext* browser_context)
    : browser_context_(browser_context), weak_factory_(this) {}

PrivetFileSystemOperationFactory::~PrivetFileSystemOperationFactory() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  RemoveAllOperations();
}

void PrivetFileSystemOperationFactory::GetFileInfo(
    const storage::FileSystemURL& url,
    const storage::AsyncFileUtil::GetFileInfoCallback& callback) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  PrivetFileSystemAsyncOperation* operation =
      new PrivetFileSystemDetailsOperation(
          url.path(), browser_context_, this, &attribute_cache_, callback);
  async_operations_.insert(operation);
  operation->Start();
}

void PrivetFileSystemOperationFactory::ReadDirectory(
    const storage::FileSystemURL& url,
    const storage::AsyncFileUtil::ReadDirectoryCallback& callback) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  PrivetFileSystemAsyncOperation* operation = new PrivetFileSystemListOperation(
      url.path(), browser_context_, this, &attribute_cache_, callback);
  async_operations_.insert(operation);
  operation->Start();
}

void PrivetFileSystemOperationFactory::RemoveOperation(
    PrivetFileSystemAsyncOperation* operation) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  async_operations_.erase(operation);
  delete operation;
}

void PrivetFileSystemOperationFactory::RemoveAllOperations() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  STLDeleteElements(&async_operations_);
}

PrivetFileSystemAsyncOperationUtil::PrivetFileSystemAsyncOperationUtil(
    const base::FilePath& full_path,
    content::BrowserContext* browser_context,
    Delegate* delegate)
    : parsed_path_(full_path),
      browser_context_(browser_context),
      delegate_(delegate),
      weak_factory_(this) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
}

PrivetFileSystemAsyncOperationUtil::~PrivetFileSystemAsyncOperationUtil() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
}

void PrivetFileSystemAsyncOperationUtil::Start() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  request_context_ = browser_context_->GetRequestContext();
  service_discovery_client_ = ServiceDiscoverySharedClient::GetInstance();
  privet_device_resolver_.reset(new PrivetDeviceResolver(
      service_discovery_client_.get(),
      parsed_path_.service_name,
      base::Bind(
          &PrivetFileSystemAsyncOperationUtil::OnGotDeviceDescription,
          base::Unretained(this))));
  privet_device_resolver_->Start();
}

void PrivetFileSystemAsyncOperationUtil::OnGotDeviceDescription(
    bool success, const DeviceDescription& device_description) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  if (!success) {
    delegate_->PrivetFileSystemResolved(NULL, parsed_path_.path);
    return;
  }

  privet_async_factory_ = PrivetHTTPAsynchronousFactory::CreateInstance(
      service_discovery_client_.get(), request_context_.get());
  privet_http_resolution_ = privet_async_factory_->CreatePrivetHTTP(
      parsed_path_.service_name,
      device_description.address,
      base::Bind(&PrivetFileSystemAsyncOperationUtil::OnGotPrivetHTTP,
                 base::Unretained(this)));
  privet_http_resolution_->Start();
}

void PrivetFileSystemAsyncOperationUtil::OnGotPrivetHTTP(
    scoped_ptr<PrivetHTTPClient> privet_http_client) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  privet_client_ = PrivetV1HTTPClient::CreateDefault(privet_http_client.Pass());
  delegate_->PrivetFileSystemResolved(privet_client_.get(),
                                      parsed_path_.path);
}

PrivetFileSystemListOperation::PrivetFileSystemListOperation(
    const base::FilePath& full_path,
    content::BrowserContext* browser_context,
    PrivetFileSystemAsyncOperationContainer* container,
    PrivetFileSystemAttributeCache* attribute_cache,
    const storage::AsyncFileUtil::ReadDirectoryCallback& callback)
    : core_(full_path, browser_context, this),
      full_path_(full_path),
      container_(container),
      attribute_cache_(attribute_cache),
      callback_(callback) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
}

PrivetFileSystemListOperation::~PrivetFileSystemListOperation() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
}

void PrivetFileSystemListOperation::Start() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  core_.Start();
}

void PrivetFileSystemListOperation::PrivetFileSystemResolved(
    PrivetV1HTTPClient* http_client,
    const std::string& path) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  if (!http_client) {
    SignalError();
    return;
  }

  list_operation_ = http_client->CreateStorageListOperation(
      path,
      base::Bind(&PrivetFileSystemListOperation::OnStorageListResult,
                 base::Unretained(this)));
    list_operation_->Start();
}

void PrivetFileSystemListOperation::OnStorageListResult(
    const base::DictionaryValue* value) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  list_operation_.reset();

  if (!value) {
    SignalError();
    return;
  }

  attribute_cache_->AddFileInfoFromJSON(full_path_, value);

  storage::AsyncFileUtil::EntryList entry_list;

  const base::ListValue* entries;
  if (!value->GetList(kPrivetListEntries, &entries)) {
    SignalError();
    return;
  }

  for (size_t i = 0; i < entries->GetSize(); i++) {
    const base::DictionaryValue* entry_value;
    if (!entries->GetDictionary(i, &entry_value)) {
      SignalError();
      return;
    }

    std::string name;
    std::string type;
    int size = 0;

    entry_value->GetString(kPrivetListKeyName, &name);
    entry_value->GetString(kPrivetListKeyType, &type);
    entry_value->GetInteger(kPrivetListKeySize, &size);

    storage::DirectoryEntry entry(name,
                                  (type == kPrivetListTypeDir)
                                      ? storage::DirectoryEntry::DIRECTORY
                                      : storage::DirectoryEntry::FILE,
                                  size,
                                  base::Time() /* TODO(noamsml) */);

    entry_list.push_back(entry);
  }

  TriggerCallbackAndDestroy(base::File::FILE_OK, entry_list, false);
}

void PrivetFileSystemListOperation::SignalError() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  TriggerCallbackAndDestroy(base::File::FILE_ERROR_FAILED,
                            storage::AsyncFileUtil::EntryList(),
                            false);
}

void PrivetFileSystemListOperation::TriggerCallbackAndDestroy(
    base::File::Error result,
    const storage::AsyncFileUtil::EntryList& file_list,
    bool has_more) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  content::BrowserThread::PostTask(
      content::BrowserThread::IO,
      FROM_HERE,
      base::Bind(callback_, result, file_list, has_more));
  container_->RemoveOperation(this);
}

PrivetFileSystemDetailsOperation::PrivetFileSystemDetailsOperation(
    const base::FilePath& full_path,
    content::BrowserContext* browser_context,
    PrivetFileSystemAsyncOperationContainer* container,
    PrivetFileSystemAttributeCache* attribute_cache,
    const storage::AsyncFileUtil::GetFileInfoCallback& callback)
    : core_(full_path, browser_context, this),
      full_path_(full_path),
      container_(container),
      attribute_cache_(attribute_cache),
      callback_(callback) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
}

PrivetFileSystemDetailsOperation::~PrivetFileSystemDetailsOperation() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
}

void PrivetFileSystemDetailsOperation::Start() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  const base::File::Info* info = attribute_cache_->GetFileInfo(full_path_);
  if (info) {
    TriggerCallbackAndDestroy(base::File::FILE_OK, *info);
    return;
  }

  core_.Start();
}

void PrivetFileSystemDetailsOperation::PrivetFileSystemResolved(
    PrivetV1HTTPClient* http_client,
    const std::string& path) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  if (!http_client) {
    SignalError();
    return;
  }

  list_operation_ = http_client->CreateStorageListOperation(
      path,
      base::Bind(&PrivetFileSystemDetailsOperation::OnStorageListResult,
                 base::Unretained(this)));
  list_operation_->Start();
}

void PrivetFileSystemDetailsOperation::OnStorageListResult(
    const base::DictionaryValue* value) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  list_operation_.reset();

  attribute_cache_->AddFileInfoFromJSON(full_path_, value);
  const base::File::Info* info = attribute_cache_->GetFileInfo(full_path_);

  if (info) {
    TriggerCallbackAndDestroy(base::File::FILE_OK, *info);
  } else {
    SignalError();
  }
}

void PrivetFileSystemDetailsOperation::SignalError() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  TriggerCallbackAndDestroy(base::File::FILE_ERROR_FAILED, base::File::Info());
}

void PrivetFileSystemDetailsOperation::TriggerCallbackAndDestroy(
    base::File::Error result,
    const base::File::Info& info) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  content::BrowserThread::PostTask(content::BrowserThread::IO,
                                   FROM_HERE,
                                   base::Bind(callback_, result, info));
  container_->RemoveOperation(this);
}

}  // namespace local_discovery
