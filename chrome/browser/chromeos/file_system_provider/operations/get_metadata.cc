// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/file_system_provider/operations/get_metadata.h"

#include <string>

#include "chrome/common/extensions/api/file_system_provider.h"
#include "chrome/common/extensions/api/file_system_provider_internal.h"

namespace chromeos {
namespace file_system_provider {
namespace operations {
namespace {

// Convert |value| into |output|. If parsing fails, then returns false.
bool ConvertRequestValueToFileInfo(scoped_ptr<RequestValue> value,
                                   base::File::Info* output) {
  using extensions::api::file_system_provider::EntryMetadata;
  using extensions::api::file_system_provider_internal::
      GetMetadataRequestedSuccess::Params;

  const Params* params = value->get_metadata_success_params();
  if (!params)
    return false;

  output->is_directory = params->metadata.is_directory;
  output->size = static_cast<int64>(params->metadata.size);
  output->is_symbolic_link = false;  // Not supported.

  std::string input_modification_time;
  if (!params->metadata.modification_time.additional_properties.GetString(
          "value", &input_modification_time)) {
    return false;
  }

  // Allow to pass invalid modification time, since there is no way to verify
  // it easily on any earlier stage.
  base::Time::FromString(
      input_modification_time.c_str(), &output->last_modified);

  return true;
}

}  // namespace

GetMetadata::GetMetadata(
    extensions::EventRouter* event_router,
    const ProvidedFileSystemInfo& file_system_info,
    const base::FilePath& directory_path,
    const fileapi::AsyncFileUtil::GetFileInfoCallback& callback)
    : Operation(event_router, file_system_info),
      directory_path_(directory_path),
      callback_(callback) {
}

GetMetadata::~GetMetadata() {
}

bool GetMetadata::Execute(int request_id) {
  scoped_ptr<base::ListValue> values(new base::ListValue);
  values->AppendString(directory_path_.AsUTF8Unsafe());
  return SendEvent(
      request_id,
      extensions::api::file_system_provider::OnGetMetadataRequested::kEventName,
      values.Pass());
}

void GetMetadata::OnSuccess(int /* request_id */,
                            scoped_ptr<RequestValue> result,
                            bool has_next) {
  base::File::Info file_info;
  const bool convert_result =
      ConvertRequestValueToFileInfo(result.Pass(), &file_info);
  DCHECK(convert_result);
  callback_.Run(base::File::FILE_OK, file_info);
}

void GetMetadata::OnError(int /* request_id */, base::File::Error error) {
  callback_.Run(error, base::File::Info());
}

}  // namespace operations
}  // namespace file_system_provider
}  // namespace chromeos
