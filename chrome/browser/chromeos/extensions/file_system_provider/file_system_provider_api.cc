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

using chromeos::file_system_provider::ProvidedFileSystemInfo;
using chromeos::file_system_provider::ProvidedFileSystemInterface;
using chromeos::file_system_provider::RequestValue;
using chromeos::file_system_provider::Service;

namespace extensions {

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

  // TODO(mtomasz): Pass more detailed errors, rather than just a bool.
  if (!service->MountFileSystem(extension_id(),
                                params->options.file_system_id,
                                params->options.display_name,
                                params->options.writable)) {
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
    linked_ptr<FileSystemInfo> item(new FileSystemInfo);
    item->file_system_id = file_systems[i].file_system_id();
    item->display_name = file_systems[i].display_name();
    item->writable = file_systems[i].writable();
    items.push_back(item);
  }

  SetResultList(api::file_system_provider::GetAll::Results::Create(items));
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
