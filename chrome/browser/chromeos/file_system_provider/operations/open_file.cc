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

OpenFile::OpenFile(
    extensions::EventRouter* event_router,
    const ProvidedFileSystemInfo& file_system_info,
    const base::FilePath& file_path,
    ProvidedFileSystemInterface::OpenFileMode mode,
    const ProvidedFileSystemInterface::OpenFileCallback& callback)
    : Operation(event_router, file_system_info),
      file_path_(file_path),
      mode_(mode),
      callback_(callback) {
}

OpenFile::~OpenFile() {
}

bool OpenFile::Execute(int request_id) {
  scoped_ptr<base::DictionaryValue> values(new base::DictionaryValue);
  values->SetString("filePath", file_path_.AsUTF8Unsafe());

  switch (mode_) {
    case ProvidedFileSystemInterface::OPEN_FILE_MODE_READ:
      values->SetString(
          "mode",
          extensions::api::file_system_provider::ToString(
              extensions::api::file_system_provider::OPEN_FILE_MODE_READ));
      break;
    case ProvidedFileSystemInterface::OPEN_FILE_MODE_WRITE:
      values->SetString(
          "mode",
          extensions::api::file_system_provider::ToString(
              extensions::api::file_system_provider::OPEN_FILE_MODE_WRITE));
      break;
  }

  return SendEvent(
      request_id,
      extensions::api::file_system_provider::OnOpenFileRequested::kEventName,
      values.Pass());
}

void OpenFile::OnSuccess(int request_id,
                         scoped_ptr<RequestValue> result,
                         bool has_more) {
  // File handle is the same as request id of the OpenFile operation.
  callback_.Run(request_id, base::File::FILE_OK);
}

void OpenFile::OnError(int /* request_id */,
                       scoped_ptr<RequestValue> /* result */,
                       base::File::Error error) {
  callback_.Run(0 /* file_handle */, error);
}

}  // namespace operations
}  // namespace file_system_provider
}  // namespace chromeos
