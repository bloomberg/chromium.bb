// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/file_system_provider/operations/create_directory.h"

#include <string>

#include "chrome/common/extensions/api/file_system_provider.h"
#include "chrome/common/extensions/api/file_system_provider_internal.h"

namespace chromeos {
namespace file_system_provider {
namespace operations {

CreateDirectory::CreateDirectory(
    extensions::EventRouter* event_router,
    const ProvidedFileSystemInfo& file_system_info,
    const base::FilePath& directory_path,
    bool exclusive,
    bool recursive,
    const storage::AsyncFileUtil::StatusCallback& callback)
    : Operation(event_router, file_system_info),
      directory_path_(directory_path),
      exclusive_(exclusive),
      recursive_(recursive),
      callback_(callback) {
}

CreateDirectory::~CreateDirectory() {
}

bool CreateDirectory::Execute(int request_id) {
  if (!file_system_info_.writable())
    return false;

  scoped_ptr<base::DictionaryValue> values(new base::DictionaryValue);
  values->SetString("directoryPath", directory_path_.AsUTF8Unsafe());
  values->SetBoolean("exclusive", exclusive_);
  values->SetBoolean("recursive", recursive_);

  return SendEvent(request_id,
                   extensions::api::file_system_provider::
                       OnCreateDirectoryRequested::kEventName,
                   values.Pass());
}

void CreateDirectory::OnSuccess(int /* request_id */,
                                scoped_ptr<RequestValue> /* result */,
                                bool has_more) {
  callback_.Run(base::File::FILE_OK);
}

void CreateDirectory::OnError(int /* request_id */,
                              scoped_ptr<RequestValue> /* result */,
                              base::File::Error error) {
  callback_.Run(error);
}

}  // namespace operations
}  // namespace file_system_provider
}  // namespace chromeos
