// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/file_system_provider/operations/move_entry.h"

#include <string>

#include "chrome/common/extensions/api/file_system_provider.h"
#include "chrome/common/extensions/api/file_system_provider_internal.h"

namespace chromeos {
namespace file_system_provider {
namespace operations {

MoveEntry::MoveEntry(extensions::EventRouter* event_router,
                     const ProvidedFileSystemInfo& file_system_info,
                     const base::FilePath& source_path,
                     const base::FilePath& target_path,
                     const fileapi::AsyncFileUtil::StatusCallback& callback)
    : Operation(event_router, file_system_info),
      source_path_(source_path),
      target_path_(target_path),
      callback_(callback) {
}

MoveEntry::~MoveEntry() {
}

bool MoveEntry::Execute(int request_id) {
  if (!file_system_info_.writable())
    return false;

  scoped_ptr<base::DictionaryValue> values(new base::DictionaryValue);
  values->SetString("sourcePath", source_path_.AsUTF8Unsafe());
  values->SetString("targetPath", target_path_.AsUTF8Unsafe());

  return SendEvent(
      request_id,
      extensions::api::file_system_provider::OnMoveEntryRequested::kEventName,
      values.Pass());
}

void MoveEntry::OnSuccess(int /* request_id */,
                          scoped_ptr<RequestValue> /* result */,
                          bool has_more) {
  callback_.Run(base::File::FILE_OK);
}

void MoveEntry::OnError(int /* request_id */,
                        scoped_ptr<RequestValue> /* result */,
                        base::File::Error error) {
  callback_.Run(error);
}

}  // namespace operations
}  // namespace file_system_provider
}  // namespace chromeos
