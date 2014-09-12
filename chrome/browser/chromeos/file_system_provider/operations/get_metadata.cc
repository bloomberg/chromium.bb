// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/file_system_provider/operations/get_metadata.h"

#include <algorithm>
#include <string>

#include "chrome/common/extensions/api/file_system_provider.h"
#include "chrome/common/extensions/api/file_system_provider_internal.h"

namespace chromeos {
namespace file_system_provider {
namespace operations {
namespace {

// Convert |value| into |output|. If parsing fails, then returns false.
bool ConvertRequestValueToFileInfo(scoped_ptr<RequestValue> value,
                                   EntryMetadata* output) {
  using extensions::api::file_system_provider::EntryMetadata;
  using extensions::api::file_system_provider_internal::
      GetMetadataRequestedSuccess::Params;

  const Params* params = value->get_metadata_success_params();
  if (!params)
    return false;

  output->name = params->metadata.name;
  output->is_directory = params->metadata.is_directory;
  output->size = static_cast<int64>(params->metadata.size);

  std::string input_modification_time;
  if (!params->metadata.modification_time.additional_properties.GetString(
          "value", &input_modification_time)) {
    return false;
  }

  // Allow to pass invalid modification time, since there is no way to verify
  // it easily on any earlier stage.
  base::Time::FromString(input_modification_time.c_str(),
                         &output->modification_time);

  if (params->metadata.mime_type.get())
    output->mime_type = *params->metadata.mime_type.get();

  if (params->metadata.thumbnail.get()) {
    // Sanity check for the thumbnail format. Note, that another, more granural
    // check is done in custom bindings. Note, this is an extra check only for
    // the security reasons.
    const std::string expected_prefix = "data:";
    std::string thumbnail_prefix =
        params->metadata.thumbnail.get()->substr(0, expected_prefix.size());
    std::transform(thumbnail_prefix.begin(),
                   thumbnail_prefix.end(),
                   thumbnail_prefix.begin(),
                   ::tolower);

    if (expected_prefix != thumbnail_prefix)
      return false;

    output->thumbnail = *params->metadata.thumbnail.get();
  }

  return true;
}

}  // namespace

GetMetadata::GetMetadata(
    extensions::EventRouter* event_router,
    const ProvidedFileSystemInfo& file_system_info,
    const base::FilePath& entry_path,
    ProvidedFileSystemInterface::MetadataFieldMask fields,
    const ProvidedFileSystemInterface::GetMetadataCallback& callback)
    : Operation(event_router, file_system_info),
      entry_path_(entry_path),
      fields_(fields),
      callback_(callback) {
}

GetMetadata::~GetMetadata() {
}

bool GetMetadata::Execute(int request_id) {
  using extensions::api::file_system_provider::GetMetadataRequestedOptions;

  GetMetadataRequestedOptions options;
  options.file_system_id = file_system_info_.file_system_id();
  options.request_id = request_id;
  options.entry_path = entry_path_.AsUTF8Unsafe();
  options.thumbnail =
      fields_ & ProvidedFileSystemInterface::METADATA_FIELD_THUMBNAIL;

  return SendEvent(
      request_id,
      extensions::api::file_system_provider::OnGetMetadataRequested::kEventName,
      extensions::api::file_system_provider::OnGetMetadataRequested::Create(
          options));
}

void GetMetadata::OnSuccess(int /* request_id */,
                            scoped_ptr<RequestValue> result,
                            bool has_more) {
  scoped_ptr<EntryMetadata> metadata(new EntryMetadata);
  const bool convert_result =
      ConvertRequestValueToFileInfo(result.Pass(), metadata.get());

  if (!convert_result) {
    LOG(ERROR) << "Failed to parse a response for the get metadata operation.";
    callback_.Run(make_scoped_ptr<EntryMetadata>(NULL),
                  base::File::FILE_ERROR_IO);
    return;
  }

  callback_.Run(metadata.Pass(), base::File::FILE_OK);
}

void GetMetadata::OnError(int /* request_id */,
                          scoped_ptr<RequestValue> /* result */,
                          base::File::Error error) {
  callback_.Run(make_scoped_ptr<EntryMetadata>(NULL), error);
}
}  // namespace operations
}  // namespace file_system_provider
}  // namespace chromeos
