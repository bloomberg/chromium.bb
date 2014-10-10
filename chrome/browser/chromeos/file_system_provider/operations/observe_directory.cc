// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/file_system_provider/operations/observe_directory.h"

#include <string>

#include "chrome/common/extensions/api/file_system_provider.h"
#include "chrome/common/extensions/api/file_system_provider_internal.h"

namespace chromeos {
namespace file_system_provider {
namespace operations {

ObserveDirectory::ObserveDirectory(
    extensions::EventRouter* event_router,
    const ProvidedFileSystemInfo& file_system_info,
    const base::FilePath& directory_path,
    bool recursive,
    const storage::AsyncFileUtil::StatusCallback& callback)
    : Operation(event_router, file_system_info),
      directory_path_(directory_path),
      recursive_(recursive),
      callback_(callback) {
}

ObserveDirectory::~ObserveDirectory() {
}

bool ObserveDirectory::Execute(int request_id) {
  using extensions::api::file_system_provider::ObserveDirectoryRequestedOptions;

  ObserveDirectoryRequestedOptions options;
  options.file_system_id = file_system_info_.file_system_id();
  options.request_id = request_id;
  options.directory_path = directory_path_.AsUTF8Unsafe();
  options.recursive = recursive_;

  return SendEvent(request_id,
                   extensions::api::file_system_provider::
                       OnObserveDirectoryRequested::kEventName,
                   extensions::api::file_system_provider::
                       OnObserveDirectoryRequested::Create(options));
}

void ObserveDirectory::OnSuccess(int /* request_id */,
                                 scoped_ptr<RequestValue> /* result */,
                                 bool has_more) {
  callback_.Run(base::File::FILE_OK);
}

void ObserveDirectory::OnError(int /* request_id */,
                               scoped_ptr<RequestValue> /* result */,
                               base::File::Error error) {
  callback_.Run(error);
}

}  // namespace operations
}  // namespace file_system_provider
}  // namespace chromeos
