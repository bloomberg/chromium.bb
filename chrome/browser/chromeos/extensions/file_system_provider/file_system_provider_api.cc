// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/file_system_provider/file_system_provider_api.h"

#include <string>
#include <vector>

#include "base/memory/linked_ptr.h"
#include "base/memory/scoped_ptr.h"
#include "base/trace_event/trace_event.h"
#include "base/values.h"
#include "chrome/browser/chromeos/file_system_provider/provided_file_system_interface.h"
#include "chrome/browser/chromeos/file_system_provider/request_manager.h"
#include "chrome/browser/chromeos/file_system_provider/request_value.h"
#include "chrome/browser/chromeos/file_system_provider/service.h"
#include "chrome/common/extensions/api/file_system_provider.h"
#include "chrome/common/extensions/api/file_system_provider_internal.h"
#include "storage/browser/fileapi/watcher_manager.h"

using chromeos::file_system_provider::MountOptions;
using chromeos::file_system_provider::OpenedFiles;
using chromeos::file_system_provider::ProvidedFileSystemInfo;
using chromeos::file_system_provider::ProvidedFileSystemInterface;
using chromeos::file_system_provider::ProvidedFileSystemObserver;
using chromeos::file_system_provider::RequestValue;
using chromeos::file_system_provider::Service;
using chromeos::file_system_provider::Watchers;

