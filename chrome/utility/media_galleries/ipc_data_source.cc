// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/utility/media_galleries/ipc_data_source.h"

#include "base/threading/thread_task_runner_handle.h"
#include "content/public/utility/utility_thread.h"

namespace metadata {

IPCDataSource::IPCDataSource(
    extensions::mojom::MediaDataSourcePtr media_data_source,
    int64_t total_size)
    : media_data_source_(std::move(media_data_source)),
      total_size_(total_size),
      utility_task_runner_(base::ThreadTaskRunnerHandle::Get()) {
  data_source_thread_checker_.DetachFromThread();
}

IPCDataSource::~IPCDataSource() {
  DCHECK(utility_thread_checker_.CalledOnValidThread());
}

void IPCDataSource::Stop() {
  DCHECK(data_source_thread_checker_.CalledOnValidThread());
}

void IPCDataSource::Abort() {
  DCHECK(data_source_thread_checker_.CalledOnValidThread());
}

void IPCDataSource::Read(int64_t position,
                         int size,
                         uint8_t* destination,
                         const DataSource::ReadCB& callback) {
  DCHECK(data_source_thread_checker_.CalledOnValidThread());

  utility_task_runner_->PostTask(
      FROM_HERE, base::Bind(&IPCDataSource::ReadBlob, base::Unretained(this),
                            destination, callback, position, size));
}

bool IPCDataSource::GetSize(int64_t* size_out) {
  DCHECK(data_source_thread_checker_.CalledOnValidThread());
  *size_out = total_size_;
  return true;
}

bool IPCDataSource::IsStreaming() {
  DCHECK(data_source_thread_checker_.CalledOnValidThread());
  return false;
}

void IPCDataSource::SetBitrate(int bitrate) {
  DCHECK(data_source_thread_checker_.CalledOnValidThread());
}

void IPCDataSource::ReadBlob(uint8_t* destination,
                             const DataSource::ReadCB& callback,
                             int64_t position,
                             int size) {
  DCHECK(utility_thread_checker_.CalledOnValidThread());
  CHECK_GE(total_size_, 0);
  CHECK_GE(position, 0);
  CHECK_GE(size, 0);

  // Cap position and size within bounds.
  position = std::min(position, total_size_);
  int64_t clamped_size =
      std::min(static_cast<int64_t>(size), total_size_ - position);

  media_data_source_->ReadBlob(
      position, clamped_size,
      base::Bind(&IPCDataSource::ReadDone, base::Unretained(this), destination,
                 callback));
}

void IPCDataSource::ReadDone(uint8_t* destination,
                             const DataSource::ReadCB& callback,
                             const std::vector<uint8_t>& data) {
  DCHECK(utility_thread_checker_.CalledOnValidThread());

  std::copy(data.begin(), data.end(), destination);
  callback.Run(data.size());
}

}  // namespace metadata
