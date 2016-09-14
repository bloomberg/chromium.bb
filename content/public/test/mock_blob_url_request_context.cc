// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/mock_blob_url_request_context.h"

#include "base/memory/ptr_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "storage/browser/blob/blob_data_builder.h"
#include "storage/browser/blob/blob_storage_context.h"
#include "storage/browser/blob/blob_url_request_job.h"
#include "storage/browser/blob/blob_url_request_job_factory.h"

namespace content {

MockBlobURLRequestContext::MockBlobURLRequestContext(
    storage::FileSystemContext* file_system_context)
    : blob_storage_context_(new storage::BlobStorageContext) {
  // Job factory owns the protocol handler.
  job_factory_.SetProtocolHandler(
      "blob", base::MakeUnique<storage::BlobProtocolHandler>(
                  blob_storage_context_.get(), file_system_context,
                  base::ThreadTaskRunnerHandle::Get()));
  set_job_factory(&job_factory_);
}

MockBlobURLRequestContext::~MockBlobURLRequestContext() {
  AssertNoURLRequests();
}

ScopedTextBlob::ScopedTextBlob(
    const MockBlobURLRequestContext& request_context,
    const std::string& blob_id,
    const std::string& data)
    : blob_id_(blob_id),
      context_(request_context.blob_storage_context()) {
  DCHECK(context_);
  storage::BlobDataBuilder blob_builder(blob_id_);
  if (!data.empty())
    blob_builder.AppendData(data);
  handle_ = context_->AddFinishedBlob(&blob_builder);
}

ScopedTextBlob::~ScopedTextBlob() {
}

std::unique_ptr<storage::BlobDataHandle> ScopedTextBlob::GetBlobDataHandle() {
  return context_->GetBlobDataFromUUID(blob_id_);
}

}  // namespace content
