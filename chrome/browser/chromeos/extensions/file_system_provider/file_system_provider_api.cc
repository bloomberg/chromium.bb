// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/file_system_provider/file_system_provider_api.h"

#include <string>
#include <vector>

#include "base/debug/trace_event.h"
#include "base/memory/linked_ptr.h"
#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "chrome/browser/chromeos/file_system_provider/provided_file_system_interface.h"
#include "chrome/browser/chromeos/file_system_provider/request_manager.h"
#include "chrome/browser/chromeos/file_system_provider/request_value.h"
#include "chrome/browser/chromeos/file_system_provider/service.h"
#include "chrome/common/extensions/api/file_system_provider.h"
#include "chrome/common/extensions/api/file_system_provider_internal.h"
#include "storage/browser/fileapi/watcher_manager.h"

using chromeos::file_system_provider::MountOptions;
using chromeos::file_system_provider::ProvidedFileSystemInfo;
using chromeos::file_system_provider::ProvidedFileSystemInterface;
using chromeos::file_system_provider::ProvidedFileSystemObserver;
using chromeos::file_system_provider::RequestValue;
using chromeos::file_system_provider::Service;

namespace extensions {
namespace {

typedef std::vector<linked_ptr<api::file_system_provider::Change>> IDLChanges;

const char kNotifyFailedErrorMessage[] =
    "Sending a response for the request failed.";
const char kInvalidNotificationErrorMessage[] = "The notification is invalid.";

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

}  // namespace

bool FileSystemProviderMountFunction::RunSync() {
  using api::file_system_provider::Mount::Params;
  const scoped_ptr<Params> params(Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  // It's an error if the file system Id is empty.
  if (params->options.file_system_id.empty()) {
    base::ListValue* const result = new base::ListValue();
    result->Append(CreateError(kSecurityErrorName, kEmptyIdErrorMessage));
    SetResult(result);
    return true;
  }

  // It's an error if the display name is empty.
  if (params->options.display_name.empty()) {
    base::ListValue* const result = new base::ListValue();
    result->Append(CreateError(kSecurityErrorName,
                               kEmptyNameErrorMessage));
    SetResult(result);
    return true;
  }

  Service* const service = Service::Get(GetProfile());
  DCHECK(service);
  if (!service)
    return false;

  MountOptions options;
  options.file_system_id = params->options.file_system_id;
  options.display_name = params->options.display_name;
  options.writable = params->options.writable;
  options.supports_notify_tag = params->options.supports_notify_tag;

  // TODO(mtomasz): Pass more detailed errors, rather than just a bool.
  if (!service->MountFileSystem(extension_id(), options)) {
    base::ListValue* const result = new base::ListValue();
    result->Append(CreateError(kSecurityErrorName, kMountFailedErrorMessage));
    SetResult(result);
    return true;
  }

  base::ListValue* result = new base::ListValue();
  SetResult(result);
  return true;
}

bool FileSystemProviderUnmountFunction::RunSync() {
  using api::file_system_provider::Unmount::Params;
  scoped_ptr<Params> params(Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  Service* const service = Service::Get(GetProfile());
  DCHECK(service);
  if (!service)
    return false;

  if (!service->UnmountFileSystem(extension_id(),
                                  params->options.file_system_id,
                                  Service::UNMOUNT_REASON_USER)) {
    // TODO(mtomasz): Pass more detailed errors, rather than just a bool.
    base::ListValue* result = new base::ListValue();
    result->Append(CreateError(kSecurityErrorName, kUnmountFailedErrorMessage));
    SetResult(result);
    return true;
  }

  base::ListValue* const result = new base::ListValue();
  SetResult(result);
  return true;
}

bool FileSystemProviderGetAllFunction::RunSync() {
  using api::file_system_provider::FileSystemInfo;
  Service* const service = Service::Get(GetProfile());
  DCHECK(service);
  if (!service)
    return false;

  const std::vector<ProvidedFileSystemInfo> file_systems =
      service->GetProvidedFileSystemInfoList();
  std::vector<linked_ptr<FileSystemInfo> > items;

  for (size_t i = 0; i < file_systems.size(); ++i) {
    if (file_systems[i].extension_id() == extension_id()) {
      linked_ptr<FileSystemInfo> item(new FileSystemInfo);
      item->file_system_id = file_systems[i].file_system_id();
      item->display_name = file_systems[i].display_name();
      item->writable = file_systems[i].writable();
      items.push_back(item);
    }
  }

  SetResultList(api::file_system_provider::GetAll::Results::Create(items));
  return true;
}

bool FileSystemProviderNotifyFunction::RunSync() {
  using api::file_system_provider::Notify::Params;
  scoped_ptr<Params> params(Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  Service* const service = Service::Get(GetProfile());
  DCHECK(service);
  if (!service)
    return false;

  ProvidedFileSystemInterface* const file_system =
      service->GetProvidedFileSystem(extension_id(),
                                     params->options.file_system_id);
  if (!file_system) {
    base::ListValue* const result = new base::ListValue();
    result->Append(CreateError(kNotFoundErrorName, kNotifyFailedErrorMessage));
    SetResult(result);
    return true;
  }

  if (!file_system->Notify(
          base::FilePath::FromUTF8Unsafe(params->options.observed_path),
          params->options.recursive,
          ParseChangeType(params->options.change_type),
          params->options.changes.get()
              ? ParseChanges(*params->options.changes.get())
              : make_scoped_ptr(new ProvidedFileSystemObserver::Changes),
          params->options.tag.get() ? *params->options.tag.get() : "")) {
    base::ListValue* const result = new base::ListValue();
    result->Append(
        CreateError(kSecurityErrorName, kInvalidNotificationErrorMessage));
    SetResult(result);
    return true;
  }

  base::ListValue* const result = new base::ListValue();
  SetResult(result);
  return true;
}

bool FileSystemProviderInternalUnmountRequestedSuccessFunction::RunWhenValid() {
  using api::file_system_provider_internal::UnmountRequestedSuccess::Params;
  scoped_ptr<Params> params(Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  FulfillRequest(RequestValue::CreateForUnmountSuccess(params.Pass()),
                 false /* has_more */);
  return true;
}

bool
FileSystemProviderInternalGetMetadataRequestedSuccessFunction::RunWhenValid() {
  using api::file_system_provider_internal::GetMetadataRequestedSuccess::Params;
  scoped_ptr<Params> params(Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  FulfillRequest(RequestValue::CreateForGetMetadataSuccess(params.Pass()),
                 false /* has_more */);
  return true;
}

bool FileSystemProviderInternalReadDirectoryRequestedSuccessFunction::
    RunWhenValid() {
  using api::file_system_provider_internal::ReadDirectoryRequestedSuccess::
      Params;
  scoped_ptr<Params> params(Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  const bool has_more = params->has_more;
  FulfillRequest(RequestValue::CreateForReadDirectorySuccess(params.Pass()),
                 has_more);
  return true;
}

bool
FileSystemProviderInternalReadFileRequestedSuccessFunction::RunWhenValid() {
  TRACE_EVENT0("file_system_provider", "ReadFileRequestedSuccess");
  using api::file_system_provider_internal::ReadFileRequestedSuccess::Params;

  scoped_ptr<Params> params(Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  const bool has_more = params->has_more;
  FulfillRequest(RequestValue::CreateForReadFileSuccess(params.Pass()),
                 has_more);
  return true;
}

bool
FileSystemProviderInternalOperationRequestedSuccessFunction::RunWhenValid() {
  using api::file_system_provider_internal::OperationRequestedSuccess::Params;
  scoped_ptr<Params> params(Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  FulfillRequest(scoped_ptr<RequestValue>(
                     RequestValue::CreateForOperationSuccess(params.Pass())),
                 false /* has_more */);
  return true;
}

bool FileSystemProviderInternalOperationRequestedErrorFunction::RunWhenValid() {
  using api::file_system_provider_internal::OperationRequestedError::Params;
  scoped_ptr<Params> params(Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  const base::File::Error error = ProviderErrorToFileError(params->error);
  RejectRequest(RequestValue::CreateForOperationError(params.Pass()), error);
  return true;
}

}  // namespace extensions
