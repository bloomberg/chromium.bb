// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media_gallery/scoped_mtp_device_map_entry.h"

#include "webkit/fileapi/media/mtp_device_file_system_config.h"

#if defined(SUPPORT_MTP_DEVICE_FILESYSTEM)
#include "base/threading/sequenced_worker_pool.h"
#include "chrome/browser/media_gallery/mtp_device_delegate_impl.h"
#include "content/public/browser/browser_thread.h"
#include "webkit/fileapi/file_system_task_runners.h"
#include "webkit/fileapi/media/mtp_device_map_service.h"
#endif

namespace chrome {

namespace {

#if defined(SUPPORT_MTP_DEVICE_FILESYSTEM)
void AddDelegateToMTPDeviceMapService(
    const FilePath::StringType& device_location,
    fileapi::MTPDeviceDelegate* delegate) {
  fileapi::MTPDeviceMapService::GetInstance()->AddDelegate(device_location,
                                                           delegate);
}
#endif

}  // namespace

ScopedMTPDeviceMapEntry::ScopedMTPDeviceMapEntry(
    const FilePath::StringType& device_location,
    const base::Closure& no_references_callback)
    : device_location_(device_location),
      no_references_callback_(no_references_callback) {
#if defined(SUPPORT_MTP_DEVICE_FILESYSTEM)
  base::SequencedWorkerPool* pool = content::BrowserThread::GetBlockingPool();
  base::SequencedWorkerPool::SequenceToken media_sequence_token =
      pool->GetNamedSequenceToken(fileapi::kMediaTaskRunnerName);
  scoped_refptr<base::SequencedTaskRunner> media_task_runner =
      pool->GetSequencedTaskRunner(media_sequence_token);
  CreateMTPDeviceDelegateCallback callback =
      base::Bind(&AddDelegateToMTPDeviceMapService, device_location_);
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
  no_references_callback_.Run();
#endif
}

}  // namespace chrome
