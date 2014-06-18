// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/file_system_provider/file_system_provider_api.h"

#include "chrome/browser/chromeos/file_system_provider/provided_file_system_interface.h"
#include "chrome/browser/chromeos/file_system_provider/request_manager.h"
#include "chrome/browser/chromeos/file_system_provider/request_value.h"
#include "chrome/browser/chromeos/file_system_provider/service.h"
#include "chrome/common/extensions/api/file_system_provider_internal.h"

using chromeos::file_system_provider::ProvidedFileSystemInterface;
using chromeos::file_system_provider::RequestManager;
using chromeos::file_system_provider::RequestValue;
using chromeos::file_system_provider::Service;

namespace extensions {

const char kNotFoundErrorName[] = "NotFoundError";
const char kSecurityErrorName[] = "SecurityError";

const char kEmptyNameErrorMessage[] = "Empty display name is not allowed.";
const char kEmptyIdErrorMessage[] = "Empty file system Id s not allowed.";
const char kMountFailedErrorMessage[] = "Mounting the file system failed.";
const char kUnmountFailedErrorMessage[] = "Unmounting the file system failed.";
const char kResponseFailedErrorMessage[] =
    "Sending a response for the request failed.";

base::DictionaryValue* CreateError(const std::string& name,
                                   const std::string& message) {
  base::DictionaryValue* error = new base::DictionaryValue();
  error->SetString("name", name);
  error->SetString("message", message);
  return error;
}

base::File::Error ProviderErrorToFileError(
    api::file_system_provider::ProviderError error) {
  switch (error) {
    case api::file_system_provider::PROVIDER_ERROR_OK:
      return base::File::FILE_OK;
    case api::file_system_provider::PROVIDER_ERROR_FAILED:
      return base::File::FILE_ERROR_FAILED;
    case api::file_system_provider::PROVIDER_ERROR_IN_USE:
      return base::File::FILE_ERROR_IN_USE;
    case api::file_system_provider::PROVIDER_ERROR_EXISTS:
      return base::File::FILE_ERROR_EXISTS;
    case api::file_system_provider::PROVIDER_ERROR_NOT_FOUND:
      return base::File::FILE_ERROR_NOT_FOUND;
    case api::file_system_provider::PROVIDER_ERROR_ACCESS_DENIED:
      return base::File::FILE_ERROR_ACCESS_DENIED;
    case api::file_system_provider::PROVIDER_ERROR_TOO_MANY_OPENED:
      return base::File::FILE_ERROR_TOO_MANY_OPENED;
    case api::file_system_provider::PROVIDER_ERROR_NO_MEMORY:
      return base::File::FILE_ERROR_NO_MEMORY;
    case api::file_system_provider::PROVIDER_ERROR_NO_SPACE:
      return base::File::FILE_ERROR_NO_SPACE;
    case api::file_system_provider::PROVIDER_ERROR_NOT_A_DIRECTORY:
      return base::File::FILE_ERROR_NOT_A_DIRECTORY;
    case api::file_system_provider::PROVIDER_ERROR_INVALID_OPERATION:
      return base::File::FILE_ERROR_INVALID_OPERATION;
    case api::file_system_provider::PROVIDER_ERROR_SECURITY:
      return base::File::FILE_ERROR_SECURITY;
    case api::file_system_provider::PROVIDER_ERROR_ABORT:
      return base::File::FILE_ERROR_ABORT;
    case api::file_system_provider::PROVIDER_ERROR_NOT_A_FILE:
      return base::File::FILE_ERROR_NOT_A_FILE;
    case api::file_system_provider::PROVIDER_ERROR_NOT_EMPTY:
      return base::File::FILE_ERROR_NOT_EMPTY;
    case api::file_system_provider::PROVIDER_ERROR_INVALID_URL:
      return base::File::FILE_ERROR_INVALID_URL;
    case api::file_system_provider::PROVIDER_ERROR_IO:
      return base::File::FILE_ERROR_IO;
    case api::file_system_provider::PROVIDER_ERROR_NONE:
      NOTREACHED();
  }
  return base::File::FILE_ERROR_FAILED;
}

FileSystemProviderInternalFunction::FileSystemProviderInternalFunction()
    : request_id_(0), request_manager_(NULL) {
}

void FileSystemProviderInternalFunction::RejectRequest(
    scoped_ptr<chromeos::file_system_provider::RequestValue> value,
    base::File::Error error) {
  if (!request_manager_->RejectRequest(request_id_, value.Pass(), error))
    SetErrorResponse(kSecurityErrorName, kResponseFailedErrorMessage);
}

void FileSystemProviderInternalFunction::FulfillRequest(
    scoped_ptr<RequestValue> value,
    bool has_more) {
  if (!request_manager_->FulfillRequest(request_id_, value.Pass(), has_more))
    SetErrorResponse(kSecurityErrorName, kResponseFailedErrorMessage);
}

bool FileSystemProviderInternalFunction::RunSync() {
  DCHECK(args_);
  if (!Parse())
    return true;

  SendResponse(RunWhenValid());
  return true;
}

bool FileSystemProviderInternalFunction::Parse() {
  std::string file_system_id;

  if (!args_->GetString(0, &file_system_id) ||
      !args_->GetInteger(1, &request_id_)) {
    bad_message_ = true;
    SendResponse(false);
    return false;
  }

  Service* service = Service::Get(GetProfile());
  if (!service) {
    SendResponse(false);
    return false;
  }

  ProvidedFileSystemInterface* file_system =
      service->GetProvidedFileSystem(extension_id(), file_system_id);
  if (!file_system) {
    SetErrorResponse(kNotFoundErrorName, kResponseFailedErrorMessage);
    SendResponse(true);
    return false;
  }

  request_manager_ = file_system->GetRequestManager();
  return true;
}

void FileSystemProviderInternalFunction::SetErrorResponse(
    const std::string& name,
    const std::string& message) {
  base::ListValue* result = new base::ListValue();
  result->Append(CreateError(name, message));
  SetResult(result);
}

}  // namespace extensions
