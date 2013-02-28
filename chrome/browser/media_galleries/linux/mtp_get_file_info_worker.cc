// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media_galleries/linux/mtp_get_file_info_worker.h"

#include "base/bind.h"
#include "base/sequenced_task_runner.h"
#include "base/synchronization/waitable_event.h"
#include "content/public/browser/browser_thread.h"
#include "device/media_transfer_protocol/media_transfer_protocol_manager.h"

namespace chrome {

using content::BrowserThread;

MTPGetFileInfoWorker::MTPGetFileInfoWorker(
    const std::string& handle,
    const std::string& path,
    base::SequencedTaskRunner* task_runner,
    base::WaitableEvent* task_completed_event,
    base::WaitableEvent* shutdown_event)
    : device_handle_(handle),
      path_(path),
      media_task_runner_(task_runner),
      error_(base::PLATFORM_FILE_OK),
      on_task_completed_event_(task_completed_event),
      on_shutdown_event_(shutdown_event) {
  DCHECK(on_task_completed_event_);
  DCHECK(on_shutdown_event_);
}

void MTPGetFileInfoWorker::Run() {
  DCHECK(media_task_runner_->RunsTasksOnCurrentThread());

  if (on_shutdown_event_->IsSignaled()) {
    // Process is in shutdown mode.
    // Do not post any task on |media_task_runner_|.
    error_ = base::PLATFORM_FILE_ERROR_FAILED;
    return;
  }

  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                          base::Bind(&MTPGetFileInfoWorker::DoWorkOnUIThread,
                                     this));
  on_task_completed_event_->Wait();

  if (on_shutdown_event_->IsSignaled()) {
    error_ = base::PLATFORM_FILE_ERROR_FAILED;
    cancel_tasks_flag_.Set();
  }
}

base::PlatformFileError MTPGetFileInfoWorker::get_file_info(
    base::PlatformFileInfo* file_info) const {
  if (file_info)
    *file_info = file_entry_info_;
  return error_;
}

MTPGetFileInfoWorker::~MTPGetFileInfoWorker() {
  DCHECK(media_task_runner_->RunsTasksOnCurrentThread());
}

void MTPGetFileInfoWorker::DoWorkOnUIThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (cancel_tasks_flag_.IsSet())
    return;

  GetMediaTransferProtocolManager()->GetFileInfoByPath(
      device_handle_, path_,
      base::Bind(&MTPGetFileInfoWorker::OnDidWorkOnUIThread, this));
}

void MTPGetFileInfoWorker::OnDidWorkOnUIThread(const MtpFileEntry& file_entry,
                                               bool error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (cancel_tasks_flag_.IsSet())
    return;

  if (error) {
    error_ = base::PLATFORM_FILE_ERROR_NOT_FOUND;
  } else {
    file_entry_info_.size = file_entry.file_size();
    file_entry_info_.is_directory =
        file_entry.file_type() == MtpFileEntry::FILE_TYPE_FOLDER;
    file_entry_info_.is_symbolic_link = false;
    file_entry_info_.last_modified =
        base::Time::FromTimeT(file_entry.modification_time());
    file_entry_info_.last_accessed =
        base::Time::FromTimeT(file_entry.modification_time());
    file_entry_info_.creation_time = base::Time();
  }
  on_task_completed_event_->Signal();
}

}  // namespace chrome
