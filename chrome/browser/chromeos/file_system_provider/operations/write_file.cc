// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/file_system_provider/operations/write_file.h"

#include "base/debug/trace_event.h"
#include "base/values.h"
#include "chrome/common/extensions/api/file_system_provider.h"
#include "chrome/common/extensions/api/file_system_provider_internal.h"

namespace chromeos {
namespace file_system_provider {
namespace operations {

WriteFile::WriteFile(extensions::EventRouter* event_router,
                     const ProvidedFileSystemInfo& file_system_info,
                     int file_handle,
                     scoped_refptr<net::IOBuffer> buffer,
                     int64 offset,
                     int length,
                     const storage::AsyncFileUtil::StatusCallback& callback)
    : Operation(event_router, file_system_info),
      file_handle_(file_handle),
      buffer_(buffer),
      offset_(offset),
      length_(length),
      callback_(callback) {
}

WriteFile::~WriteFile() {
}

bool WriteFile::Execute(int request_id) {
  TRACE_EVENT0("file_system_provider", "WriteFile::Execute");

  if (!file_system_info_.writable())
    return false;

  scoped_ptr<base::DictionaryValue> values(new base::DictionaryValue);
  values->SetInteger("openRequestId", file_handle_);
  values->SetDouble("offset", offset_);
  values->SetInteger("length", length_);

  DCHECK(buffer_.get());
  values->Set(
      "data",
      base::BinaryValue::CreateWithCopiedBuffer(buffer_->data(), length_));

  return SendEvent(
      request_id,
      extensions::api::file_system_provider::OnWriteFileRequested::kEventName,
      values.Pass());
}

void WriteFile::OnSuccess(int /* request_id */,
                          scoped_ptr<RequestValue> /* result */,
                          bool /* has_more */) {
  TRACE_EVENT0("file_system_provider", "WriteFile::OnSuccess");
  callback_.Run(base::File::FILE_OK);
}

void WriteFile::OnError(int /* request_id */,
                        scoped_ptr<RequestValue> /* result */,
                        base::File::Error error) {
  TRACE_EVENT0("file_system_provider", "WriteFile::OnError");
  callback_.Run(error);
}

}  // namespace operations
}  // namespace file_system_provider
}  // namespace chromeos
