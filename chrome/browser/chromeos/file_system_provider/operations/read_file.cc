// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/file_system_provider/operations/read_file.h"

#include <limits>
#include <string>

#include "chrome/common/extensions/api/file_system_provider.h"
#include "chrome/common/extensions/api/file_system_provider_internal.h"

namespace chromeos {
namespace file_system_provider {
namespace operations {
namespace {

// Convert |value| into |output|. If parsing fails, then returns a negative
// value. Otherwise returns number of bytes written to the buffer.
int CopyRequestValueToBuffer(scoped_ptr<RequestValue> value,
                             net::IOBuffer* buffer,
                             int buffer_offset,
                             int buffer_length) {
  using extensions::api::file_system_provider_internal::
      ReadFileRequestedSuccess::Params;

  const Params* params = value->read_file_success_params();
  if (!params)
    return -1;

  const size_t chunk_size = params->data.length();

  // Check for overflows.
  if (chunk_size > static_cast<size_t>(buffer_length) - buffer_offset)
    return -1;

  memcpy(buffer->data() + buffer_offset, params->data.c_str(), chunk_size);

  return chunk_size;
}

}  // namespace

ReadFile::ReadFile(
    extensions::EventRouter* event_router,
    const ProvidedFileSystemInfo& file_system_info,
    int file_handle,
    net::IOBuffer* buffer,
    int64 offset,
    int length,
    const ProvidedFileSystemInterface::ReadChunkReceivedCallback& callback)
    : Operation(event_router, file_system_info),
      file_handle_(file_handle),
      buffer_(buffer),
      offset_(offset),
      length_(length),
      current_offset_(offset),
      callback_(callback) {
}

ReadFile::~ReadFile() {
}

bool ReadFile::Execute(int request_id) {
  scoped_ptr<base::ListValue> values(new base::ListValue);
  values->AppendInteger(file_handle_);
  values->AppendDouble(offset_);
  values->AppendInteger(length_);
  return SendEvent(
      request_id,
      extensions::api::file_system_provider::OnReadFileRequested::kEventName,
      values.Pass());
}

void ReadFile::OnSuccess(int /* request_id */,
                         scoped_ptr<RequestValue> result,
                         bool has_next) {
  const int copy_result = CopyRequestValueToBuffer(
      result.Pass(), buffer_, current_offset_, length_);
  DCHECK_LE(0, copy_result);
  DCHECK(!has_next || copy_result > 0);
  if (copy_result > 0)
    current_offset_ += copy_result;
  callback_.Run(copy_result, has_next, base::File::FILE_OK);
}

void ReadFile::OnError(int /* request_id */, base::File::Error error) {
  callback_.Run(0 /* chunk_length */, false /* has_next */, error);
}

}  // namespace operations
}  // namespace file_system_provider
}  // namespace chromeos
