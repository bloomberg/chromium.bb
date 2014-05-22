// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/file_system_provider/file_system_provider_api.h"

#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "chrome/browser/chromeos/file_system_provider/provided_file_system_interface.h"
#include "chrome/browser/chromeos/file_system_provider/request_manager.h"
#include "chrome/browser/chromeos/file_system_provider/request_value.h"
#include "chrome/browser/chromeos/file_system_provider/service.h"
#include "chrome/common/extensions/api/file_system_provider.h"
#include "chrome/common/extensions/api/file_system_provider_internal.h"

using chromeos::file_system_provider::ProvidedFileSystemInterface;
using chromeos::file_system_provider::RequestValue;
using chromeos::file_system_provider::Service;

namespace extensions {

bool FileSystemProviderMountFunction::RunSync() {
  using api::file_system_provider::Mount::Params;
  const scoped_ptr<Params> params(Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  // It's an error if the file system Id is empty.
  if (params->file_system_id.empty()) {
    base::ListValue* result = new base::ListValue();
    result->Append(CreateError(kSecurityErrorName, kEmptyIdErrorMessage));
    SetResult(result);
    return true;
  }

  // It's an error if the display name is empty.
  if (params->display_name.empty()) {
    base::ListValue* result = new base::ListValue();
    result->Append(CreateError(kSecurityErrorName,
                               kEmptyNameErrorMessage));
    SetResult(result);
    return true;
  }

  Service* service = Service::Get(GetProfile());
  DCHECK(service);
  if (!service)
    return false;

  // TODO(mtomasz): Pass more detailed errors, rather than just a bool.
  if (!service->MountFileSystem(
          extension_id(), params->file_system_id, params->display_name)) {
    base::ListValue* result = new base::ListValue();
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

  Service* service = Service::Get(GetProfile());
  DCHECK(service);
  if (!service)
    return false;

  if (!service->UnmountFileSystem(extension_id(), params->file_system_id)) {
    // TODO(mtomasz): Pass more detailed errors, rather than just a bool.
    base::ListValue* result = new base::ListValue();
    result->Append(CreateError(kSecurityErrorName, kUnmountFailedErrorMessage));
    SetResult(result);
    return true;
  }

  base::ListValue* result = new base::ListValue();
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

bool FileSystemProviderInternalUnmountRequestedErrorFunction::RunWhenValid() {
  using api::file_system_provider_internal::UnmountRequestedError::Params;
  const scoped_ptr<Params> params(Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  RejectRequest(ProviderErrorToFileError(params->error));
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

bool
FileSystemProviderInternalGetMetadataRequestedErrorFunction::RunWhenValid() {
  using api::file_system_provider_internal::GetMetadataRequestedError::Params;
  const scoped_ptr<Params> params(Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  RejectRequest(ProviderErrorToFileError(params->error));
  return true;
}

bool FileSystemProviderInternalReadDirectoryRequestedSuccessFunction::
    RunWhenValid() {
  using api::file_system_provider_internal::ReadDirectoryRequestedSuccess::
      Params;
  scoped_ptr<Params> params(Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  const bool has_next = params->has_next;
  FulfillRequest(RequestValue::CreateForReadDirectorySuccess(params.Pass()),
                 has_next);
  return true;
}

bool
FileSystemProviderInternalReadDirectoryRequestedErrorFunction::RunWhenValid() {
  using api::file_system_provider_internal::ReadDirectoryRequestedError::Params;
  const scoped_ptr<Params> params(Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  RejectRequest(ProviderErrorToFileError(params->error));
  return true;
}

bool
FileSystemProviderInternalOpenFileRequestedSuccessFunction::RunWhenValid() {
  using api::file_system_provider_internal::OpenFileRequestedSuccess::Params;
  scoped_ptr<Params> params(Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  FulfillRequest(scoped_ptr<RequestValue>(new RequestValue()),
                 false /* has_more */);
  return true;
}

bool FileSystemProviderInternalOpenFileRequestedErrorFunction::RunWhenValid() {
  using api::file_system_provider_internal::OpenFileRequestedError::Params;
  const scoped_ptr<Params> params(Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  RejectRequest(ProviderErrorToFileError(params->error));
  return true;
}

bool
FileSystemProviderInternalCloseFileRequestedSuccessFunction::RunWhenValid() {
  using api::file_system_provider_internal::CloseFileRequestedSuccess::Params;
  scoped_ptr<Params> params(Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  FulfillRequest(scoped_ptr<RequestValue>(new RequestValue()),
                 false /* has_more */);
  return true;
}

bool FileSystemProviderInternalCloseFileRequestedErrorFunction::RunWhenValid() {
  using api::file_system_provider_internal::CloseFileRequestedError::Params;
  const scoped_ptr<Params> params(Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  RejectRequest(ProviderErrorToFileError(params->error));
  return true;
}

bool
FileSystemProviderInternalReadFileRequestedSuccessFunction::RunWhenValid() {
  using api::file_system_provider_internal::ReadFileRequestedSuccess::Params;
  scoped_ptr<Params> params(Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  const bool has_next = params->has_next;
  FulfillRequest(RequestValue::CreateForReadFileSuccess(params.Pass()),
                 has_next);
  return true;
}

bool FileSystemProviderInternalReadFileRequestedErrorFunction::RunWhenValid() {
  using api::file_system_provider_internal::ReadFileRequestedError::Params;
  const scoped_ptr<Params> params(Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  RejectRequest(ProviderErrorToFileError(params->error));
  return true;
}

}  // namespace extensions