namespace extensions {
namespace {

typedef std::vector<linked_ptr<api::file_system_provider::Change>> IDLChanges;

// Converts the change type from the IDL type to a native type. |changed_type|
// must be specified (not CHANGE_TYPE_NONE).
storage::WatcherManager::ChangeType ParseChangeType(
    const api::file_system_provider::ChangeType& change_type) {
  switch (change_type) {
    case api::file_system_provider::CHANGE_TYPE_CHANGED:
      return storage::WatcherManager::CHANGED;
    case api::file_system_provider::CHANGE_TYPE_DELETED:
      return storage::WatcherManager::DELETED;
    default:
      break;
  }
  NOTREACHED();
  return storage::WatcherManager::CHANGED;
}

// Convert the change from the IDL type to a native type. The reason IDL types
// are not used is since they are imperfect, eg. paths are stored as strings.
ProvidedFileSystemObserver::Change ParseChange(
    const api::file_system_provider::Change& change) {
  ProvidedFileSystemObserver::Change result;
  result.entry_path = base::FilePath::FromUTF8Unsafe(change.entry_path);
  result.change_type = ParseChangeType(change.change_type);
  return result;
}

// Converts a list of child changes from the IDL type to a native type.
scoped_ptr<ProvidedFileSystemObserver::Changes> ParseChanges(
    const IDLChanges& changes) {
  scoped_ptr<ProvidedFileSystemObserver::Changes> results(
      new ProvidedFileSystemObserver::Changes);
  for (const auto& change : changes) {
    results->push_back(ParseChange(*change));
  }
  return results;
}

// Fills the IDL's FileSystemInfo with FSP's ProvidedFileSystemInfo and
// Watchers.
void FillFileSystemInfo(const ProvidedFileSystemInfo& file_system_info,
                        const Watchers& watchers,
                        const OpenedFiles& opened_files,
                        api::file_system_provider::FileSystemInfo* output) {
  using api::file_system_provider::Watcher;
  using api::file_system_provider::OpenedFile;

  output->file_system_id = file_system_info.file_system_id();
  output->display_name = file_system_info.display_name();
  output->writable = file_system_info.writable();
  output->opened_files_limit = file_system_info.opened_files_limit();

  std::vector<linked_ptr<Watcher>> watcher_items;
  for (const auto& watcher : watchers) {
    const linked_ptr<Watcher> watcher_item(new Watcher);
    watcher_item->entry_path = watcher.second.entry_path.value();
    watcher_item->recursive = watcher.second.recursive;
    if (!watcher.second.last_tag.empty())
      watcher_item->last_tag.reset(new std::string(watcher.second.last_tag));
    watcher_items.push_back(watcher_item);
  }
  output->watchers = watcher_items;

  std::vector<linked_ptr<OpenedFile>> opened_file_items;
  for (const auto& opened_file : opened_files) {
    const linked_ptr<OpenedFile> opened_file_item(new OpenedFile);
    opened_file_item->open_request_id = opened_file.first;
    opened_file_item->file_path = opened_file.second.file_path.value();
    switch (opened_file.second.mode) {
      case chromeos::file_system_provider::OPEN_FILE_MODE_READ:
        opened_file_item->mode =
            extensions::api::file_system_provider::OPEN_FILE_MODE_READ;
        break;
      case chromeos::file_system_provider::OPEN_FILE_MODE_WRITE:
        opened_file_item->mode =
            extensions::api::file_system_provider::OPEN_FILE_MODE_WRITE;
        break;
    }
    opened_file_items.push_back(opened_file_item);
  }
  output->opened_files = opened_file_items;
}

}  // namespace

bool FileSystemProviderMountFunction::RunSync() {
  using api::file_system_provider::Mount::Params;
  const scoped_ptr<Params> params(Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  // It's an error if the file system Id is empty.
  if (params->options.file_system_id.empty()) {
    SetError(FileErrorToString(base::File::FILE_ERROR_INVALID_OPERATION));
    return false;
  }

  // It's an error if the display name is empty.
  if (params->options.display_name.empty()) {
    SetError(FileErrorToString(base::File::FILE_ERROR_INVALID_OPERATION));
    return false;
  }

  // If the opened files limit is set, then it must be larger or equal than 0.
  if (params->options.opened_files_limit.get() &&
      *params->options.opened_files_limit.get() < 0) {
    SetError(FileErrorToString(base::File::FILE_ERROR_INVALID_OPERATION));
    return false;
  }

  Service* const service = Service::Get(GetProfile());
  DCHECK(service);

  MountOptions options;
  options.file_system_id = params->options.file_system_id;
  options.display_name = params->options.display_name;
  options.writable = params->options.writable;
  options.opened_files_limit = params->options.opened_files_limit.get()
                                   ? *params->options.opened_files_limit.get()
                                   : 0;
  options.supports_notify_tag = params->options.supports_notify_tag;

  const base::File::Error result =
      service->MountFileSystem(extension_id(), options);
  if (result != base::File::FILE_OK) {
    SetError(FileErrorToString(result));
    return false;
  }

  return true;
}

bool FileSystemProviderUnmountFunction::RunSync() {
  using api::file_system_provider::Unmount::Params;
  scoped_ptr<Params> params(Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  Service* const service = Service::Get(GetProfile());
  DCHECK(service);

  const base::File::Error result =
      service->UnmountFileSystem(extension_id(), params->options.file_system_id,
                                 Service::UNMOUNT_REASON_USER);
  if (result != base::File::FILE_OK) {
    SetError(FileErrorToString(result));
    return false;
  }

  return true;
}

bool FileSystemProviderGetAllFunction::RunSync() {
  using api::file_system_provider::FileSystemInfo;
  Service* const service = Service::Get(GetProfile());
  DCHECK(service);

  const std::vector<ProvidedFileSystemInfo> file_systems =
      service->GetProvidedFileSystemInfoList();
  std::vector<linked_ptr<FileSystemInfo>> items;

  for (const auto& file_system_info : file_systems) {
    if (file_system_info.extension_id() == extension_id()) {
      const linked_ptr<FileSystemInfo> item(new FileSystemInfo);

      chromeos::file_system_provider::ProvidedFileSystemInterface* const
          file_system =
              service->GetProvidedFileSystem(file_system_info.extension_id(),
                                             file_system_info.file_system_id());
      DCHECK(file_system);

      FillFileSystemInfo(file_system_info, *file_system->GetWatchers(),
                         file_system->GetOpenedFiles(), item.get());
      items.push_back(item);
    }
  }

  SetResultList(api::file_system_provider::GetAll::Results::Create(items));
  return true;
}

bool FileSystemProviderGetFunction::RunSync() {
  using api::file_system_provider::Get::Params;
  scoped_ptr<Params> params(Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  using api::file_system_provider::FileSystemInfo;
  Service* const service = Service::Get(GetProfile());
  DCHECK(service);

  chromeos::file_system_provider::ProvidedFileSystemInterface* const
      file_system = service->GetProvidedFileSystem(extension_id(),
                                                   params->file_system_id);

  if (!file_system) {
    SetError(FileErrorToString(base::File::FILE_ERROR_NOT_FOUND));
    return false;
  }

  FileSystemInfo file_system_info;
  FillFileSystemInfo(file_system->GetFileSystemInfo(),
                     *file_system->GetWatchers(), file_system->GetOpenedFiles(),
                     &file_system_info);
  SetResultList(
      api::file_system_provider::Get::Results::Create(file_system_info));
  return true;
}

bool FileSystemProviderNotifyFunction::RunAsync() {
  using api::file_system_provider::Notify::Params;
  scoped_ptr<Params> params(Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  Service* const service = Service::Get(GetProfile());
  DCHECK(service);

  ProvidedFileSystemInterface* const file_system =
      service->GetProvidedFileSystem(extension_id(),
                                     params->options.file_system_id);
  if (!file_system) {
    SetError(FileErrorToString(base::File::FILE_ERROR_NOT_FOUND));
    return false;
  }

  file_system->Notify(
      base::FilePath::FromUTF8Unsafe(params->options.observed_path),
      params->options.recursive, ParseChangeType(params->options.change_type),
      params->options.changes.get()
          ? ParseChanges(*params->options.changes.get())
          : make_scoped_ptr(new ProvidedFileSystemObserver::Changes),
      params->options.tag.get() ? *params->options.tag.get() : "",
      base::Bind(&FileSystemProviderNotifyFunction::OnNotifyCompleted, this));

  return true;
}

void FileSystemProviderNotifyFunction::OnNotifyCompleted(
    base::File::Error result) {
  if (result != base::File::FILE_OK) {
    SetError(FileErrorToString(result));
    SendResponse(false);
    return;
  }

  SendResponse(true);
}

bool FileSystemProviderInternalUnmountRequestedSuccessFunction::RunWhenValid() {
  using api::file_system_provider_internal::UnmountRequestedSuccess::Params;
  scoped_ptr<Params> params(Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  return FulfillRequest(RequestValue::CreateForUnmountSuccess(params.Pass()),
                        false /* has_more */);
}

bool
FileSystemProviderInternalGetMetadataRequestedSuccessFunction::RunWhenValid() {
  using api::file_system_provider_internal::GetMetadataRequestedSuccess::Params;
  scoped_ptr<Params> params(Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  return FulfillRequest(
      RequestValue::CreateForGetMetadataSuccess(params.Pass()),
      false /* has_more */);
}

bool FileSystemProviderInternalReadDirectoryRequestedSuccessFunction::
    RunWhenValid() {
  using api::file_system_provider_internal::ReadDirectoryRequestedSuccess::
      Params;
  scoped_ptr<Params> params(Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  const bool has_more = params->has_more;
  return FulfillRequest(
      RequestValue::CreateForReadDirectorySuccess(params.Pass()), has_more);
}

bool
FileSystemProviderInternalReadFileRequestedSuccessFunction::RunWhenValid() {
  TRACE_EVENT0("file_system_provider", "ReadFileRequestedSuccess");
  using api::file_system_provider_internal::ReadFileRequestedSuccess::Params;

  scoped_ptr<Params> params(Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  const bool has_more = params->has_more;
  return FulfillRequest(RequestValue::CreateForReadFileSuccess(params.Pass()),
                        has_more);
}

bool
FileSystemProviderInternalOperationRequestedSuccessFunction::RunWhenValid() {
  using api::file_system_provider_internal::OperationRequestedSuccess::Params;
  scoped_ptr<Params> params(Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  return FulfillRequest(
      scoped_ptr<RequestValue>(
          RequestValue::CreateForOperationSuccess(params.Pass())),
      false /* has_more */);
}

bool FileSystemProviderInternalOperationRequestedErrorFunction::RunWhenValid() {
  using api::file_system_provider_internal::OperationRequestedError::Params;
  scoped_ptr<Params> params(Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  const base::File::Error error = ProviderErrorToFileError(params->error);
  return RejectRequest(RequestValue::CreateForOperationError(params.Pass()),
                       error);
}

}  // namespace extensions
