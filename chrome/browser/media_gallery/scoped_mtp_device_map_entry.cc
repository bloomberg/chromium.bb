// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media_gallery/scoped_mtp_device_map_entry.h"

#include "webkit/fileapi/media/mtp_device_file_system_config.h"

#if defined(SUPPORT_MTP_DEVICE_FILESYSTEM)
#include "base/threading/sequenced_worker_pool.h"
#include "chrome/browser/media_gallery/mtp_device_delegate_impl.h"
#include "webkit/fileapi/file_system_task_runners.h"
#include "webkit/fileapi/media/mtp_device_map_service.h"
#endif

namespace chrome {

namespace {

#if defined(SUPPORT_MTP_DEVICE_FILESYSTEM)
bool IsMediaTaskRunnerThread() {
  base::SequencedWorkerPool* pool = content::BrowserThread::GetBlockingPool();
  base::SequencedWorkerPool::SequenceToken media_sequence_token =
      pool->GetNamedSequenceToken(fileapi::kMediaTaskRunnerName);
  return pool->IsRunningSequenceOnCurrentThread(media_sequence_token);
}

scoped_refptr<base::SequencedTaskRunner> GetSequencedTaskRunner() {
  base::SequencedWorkerPool* pool = content::BrowserThread::GetBlockingPool();
  base::SequencedWorkerPool::SequenceToken media_sequence_token =
      pool->GetNamedSequenceToken(fileapi::kMediaTaskRunnerName);
  return pool->GetSequencedTaskRunner(media_sequence_token);
}
#endif

}  // namespace

ScopedMTPDeviceMapEntry::ScopedMTPDeviceMapEntry(
    const FilePath::StringType& device_location,
    const base::Closure& on_destruction_callback)
    : device_location_(device_location),
      on_destruction_callback_(on_destruction_callback) {
}

void ScopedMTPDeviceMapEntry::Init() {
#if defined(SUPPORT_MTP_DEVICE_FILESYSTEM)
  CreateMTPDeviceDelegateCallback callback =
      base::Bind(&ScopedMTPDeviceMapEntry::OnMTPDeviceDelegateCreated, this);
  scoped_refptr<base::SequencedTaskRunner> media_task_runner =
      GetSequencedTaskRunner();
  media_task_runner->PostTask(FROM_HERE,
                              base::Bind(&CreateMTPDeviceDelegate,
                                         device_location_,
                                         media_task_runner,
                                         callback));
#endif
}

ScopedMTPDeviceMapEntry::~ScopedMTPDeviceMapEntry() {
#if defined(SUPPORT_MTP_DEVICE_FILESYSTEM)
  fileapi::MTPDeviceMapService::GetInstance()->RemoveDelegate(device_location_);
  on_destruction_callback_.Run();
#endif
}

void ScopedMTPDeviceMapEntry::OnMTPDeviceDelegateCreated(
    fileapi::MTPDeviceDelegate* delegate) {
#if defined(SUPPORT_MTP_DEVICE_FILESYSTEM)
  DCHECK(IsMediaTaskRunnerThread());
  fileapi::MTPDeviceMapService::GetInstance()->AddDelegate(device_location_,
                                                           delegate);
#endif
}

}  // namespace chrome
