// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_FILE_SYSTEM_PROVIDER_REQUEST_VALUE_H_
#define CHROME_BROWSER_CHROMEOS_FILE_SYSTEM_PROVIDER_REQUEST_VALUE_H_

#include <string>

#include "base/memory/scoped_ptr.h"
#include "chrome/common/extensions/api/file_system_provider_internal.h"

namespace chromeos {
namespace file_system_provider {

// Holds a parsed value returned by a providing extension. Each accessor can
// return NULL in case the requested value type is not available. It is used
// to pass values of success callbacks.
class RequestValue {
 public:
  // Creates an empty value. Use static methods to create a value holding a
  // proper content.
  RequestValue();

  virtual ~RequestValue();

  static scoped_ptr<RequestValue> CreateForUnmountSuccess(
      scoped_ptr<extensions::api::file_system_provider_internal::
                     UnmountRequestedSuccess::Params> params);

  static scoped_ptr<RequestValue> CreateForGetMetadataSuccess(
      scoped_ptr<extensions::api::file_system_provider_internal::
                     GetMetadataRequestedSuccess::Params> params);

  static scoped_ptr<RequestValue> CreateForReadDirectorySuccess(
      scoped_ptr<extensions::api::file_system_provider_internal::
                     ReadDirectoryRequestedSuccess::Params> params);

  static scoped_ptr<RequestValue> CreateForReadFileSuccess(
      scoped_ptr<extensions::api::file_system_provider_internal::
                     ReadFileRequestedSuccess::Params> params);

  static scoped_ptr<RequestValue> CreateForOperationSuccess(
      scoped_ptr<extensions::api::file_system_provider_internal::
                     OperationRequestedSuccess::Params> params);

  static scoped_ptr<RequestValue> CreateForOperationError(
      scoped_ptr<extensions::api::file_system_provider_internal::
                     OperationRequestedError::Params> params);

  static scoped_ptr<RequestValue> CreateForTesting(const std::string& params);

  const extensions::api::file_system_provider_internal::
      UnmountRequestedSuccess::Params*
      unmount_success_params() const {
    return unmount_success_params_.get();
  }

  const extensions::api::file_system_provider_internal::
      GetMetadataRequestedSuccess::Params*
      get_metadata_success_params() const {
    return get_metadata_success_params_.get();
  }

  const extensions::api::file_system_provider_internal::
      ReadDirectoryRequestedSuccess::Params*
      read_directory_success_params() const {
    return read_directory_success_params_.get();
  }

  const extensions::api::file_system_provider_internal::
      ReadFileRequestedSuccess::Params*
      read_file_success_params() const {
    return read_file_success_params_.get();
  }

  const extensions::api::file_system_provider_internal::
      OperationRequestedSuccess::Params*
      operation_success_params() const {
    return operation_success_params_.get();
  }

  const extensions::api::file_system_provider_internal::
      OperationRequestedError::Params*
      operation_error_params() const {
    return operation_error_params_.get();
  }

  const std::string* testing_params() const { return testing_params_.get(); }

 private:
  scoped_ptr<extensions::api::file_system_provider_internal::
                 UnmountRequestedSuccess::Params> unmount_success_params_;
  scoped_ptr<extensions::api::file_system_provider_internal::
                 GetMetadataRequestedSuccess::Params>
      get_metadata_success_params_;
  scoped_ptr<extensions::api::file_system_provider_internal::
                 ReadDirectoryRequestedSuccess::Params>
      read_directory_success_params_;
  scoped_ptr<extensions::api::file_system_provider_internal::
                 ReadFileRequestedSuccess::Params> read_file_success_params_;
  scoped_ptr<extensions::api::file_system_provider_internal::
                 OperationRequestedSuccess::Params> operation_success_params_;
  scoped_ptr<extensions::api::file_system_provider_internal::
                 OperationRequestedError::Params> operation_error_params_;
  scoped_ptr<std::string> testing_params_;

  DISALLOW_COPY_AND_ASSIGN(RequestValue);
};

}  // namespace file_system_provider
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_FILE_SYSTEM_PROVIDER_REQUEST_VALUE_H_
