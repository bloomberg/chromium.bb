// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/file_system_provider/operations/open_file.h"

#include <string>

#include "chrome/common/extensions/api/file_system_provider.h"
#include "chrome/common/extensions/api/file_system_provider_internal.h"

namespace chromeos {
namespace file_system_provider {
namespace operations {

OpenFile::OpenFile(extensions::EventRouter* event_router,
                   const ProvidedFileSystemInfo& file_system_info,
                   const base::FilePath& file_path,
                   ProvidedFileSystemInterface::OpenFileMode mode,
                   bool create,
                   const fileapi::AsyncFileUtil::StatusCallback& callback)
    : Operation(event_router, file_system_info),
      file_path_(file_path),
      mode_(mode),
      create_(create),
      callback_(callback) {
}

OpenFile::~OpenFile() {
}

bool OpenFile::Execute(int request_id) {
  scoped_ptr<base::ListValue> values(new base::ListValue);
  values->AppendString(file_path_.AsUTF8Unsafe());

  switch (mode_) {
    case ProvidedFileSystemInterface::OPEN_FILE_MODE_READ:
      values->AppendString(extensions::api::file_system_provider::ToString(
          extensions::api::file_system_provider::OPEN_FILE_MODE_READ));
      break;
    case ProvidedFileSystemInterface::OPEN_FILE_MODE_WRITE:
      values->AppendString(extensions::api::file_system_provider::ToString(
          extensions::api::file_system_provider::OPEN_FILE_MODE_WRITE));
      break;
  }

  values->AppendBoolean(create_);

  return SendEvent(
      request_id,
      extensions::api::file_system_provider::OnOpenFileRequested::kEventName,
      values.Pass());
}

void OpenFile::OnSuccess(int /* request_id */,
                         scoped_ptr<RequestValue> result,
                         bool has_next) {
  callback_.Run(base::File::FILE_OK);
}

void OpenFile::OnError(int /* request_id */, base::File::Error error) {
  callback_.Run(error);
}

}  // namespace operations
}  // namespace file_system_provider
}  // namespace chromeos
