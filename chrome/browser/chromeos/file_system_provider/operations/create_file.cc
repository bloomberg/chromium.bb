// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/file_system_provider/operations/create_file.h"

#include <string>

#include "chrome/common/extensions/api/file_system_provider.h"
#include "chrome/common/extensions/api/file_system_provider_internal.h"

namespace chromeos {
namespace file_system_provider {
namespace operations {

CreateFile::CreateFile(extensions::EventRouter* event_router,
                       const ProvidedFileSystemInfo& file_system_info,
                       const base::FilePath& file_path,
                       const storage::AsyncFileUtil::StatusCallback& callback)
    : Operation(event_router, file_system_info),
      file_path_(file_path),
      callback_(callback) {
}

CreateFile::~CreateFile() {
}

bool CreateFile::Execute(int request_id) {
  using extensions::api::file_system_provider::CreateFileRequestedOptions;

  if (!file_system_info_.writable())
    return false;

  CreateFileRequestedOptions options;
  options.file_system_id = file_system_info_.file_system_id();
  options.request_id = request_id;
  options.file_path = file_path_.AsUTF8Unsafe();

  return SendEvent(
      request_id,
      extensions::api::file_system_provider::OnCreateFileRequested::kEventName,
      extensions::api::file_system_provider::OnCreateFileRequested::Create(
          options));
}

void CreateFile::OnSuccess(int /* request_id */,
                           scoped_ptr<RequestValue> /* result */,
                           bool has_more) {
  callback_.Run(base::File::FILE_OK);
}

void CreateFile::OnError(int /* request_id */,
                         scoped_ptr<RequestValue> /* result */,
                         base::File::Error error) {
  callback_.Run(error);
}

}  // namespace operations
}  // namespace file_system_provider
}  // namespace chromeos
