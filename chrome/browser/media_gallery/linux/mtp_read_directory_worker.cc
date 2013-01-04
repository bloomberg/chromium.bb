// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media_gallery/linux/mtp_read_directory_worker.h"

#include "base/bind.h"
#include "base/sequenced_task_runner.h"
#include "base/synchronization/waitable_event.h"
#include "content/public/browser/browser_thread.h"
#include "device/media_transfer_protocol/media_transfer_protocol_manager.h"

namespace chrome {

using content::BrowserThread;

MTPReadDirectoryWorker::MTPReadDirectoryWorker(
    const std::string& handle,
    const std::string& path,
    base::SequencedTaskRunner* task_runner,
    base::WaitableEvent* task_completed_event,
    base::WaitableEvent* shutdown_event)
    : device_handle_(handle),
      dir_path_(path),
      dir_entry_id_(0),
      media_task_runner_(task_runner),
      on_task_completed_event_(task_completed_event),
      on_shutdown_event_(shutdown_event) {
  DCHECK(!dir_path_.empty());
  DCHECK(on_task_completed_event_);
  DCHECK(on_shutdown_event_);
}

MTPReadDirectoryWorker::MTPReadDirectoryWorker(
    const std::string& storage_name,
    const uint32_t entry_id,
    base::SequencedTaskRunner* task_runner,
    base::WaitableEvent* task_completed_event,
    base::WaitableEvent* shutdown_event)
    : device_handle_(storage_name),
      dir_entry_id_(entry_id),
      media_task_runner_(task_runner),
      on_task_completed_event_(task_completed_event),
      on_shutdown_event_(shutdown_event) {
  DCHECK(on_task_completed_event_);
  DCHECK(on_shutdown_event_);
}

void MTPReadDirectoryWorker::Run() {
  DCHECK(media_task_runner_->RunsTasksOnCurrentThread());

  if (on_shutdown_event_->IsSignaled()) {
    // Process is in shutdown mode.
    // Do not post any task on |media_task_runner_|.
    return;
  }

  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                          base::Bind(&MTPReadDirectoryWorker::DoWorkOnUIThread,
                                     this));
  on_task_completed_event_->Wait();
  if (on_shutdown_event_->IsSignaled())
    cancel_tasks_flag_.Set();
}

MTPReadDirectoryWorker::~MTPReadDirectoryWorker() {
  DCHECK(media_task_runner_->RunsTasksOnCurrentThread());
}

void MTPReadDirectoryWorker::DoWorkOnUIThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (cancel_tasks_flag_.IsSet())
    return;

  if (!dir_path_.empty()) {
    GetMediaTransferProtocolManager()->ReadDirectoryByPath(
        device_handle_, dir_path_,
        base::Bind(&MTPReadDirectoryWorker::OnDidWorkOnUIThread, this));
  } else {
    GetMediaTransferProtocolManager()->ReadDirectoryById(
        device_handle_, dir_entry_id_,
        base::Bind(&MTPReadDirectoryWorker::OnDidWorkOnUIThread, this));
  }
}

void MTPReadDirectoryWorker::OnDidWorkOnUIThread(
    const std::vector<MtpFileEntry>& file_entries,
    bool error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (cancel_tasks_flag_.IsSet())
    return;

  if (!error)
    file_entries_ = file_entries;
  on_task_completed_event_->Signal();
}

}  // namespace chrome
