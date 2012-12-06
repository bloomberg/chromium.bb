// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// MTPOpenStorageWorker implementation.

#include "chrome/browser/media_gallery/linux/mtp_open_storage_worker.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/sequenced_task_runner.h"
#include "base/synchronization/waitable_event.h"
#include "chrome/browser/media_transfer_protocol/media_transfer_protocol_manager.h"
#include "content/public/browser/browser_thread.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chrome {

MTPOpenStorageWorker::MTPOpenStorageWorker(
    const std::string& name,
    base::SequencedTaskRunner* task_runner,
    base::WaitableEvent* task_completed_event,
    base::WaitableEvent* shutdown_event)
    : storage_name_(name),
      media_task_runner_(task_runner),
      on_task_completed_event_(task_completed_event),
      on_shutdown_event_(shutdown_event) {
  DCHECK(on_task_completed_event_);
  DCHECK(on_shutdown_event_);
}

void MTPOpenStorageWorker::Run() {
  DCHECK(media_task_runner_->RunsTasksOnCurrentThread());

  if (on_shutdown_event_->IsSignaled()) {
    // Process is in shutdown mode.
    // Do not post any task on |media_task_runner_|.
    return;
  }

  DCHECK(media_task_runner_->RunsTasksOnCurrentThread());
  content::BrowserThread::PostTask(
      content::BrowserThread::UI, FROM_HERE,
      base::Bind(&MTPOpenStorageWorker::DoWorkOnUIThread, this));
  on_task_completed_event_->Wait();

  if (on_shutdown_event_->IsSignaled())
    cancel_tasks_flag_.Set();
}

MTPOpenStorageWorker::~MTPOpenStorageWorker() {
  DCHECK(media_task_runner_->RunsTasksOnCurrentThread());
}

void MTPOpenStorageWorker::DoWorkOnUIThread() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  if (cancel_tasks_flag_.IsSet())
    return;

  GetMediaTransferProtocolManager()->OpenStorage(
      storage_name_, mtpd::kReadOnlyMode,
      base::Bind(&MTPOpenStorageWorker::OnDidWorkOnUIThread, this));
}

void MTPOpenStorageWorker::OnDidWorkOnUIThread(const std::string& device_handle,
                                               bool error) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  if (cancel_tasks_flag_.IsSet())
    return;

  if (!error)
    device_handle_ = device_handle;
  on_task_completed_event_->Signal();
}

}  // namespace chrome
