// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/file_system_provider/operations/delete_entry.h"

#include <string>

#include "chrome/common/extensions/api/file_system_provider.h"
#include "chrome/common/extensions/api/file_system_provider_internal.h"

namespace chromeos {
namespace file_system_provider {
namespace operations {

DeleteEntry::DeleteEntry(extensions::EventRouter* event_router,
                         const ProvidedFileSystemInfo& file_system_info,
                         const base::FilePath& entry_path,
                         bool recursive,
                         const fileapi::AsyncFileUtil::StatusCallback& callback)
    : Operation(event_router, file_system_info),
      entry_path_(entry_path),
      recursive_(recursive),
      callback_(callback) {
}

DeleteEntry::~DeleteEntry() {
}

bool DeleteEntry::Execute(int request_id) {
  scoped_ptr<base::DictionaryValue> values(new base::DictionaryValue);
  values->SetString("entryPath", entry_path_.AsUTF8Unsafe());
  values->SetBoolean("recursive", recursive_);

  return SendEvent(
      request_id,
      extensions::api::file_system_provider::OnDeleteEntryRequested::kEventName,
      values.Pass());
}

void DeleteEntry::OnSuccess(int /* request_id */,
                            scoped_ptr<RequestValue> /* result */,
                            bool has_more) {
  callback_.Run(base::File::FILE_OK);
}

void DeleteEntry::OnError(int /* request_id */,
                          scoped_ptr<RequestValue> /* result */,
                          base::File::Error error) {
  callback_.Run(error);
}

}  // namespace operations
}  // namespace file_system_provider
}  // namespace chromeos
