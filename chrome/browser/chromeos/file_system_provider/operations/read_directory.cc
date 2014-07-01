// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/file_system_provider/operations/read_directory.h"

#include <string>

#include "base/memory/linked_ptr.h"
#include "chrome/common/extensions/api/file_system_provider.h"
#include "chrome/common/extensions/api/file_system_provider_internal.h"

namespace chromeos {
namespace file_system_provider {
namespace operations {
namespace {

// Convert |input| into |output|. If parsing fails, then returns false.
bool ConvertRequestValueToEntryList(scoped_ptr<RequestValue> value,
                                    fileapi::AsyncFileUtil::EntryList* output) {
  using extensions::api::file_system_provider::EntryMetadata;
  using extensions::api::file_system_provider_internal::
      ReadDirectoryRequestedSuccess::Params;

  const Params* params = value->read_directory_success_params();
  if (!params)
    return false;

  for (size_t i = 0; i < params->entries.size(); ++i) {
    const linked_ptr<EntryMetadata> entry_metadata = params->entries[i];

    fileapi::DirectoryEntry output_entry;
    output_entry.is_directory = entry_metadata->is_directory;
    output_entry.name = entry_metadata->name;
    output_entry.size = static_cast<int64>(entry_metadata->size);

    std::string input_modification_time;
    if (!entry_metadata->modification_time.additional_properties.GetString(
            "value", &input_modification_time)) {
      return false;
    }
    if (!base::Time::FromString(input_modification_time.c_str(),
                                &output_entry.last_modified_time)) {
      return false;
    }

    output->push_back(output_entry);
  }

  return true;
}

}  // namespace

ReadDirectory::ReadDirectory(
    extensions::EventRouter* event_router,
    const ProvidedFileSystemInfo& file_system_info,
    const base::FilePath& directory_path,
    const fileapi::AsyncFileUtil::ReadDirectoryCallback& callback)
    : Operation(event_router, file_system_info),
      directory_path_(directory_path),
      callback_(callback) {
}

ReadDirectory::~ReadDirectory() {
}

bool ReadDirectory::Execute(int request_id) {
  scoped_ptr<base::DictionaryValue> values(new base::DictionaryValue);
  values->SetString("directoryPath", directory_path_.AsUTF8Unsafe());
  return SendEvent(request_id,
                   extensions::api::file_system_provider::
                       OnReadDirectoryRequested::kEventName,
                   values.Pass());
}

void ReadDirectory::OnSuccess(int /* request_id */,
                              scoped_ptr<RequestValue> result,
                              bool has_more) {
  fileapi::AsyncFileUtil::EntryList entry_list;
  const bool convert_result =
      ConvertRequestValueToEntryList(result.Pass(), &entry_list);

  if (!convert_result) {
    LOG(ERROR)
        << "Failed to parse a response for the read directory operation.";
    callback_.Run(base::File::FILE_ERROR_IO,
                  fileapi::AsyncFileUtil::EntryList(),
                  false /* has_more */);
    return;
  }

  callback_.Run(base::File::FILE_OK, entry_list, has_more);
}

void ReadDirectory::OnError(int /* request_id */,
                            scoped_ptr<RequestValue> /* result */,
                            base::File::Error error) {
  callback_.Run(
      error, fileapi::AsyncFileUtil::EntryList(), false /* has_more */);
}

}  // namespace operations
}  // namespace file_system_provider
}  // namespace chromeos
