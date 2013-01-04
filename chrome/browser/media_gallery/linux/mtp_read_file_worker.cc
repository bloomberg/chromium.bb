// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media_gallery/linux/mtp_read_file_worker.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "base/bind.h"
#include "base/file_util.h"
#include "base/sequenced_task_runner.h"
#include "base/synchronization/waitable_event.h"
#include "chrome/browser/media_transfer_protocol/media_transfer_protocol_manager.h"
#include "chrome/browser/media_transfer_protocol/mtp_file_entry.pb.h"
#include "content/public/browser/browser_thread.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

using content::BrowserThread;

namespace chrome {

MTPReadFileWorker::MTPReadFileWorker(const std::string& handle,
                                     const std::string& src_path,
                                     uint32 total_size,
                                     const FilePath& dest_path,
                                     base::SequencedTaskRunner* task_runner,
                                     base::WaitableEvent* task_completed_event,
                                     base::WaitableEvent* shutdown_event)
    : device_handle_(handle),
      src_path_(src_path),
      total_bytes_(total_size),
      dest_path_(dest_path),
      bytes_read_(0),
      error_occurred_(false),
      media_task_runner_(task_runner),
      on_task_completed_event_(task_completed_event),
      on_shutdown_event_(shutdown_event) {
  DCHECK(on_task_completed_event_);
  DCHECK(on_shutdown_event_);
}

void MTPReadFileWorker::Run() {
  DCHECK(media_task_runner_->RunsTasksOnCurrentThread());

  if (on_shutdown_event_->IsSignaled()) {
    // Process is in shutdown mode.
    // Do not post any task on |media_task_runner_|.
    return;
  }

  int dest_fd = open(dest_path_.value().c_str(), O_WRONLY);
  if (dest_fd < 0)
    return;
  file_util::ScopedFD dest_fd_scoper(&dest_fd);

  while (bytes_read_ < total_bytes_) {
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                            base::Bind(&MTPReadFileWorker::DoWorkOnUIThread,
                                       this));
    on_task_completed_event_->Wait();
    if (error_occurred_)
      break;
    if (on_shutdown_event_->IsSignaled()) {
      cancel_tasks_flag_.Set();
      break;
    }

    int bytes_written =
        file_util::WriteFileDescriptor(dest_fd, data_.data(), data_.size());
    if (static_cast<int>(data_.size()) != bytes_written)
      break;

    bytes_read_ += data_.size();
  }
}

bool MTPReadFileWorker::Succeeded() const {
  return (!error_occurred_ && (bytes_read_ == total_bytes_));
}

MTPReadFileWorker::~MTPReadFileWorker() {
  DCHECK(media_task_runner_->RunsTasksOnCurrentThread());
}

void MTPReadFileWorker::DoWorkOnUIThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (cancel_tasks_flag_.IsSet())
    return;

  GetMediaTransferProtocolManager()->ReadFileChunkByPath(
      device_handle_, src_path_, bytes_read_, BytesToRead(),
      base::Bind(&MTPReadFileWorker::OnDidWorkOnUIThread, this));
}

void MTPReadFileWorker::OnDidWorkOnUIThread(const std::string& data,
                                            bool error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (cancel_tasks_flag_.IsSet())
    return;
  error_occurred_ = error || (data.size() != BytesToRead());
  if (!error_occurred_)
    data_ = data;
  on_task_completed_event_->Signal();
}

uint32 MTPReadFileWorker::BytesToRead() const {
  // Read data in 1 MB chunks.
  static const uint32 kReadChunkSize = 1024 * 1024;
  return std::min(kReadChunkSize, total_bytes_ - bytes_read_);
}

}  // namespace chrome
