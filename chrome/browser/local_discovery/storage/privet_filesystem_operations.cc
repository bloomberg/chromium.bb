// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/local_discovery/storage/privet_filesystem_operations.h"

namespace local_discovery {

namespace {
const char kPrivetListEntries[] = "entries";
const char kPrivetListKeyName[] = "name";
const char kPrivetListKeySize[] = "size";
const char kPrivetListKeyType[] = "type";
const char kPrivetListTypeDir[] = "dir";
}

PrivetFileSystemAsyncOperationUtil::PrivetFileSystemAsyncOperationUtil(
    const base::FilePath& full_path,
    net::URLRequestContextGetter* request_context,
    Delegate* delegate)
    : parsed_path_(full_path), request_context_(request_context),
      delegate_(delegate) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
}

PrivetFileSystemAsyncOperationUtil::~PrivetFileSystemAsyncOperationUtil() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
}

void PrivetFileSystemAsyncOperationUtil::Start() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
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
    delegate_->PrivetFileSystemResolved(NULL,
                                       parsed_path_.path);
    return;
  }

  privet_async_factory_ = PrivetHTTPAsynchronousFactory::CreateInstance(
      service_discovery_client_.get(),
      request_context_.get());
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
  privet_client_ = privet_http_client.Pass();
  delegate_->PrivetFileSystemResolved(privet_client_.get(),
                                      parsed_path_.path);
}


PrivetFileSystemListOperation::PrivetFileSystemListOperation(
    const base::FilePath& full_path,
    content::BrowserContext* browser_context,
    PrivetFileSystemAsyncOperationContainer* async_file_util,
    const fileapi::AsyncFileUtil::ReadDirectoryCallback& callback)
    : full_path_(full_path), browser_context_(browser_context),
      async_file_util_(async_file_util),
      callback_(callback), weak_factory_(this) {
}

PrivetFileSystemListOperation::~PrivetFileSystemListOperation() {
}

void PrivetFileSystemListOperation::Start() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));
  content::BrowserThread::PostTask(
      content::BrowserThread::UI,
      FROM_HERE,
      base::Bind(
          &PrivetFileSystemListOperation::StartOnUIThread,
          weak_factory_.GetWeakPtr()));
}

void PrivetFileSystemListOperation::StartOnUIThread() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  core_.reset(new PrivetFileSystemAsyncOperationUtil(
      full_path_,
      browser_context_->GetRequestContext(),
      this));
  core_->Start();
}

void PrivetFileSystemListOperation::PrivetFileSystemResolved(
    PrivetHTTPClient* http_client,
    const std::string& path) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
    if (!http_client) {
      TriggerCallback(base::File::FILE_ERROR_FAILED,
                      fileapi::AsyncFileUtil::EntryList(), false);
      return;
    }

    list_operation_ = http_client->CreateStorageListOperation(
        path,
        base::Bind(&PrivetFileSystemListOperation::OnStorageListResult,
                   base::Unretained(this)));
    list_operation_->Start();
}

void PrivetFileSystemListOperation::TriggerCallback(
    base::File::Error result,
    const fileapi::AsyncFileUtil::EntryList& file_list,
    bool has_more) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  list_operation_.reset();
  core_.reset();
  content::BrowserThread::PostTask(
      content::BrowserThread::IO,
      FROM_HERE,
      base::Bind(
          &PrivetFileSystemListOperation::TriggerCallbackOnIOThread,
          weak_factory_.GetWeakPtr(),
          result, file_list, has_more));
}

void PrivetFileSystemListOperation::TriggerCallbackOnIOThread(
    base::File::Error result,
    fileapi::AsyncFileUtil::EntryList file_list,
    bool has_more) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));
  fileapi::AsyncFileUtil::ReadDirectoryCallback callback;
  callback = callback_;
  async_file_util_->RemoveOperation(this);
  callback.Run(result, file_list, has_more);
}

void PrivetFileSystemListOperation::OnStorageListResult(
    const base::DictionaryValue* value) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  fileapi::AsyncFileUtil::EntryList entry_list;

  if (!value) {
    TriggerCallback(base::File::FILE_ERROR_FAILED,
                    fileapi::AsyncFileUtil::EntryList(), false);
    return;
  }

  const base::ListValue* entries;
  if (!value->GetList(kPrivetListEntries, &entries)) {
    TriggerCallback(base::File::FILE_ERROR_FAILED,
                    fileapi::AsyncFileUtil::EntryList(), false);
    return;
  }

  for (size_t i = 0; i < entries->GetSize(); i++) {
    const base::DictionaryValue* entry_value;
    if (!entries->GetDictionary(i, &entry_value)) {
      TriggerCallback(base::File::FILE_ERROR_FAILED,
                      fileapi::AsyncFileUtil::EntryList(), false);
      return;
    }

    std::string name;
    std::string type;
    int size = 0;

    entry_value->GetString(kPrivetListKeyName, &name);
    entry_value->GetString(kPrivetListKeyType, &type);
    entry_value->GetInteger(kPrivetListKeySize, &size);

    fileapi::DirectoryEntry entry(
        name,
        (type == kPrivetListTypeDir) ?
        fileapi::DirectoryEntry::DIRECTORY : fileapi::DirectoryEntry::FILE,
        size,
        base::Time() /* TODO(noamsml) */);

    entry_list.push_back(entry);
  }

  TriggerCallback(base::File::FILE_OK, entry_list, false);
}


}  // namespace local_discovery
